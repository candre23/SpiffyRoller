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


static void push_options(
    lua_State *state,
    const dice_rule_definition_t *definition,
    const dice_rule_state_t *rule_state)
{
    lua_newtable(state);

    if (definition == NULL || rule_state == NULL) {
        return;
    }

    for (size_t index = 0;
         index < definition->control_count;
         ++index) {
        lua_pushinteger(
            state,
            rule_state->values[index]);
        lua_setfield(
            state,
            -2,
            definition->controls[index].id);
    }
}

static lua_State *load_rules_vm(
    const dice_rule_definition_t *definition,
    char **script_out)
{
    if (definition == NULL ||
        !definition->loaded ||
        script_out == NULL) {
        return NULL;
    }

    *script_out = read_file(definition->script_path);
    if (*script_out == NULL) {
        return NULL;
    }

    lua_memory_t *memory = calloc(1, sizeof(*memory));
    if (memory == NULL) {
        free(*script_out);
        *script_out = NULL;
        return NULL;
    }

    memory->limit = definition->memory_limit_bytes;

    lua_State *lua = lua_newstate(
        limited_alloc,
        memory,
        0U);

    if (lua == NULL) {
        free(memory);
        free(*script_out);
        *script_out = NULL;
        return NULL;
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
        *script_out,
        strlen(*script_out),
        definition->script_path);

    if (status == LUA_OK) {
        status = lua_pcall(lua, 0, 0, 0);
    }

    if (status != LUA_OK) {
        ESP_LOGE(TAG, "%s", lua_tostring(lua, -1));
        lua_close(lua);
        free(memory);
        free(*script_out);
        *script_out = NULL;
        return NULL;
    }

    /*
     * The Lua allocator user-data must remain valid until lua_close().
     * Store it in the registry so the helper that closes the VM can recover it.
     */
    lua_pushlightuserdata(lua, memory);
    lua_setfield(lua, LUA_REGISTRYINDEX, "spiffy_memory_state");

    return lua;
}

static void close_rules_vm(
    lua_State *lua,
    char *script)
{
    if (lua != NULL) {
        lua_getfield(lua, LUA_REGISTRYINDEX, "spiffy_memory_state");
        lua_memory_t *memory = lua_touserdata(lua, -1);
        lua_pop(lua, 1);
        lua_close(lua);
        free(memory);
    }

    free(script);
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
    const cJSON *actions =
        cJSON_GetObjectItemCaseSensitive(root, "actions");

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


        if (cJSON_IsArray(actions)) {
            const cJSON *action = NULL;

            cJSON_ArrayForEach(action, actions) {
                if (definition->action_count >=
                    DICE_RULE_MAX_ACTIONS) {
                    break;
                }

                if (!cJSON_IsObject(action)) {
                    continue;
                }

                const cJSON *id =
                    cJSON_GetObjectItemCaseSensitive(
                        action,
                        "id");
                const cJSON *label =
                    cJSON_GetObjectItemCaseSensitive(
                        action,
                        "label");
                const cJSON *available =
                    cJSON_GetObjectItemCaseSensitive(
                        action,
                        "available");
                const cJSON *apply =
                    cJSON_GetObjectItemCaseSensitive(
                        action,
                        "apply");

                if (!cJSON_IsString(id) ||
                    !cJSON_IsString(label) ||
                    !cJSON_IsString(apply)) {
                    continue;
                }

                dice_rule_action_t *output =
                    &definition->actions[
                        definition->action_count];

                snprintf(
                    output->id,
                    sizeof(output->id),
                    "%s",
                    id->valuestring);
                snprintf(
                    output->label,
                    sizeof(output->label),
                    "%s",
                    label->valuestring);
                snprintf(
                    output->apply_function,
                    sizeof(output->apply_function),
                    "%s",
                    apply->valuestring);

                if (cJSON_IsString(available)) {
                    snprintf(
                        output->available_function,
                        sizeof(output->available_function),
                        "%s",
                        available->valuestring);
                }

                ++definition->action_count;
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
    push_options(lua, definition, state);

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
    if (definition == NULL ||
        state == NULL ||
        roll == NULL ||
        !definition->loaded ||
        index >= definition->action_count ||
        state->action_used[index]) {
        return false;
    }

    const dice_rule_action_t *action =
        &definition->actions[index];

    if (action->available_function[0] == ' ') {
        return true;
    }

    char *script = NULL;
    lua_State *lua =
        load_rules_vm(definition, &script);

    if (lua == NULL) {
        return false;
    }

    lua_getglobal(lua, action->available_function);
    if (!lua_isfunction(lua, -1)) {
        close_rules_vm(lua, script);
        return false;
    }

    push_roll(lua, roll);
    push_options(lua, definition, state);

    int status = lua_pcall(lua, 2, 1, 0);
    bool available =
        status == LUA_OK &&
        lua_toboolean(lua, -1);

    if (status != LUA_OK) {
        ESP_LOGE(TAG, "%s", lua_tostring(lua, -1));
    }

    close_rules_vm(lua, script);
    return available;
}

bool dice_rules_apply_action(
    const dice_rule_definition_t *definition,
    dice_rule_state_t *state,
    size_t index,
    dice_custom_roll_result_t *roll,
    bool *mask,
    size_t mask_size)
{
    if (definition == NULL ||
        state == NULL ||
        roll == NULL ||
        mask == NULL ||
        !definition->loaded ||
        index >= definition->action_count ||
        state->action_used[index]) {
        return false;
    }

    memset(mask, 0, mask_size * sizeof(*mask));

    const dice_rule_action_t *action =
        &definition->actions[index];

    if (action->apply_function[0] == ' ') {
        return false;
    }

    char *script = NULL;
    lua_State *lua =
        load_rules_vm(definition, &script);

    if (lua == NULL) {
        return false;
    }

    lua_getglobal(lua, action->apply_function);
    if (!lua_isfunction(lua, -1)) {
        close_rules_vm(lua, script);
        return false;
    }

    push_roll(lua, roll);
    push_options(lua, definition, state);

    int status = lua_pcall(lua, 2, 1, 0);
    if (status != LUA_OK) {
        ESP_LOGE(TAG, "%s", lua_tostring(lua, -1));
        close_rules_vm(lua, script);
        return false;
    }

    if (!lua_istable(lua, -1)) {
        close_rules_vm(lua, script);
        return false;
    }

    lua_getfield(lua, -1, "reroll");
    if (!lua_istable(lua, -1)) {
        close_rules_vm(lua, script);
        return false;
    }

    bool changed = false;
    lua_Integer count = luaL_len(lua, -1);

    for (lua_Integer item = 1;
         item <= count;
         ++item) {
        lua_rawgeti(lua, -1, item);

        if (lua_isinteger(lua, -1)) {
            lua_Integer requested =
                lua_tointeger(lua, -1);

            if (requested >= 1 &&
                (size_t)requested <= roll->count) {
                size_t die_index =
                    (size_t)requested - 1U;

                if (die_index < mask_size &&
                    !mask[die_index]) {
                    dice_custom_result_die_t *result =
                        &roll->dice[die_index];

                    if (result->die != NULL) {
                        if (result->face != NULL &&
                            result->face->has_numeric_value) {
                            roll->numeric_total -=
                                result->face->numeric_value;
                        }

                        size_t face_index = 0;
                        const dice_set_face_t *face =
                            dice_custom_random_face(
                                result->die,
                                &face_index);

                        if (face != NULL) {
                            result->face = face;
                            result->face_index = face_index;

                            if (face->has_numeric_value) {
                                roll->numeric_total +=
                                    face->numeric_value;
                            }

                            mask[die_index] = true;
                            changed = true;
                        }
                    }
                }
            }
        }

        lua_pop(lua, 1);
    }

    close_rules_vm(lua, script);

    if (changed) {
        state->action_used[index] = true;
    }

    return changed;
}
