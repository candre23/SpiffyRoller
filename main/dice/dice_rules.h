#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "dice_custom_runtime.h"
#include "dice_set_catalog.h"

#define DICE_RULE_MAX_CONTROLS 8
#define DICE_RULE_MAX_ACTIONS 4
#define DICE_RULE_ID_MAX 31
#define DICE_RULE_LABEL_MAX 31
#define DICE_RULE_SUFFIX_MAX 7
#define DICE_RULE_SCRIPT_PATH_MAX 255

typedef enum { DICE_RULE_CONTROL_TOGGLE = 0, DICE_RULE_CONTROL_CHOICE, DICE_RULE_CONTROL_INTEGER } dice_rule_control_type_t;
typedef struct { char id[32]; char label[32]; char value_suffix[8]; dice_rule_control_type_t type; bool display_main; int default_value; int minimum; int maximum; int step; int choices[8]; size_t choice_count; } dice_rule_control_t;
typedef struct { char id[32]; char label[32]; char available_function[32]; char apply_function[32]; } dice_rule_action_t;
typedef struct {
    bool loaded;
    bool lua;
    char script_path[DICE_RULE_SCRIPT_PATH_MAX + 1];
    char entry[DICE_RULE_ID_MAX + 1];
    uint32_t instruction_limit;
    uint32_t memory_limit_bytes;
    dice_rule_control_t controls[DICE_RULE_MAX_CONTROLS];
    size_t control_count;
    dice_rule_action_t actions[DICE_RULE_MAX_ACTIONS];
    size_t action_count;
    size_t result_count;
} dice_rule_definition_t;
typedef struct { int values[DICE_RULE_MAX_CONTROLS]; bool action_used[DICE_RULE_MAX_ACTIONS]; } dice_rule_state_t;

bool dice_rules_load(const dice_set_definition_t *, dice_rule_definition_t *);
void dice_rules_state_defaults(const dice_rule_definition_t *, dice_rule_state_t *);
void dice_rules_cycle_control(const dice_rule_definition_t *, dice_rule_state_t *, size_t);
void dice_rules_adjust_control(const dice_rule_definition_t *, dice_rule_state_t *, size_t, int);
void dice_rules_format_control_value(const dice_rule_definition_t *, const dice_rule_state_t *, size_t, char *, size_t);
void dice_rules_evaluate(const dice_rule_definition_t *, const dice_rule_state_t *, const dice_custom_roll_result_t *, char *, size_t, char *, size_t);
bool dice_rules_action_available(const dice_rule_definition_t *, const dice_rule_state_t *, size_t, const dice_custom_roll_result_t *);
bool dice_rules_apply_action(const dice_rule_definition_t *, dice_rule_state_t *, size_t, dice_custom_roll_result_t *, bool *, size_t);
