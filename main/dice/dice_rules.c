#include "dice_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#define MAX_RULE_BYTES 65536
#define LUA_HOOK_GRANULARITY 100
#define LUA_DEFAULT_INSTRUCTION_LIMIT 50000
#define LUA_DEFAULT_MEMORY_LIMIT_BYTES (96U * 1024U)

static const char *TAG = "lua_rules";

typedef struct {
    size_t used;
    size_t limit;
} lua_memory_t;

static uint32_t s_hook_count;
static uint32_t s_hook_limit;

static void *limited_alloc(
    void *user_data,
    void *pointer,
    size_t old_size,
    size_t new_size)
{
    lua_memory_t *memory = user_data;

    if (new_size == 0) {
        if (pointer != NULL) {
            memory->used =
                memory->used >= old_size
                    ? memory->used - old_size
                    : 0;
            free(pointer);
        }
        return NULL;
    }

    if (new_size > old_size) {
        size_t increase = new_size - old_size;
        if (memory->used + increase > memory->limit) {
            return NULL;
        }
    }

    void *next = realloc(pointer, new_size);
    if (next != NULL) {
        memory->used = memory->used - old_size + new_size;
    }

    return next;
}

static void instruction_hook(lua_State *state, lua_Debug *debug)
{
    (void)debug;

    s_hook_count += LUA_HOOK_GRANULARITY;
    if (s_hook_count > s_hook_limit) {
        luaL_error(state, "instruction limit exceeded");
    }
}


static bool join_path(
    char *destination,
    size_t destination_size,
    const char *folder,
    const char *name)
{
    if (destination == NULL || destination_size == 0 ||
        folder == NULL || name == NULL) {
        return false;
    }

    const size_t folder_length = strlen(folder);
    const size_t name_length = strlen(name);
    const bool needs_separator =
        folder_length > 0 && folder[folder_length - 1] != '/';
    const size_t separator_length = needs_separator ? 1U : 0U;

    if (folder_length >= destination_size ||
        name_length >= destination_size ||
        folder_length + separator_length + name_length >= destination_size) {
        destination[0] = '\0';
        return false;
    }

    memcpy(destination, folder, folder_length);
    size_t position = folder_length;

    if (needs_separator) {
        destination[position++] = '/';
    }

    memcpy(destination + position, name, name_length);
    destination[position + name_length] = '\0';
    return true;
}

static char *read_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long length = ftell(file);
    if (length <= 0 || length > MAX_RULE_BYTES) {
        fclose(file);
        return NULL;
    }

    rewind(file);

    char *buffer = malloc((size_t)length + 1U);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, (size_t)length, file) != (size_t)length) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    fclose(file);
    buffer[length] = '\0';
    return buffer;
}

static void remove_global(lua_State *state, const char *name)
{
    lua_pushnil(state);
    lua_setglobal(state, name);
}

static void sandbox(lua_State *state)
{
    luaL_openlibs(state);

    remove_global(state, "io");
    remove_global(state, "os");
    remove_global(state, "package");
    remove_global(state, "debug");
    remove_global(state, "dofile");
    remove_global(state, "loadfile");
    remove_global(state, "require");
    remove_global(state, "load");
    remove_global(state, "collectgarbage");
}

static void push_symbol(lua_State *state, const char *name, int value)
{
    lua_pushinteger(state, value);
    lua_setfield(state, -2, name);
}

static void push_symbols(lua_State *state, const dice_set_face_t *face)
{
    lua_newtable(state);
    push_symbol(state, "success", face->success_count);
    push_symbol(state, "advantage", face->advantage_count);
    push_symbol(state, "triumph", face->triumph_count);
    push_symbol(state, "failure", face->failure_count);
    push_symbol(state, "threat", face->threat_count);
    push_symbol(state, "despair", face->despair_count);
}

static void push_roll(
    lua_State *state,
    const dice_custom_roll_result_t *roll)
{
    lua_newtable(state);
    lua_newtable(state);

    for (size_t index = 0; index < roll->count; ++index) {
        const dice_custom_result_die_t *result = &roll->dice[index];

        lua_newtable(state);

        lua_pushstring(
            state,
            result->die != NULL ? result->die->id : "");
        lua_setfield(state, -2, "die_id");

        lua_pushstring(
            state,
            result->die != NULL ? result->die->name : "");
        lua_setfield(state, -2, "die_name");

        lua_pushinteger(
            state,
            (lua_Integer)result->face_index + 1);
        lua_setfield(state, -2, "face_index");

        lua_pushstring(
            state,
            result->face != NULL ? result->face->label : "");
        lua_setfield(state, -2, "label");

        if (result->face != NULL &&
            result->face->has_numeric_value) {
            lua_pushinteger(
                state,
                result->face->numeric_value);
            lua_setfield(state, -2, "value");
        }

        if (result->face != NULL) {
            push_symbols(state, result->face);
            lua_setfield(state, -2, "symbols");
        }

        lua_rawseti(
            state,
            -2,
            (lua_Integer)index + 1);
    }

    lua_setfield(state, -2, "dice");

    lua_pushinteger(state, roll->numeric_total);
    lua_setfield(state, -2, "numeric_total");
}

bool dice_rules_load(
    const dice_set_definition_t *set,
    dice_rule_definition_t *definition)
{
    if (set == NULL ||
        definition == NULL ||
        set->rules_path[0] == '\0') {
        return false;
    }

    memset(definition, 0, sizeof(*definition));

    char path[300];
    snprintf(
        path,
        sizeof(path),
        "%s/%s",
        set->folder_path,
        set->rules_path);

    char *text = read_file(path);
    if (text == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(text);
    free(text);

    if (root == NULL) {
        return false;
    }

    const cJSON *format =
        cJSON_GetObjectItemCaseSensitive(root, "format");
    const cJSON *version =
        cJSON_GetObjectItemCaseSensitive(root, "format_version");
    const cJSON *engine =
        cJSON_GetObjectItemCaseSensitive(root, "engine");
    const cJSON *script =
        cJSON_GetObjectItemCaseSensitive(root, "script");
    const cJSON *entry =
        cJSON_GetObjectItemCaseSensitive(root, "entry");
    const cJSON *limits =
        cJSON_GetObjectItemCaseSensitive(root, "limits");

    bool valid =
        cJSON_IsString(format) &&
        strcmp(format->valuestring, "spiffy-roller-rules") == 0 &&
        cJSON_IsNumber(version) &&
        version->valueint == 2 &&
        cJSON_IsString(engine) &&
        strcmp(engine->valuestring, "lua") == 0 &&
        cJSON_IsString(script) &&
        cJSON_IsString(entry);

    if (valid) {
        if (!join_path(
                definition->script_path,
                sizeof(definition->script_path),
                set->folder_path,
                script->valuestring)) {
            valid = false;
        }

        snprintf(
            definition->entry,
            sizeof(definition->entry),
            "%s",
            entry->valuestring);

        definition->instruction_limit =
            LUA_DEFAULT_INSTRUCTION_LIMIT;
        definition->memory_limit_bytes =
            LUA_DEFAULT_MEMORY_LIMIT_BYTES;

        if (cJSON_IsObject(limits)) {
            const cJSON *instructions =
                cJSON_GetObjectItemCaseSensitive(
                    limits,
                    "instructions");
            const cJSON *memory_kb =
                cJSON_GetObjectItemCaseSensitive(
                    limits,
                    "memory_kb");

            if (cJSON_IsNumber(instructions) &&
                instructions->valueint > 0) {
                definition->instruction_limit =
                    (uint32_t)instructions->valueint;
            }

            if (cJSON_IsNumber(memory_kb) &&
                memory_kb->valueint > 0) {
                definition->memory_limit_bytes =
                    (size_t)memory_kb->valueint * 1024U;
            }
        }

        FILE *script_file =
            fopen(definition->script_path, "rb");
        if (script_file == NULL) {
            valid = false;
        } else {
            fclose(script_file);
        }
    }

    cJSON_Delete(root);

    definition->loaded = valid;
    definition->lua = valid;
    definition->result_count = valid ? 1 : 0;
    return valid;
}

void dice_rules_state_defaults(
    const dice_rule_definition_t *definition,
    dice_rule_state_t *state)
{
    (void)definition;
    memset(state, 0, sizeof(*state));
}

void dice_rules_cycle_control(
    const dice_rule_definition_t *definition,
    dice_rule_state_t *state,
    size_t index)
{
    (void)definition;
    (void)state;
    (void)index;
}

void dice_rules_adjust_control(
    const dice_rule_definition_t *definition,
    dice_rule_state_t *state,
    size_t index,
    int quantity)
{
    (void)definition;
    (void)state;
    (void)index;
    (void)quantity;
}

void dice_rules_format_control_value(
    const dice_rule_definition_t *definition,
    const dice_rule_state_t *state,
    size_t index,
    char *buffer,
    size_t buffer_size)
{
    (void)definition;
    (void)state;
    (void)index;

    if (buffer_size > 0) {
        buffer[0] = '\0';
    }
}

void dice_rules_evaluate(
    const dice_rule_definition_t *definition,
    const dice_rule_state_t *state,
    const dice_custom_roll_result_t *roll,
    char *title,
    size_t title_size,
    char *details,
    size_t details_size)
{
    (void)state;

    if (title_size > 0) {
        snprintf(
            title,
            title_size,
            "Total: %ld",
            (long)roll->numeric_total);
    }

    if (details_size > 0) {
        details[0] = '\0';
    }

    if (definition == NULL || !definition->loaded) {
        return;
    }

    char *script = read_file(definition->script_path);
    if (script == NULL) {
        snprintf(title, title_size, "Rules file error");
        return;
    }

    lua_memory_t memory = {
        .used = 0,
        .limit = definition->memory_limit_bytes,
    };

    lua_State *lua = lua_newstate(
        limited_alloc,
        &memory,
        0U);

    if (lua == NULL) {
        free(script);
        snprintf(title, title_size, "Rules memory error");
        return;
    }

    sandbox(lua);

    s_hook_count = 0;
    s_hook_limit = definition->instruction_limit;
    lua_sethook(
        lua,
        instruction_hook,
        LUA_MASKCOUNT,
        LUA_HOOK_GRANULARITY);

    int status = luaL_loadbuffer(
        lua,
        script,
        strlen(script),
        definition->script_path);

    if (status == LUA_OK) {
        status = lua_pcall(lua, 0, 0, 0);
    }

    if (status != LUA_OK) {
        ESP_LOGE(TAG, "%s", lua_tostring(lua, -1));
        snprintf(title, title_size, "Rules script error");
        lua_close(lua);
        free(script);
        return;
    }

    free(script);

    lua_getglobal(lua, definition->entry);
    if (!lua_isfunction(lua, -1)) {
        snprintf(title, title_size, "Missing resolve()");
        lua_close(lua);
        return;
    }

    push_roll(lua, roll);
    lua_newtable(lua);

    status = lua_pcall(lua, 2, 1, 0);
    if (status != LUA_OK) {
        ESP_LOGE(TAG, "%s", lua_tostring(lua, -1));
        snprintf(title, title_size, "Rules runtime error");
        lua_close(lua);
        return;
    }

    if (lua_istable(lua, -1)) {
        lua_getfield(lua, -1, "title");
        if (lua_isstring(lua, -1)) {
            snprintf(
                title,
                title_size,
                "%s",
                lua_tostring(lua, -1));
        }
        lua_pop(lua, 1);

        lua_getfield(lua, -1, "lines");
        if (lua_istable(lua, -1)) {
            size_t used = 0;
            lua_Integer count = luaL_len(lua, -1);

            for (lua_Integer index = 1;
                 index <= count;
                 ++index) {
                lua_rawgeti(lua, -1, index);

                if (lua_isstring(lua, -1) &&
                    used < details_size) {
                    const char *line =
                        lua_tostring(lua, -1);
                    int written = snprintf(
                        details + used,
                        details_size - used,
                        "%s%s",
                        used > 0 ? "\n" : "",
                        line);

                    if (written > 0) {
                        size_t amount = (size_t)written;
                        used =
                            amount < details_size - used
                                ? used + amount
                                : details_size - 1;
                    }
                }

                lua_pop(lua, 1);
            }
        }
        lua_pop(lua, 1);
    }

    lua_close(lua);
}

bool dice_rules_action_available(
    const dice_rule_definition_t *definition,
    const dice_rule_state_t *state,
    size_t index,
    const dice_custom_roll_result_t *roll)
{
    (void)definition;
    (void)state;
    (void)index;
    (void)roll;
    return false;
}

bool dice_rules_apply_action(
    const dice_rule_definition_t *definition,
    dice_rule_state_t *state,
    size_t index,
    dice_custom_roll_result_t *roll,
    bool *mask,
    size_t mask_size)
{
    (void)definition;
    (void)state;
    (void)index;
    (void)roll;
    (void)mask;
    (void)mask_size;
    return false;
}
