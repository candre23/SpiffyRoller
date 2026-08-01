# Spiffy Roller Custom Dice Set Authoring Guide

**Applies to:** Spiffy Roller v1.0  
**Dice set manifest format:** `universal-dice-set`, version 1  
**Rules format:** `spiffy-roller-rules`, version 2

Spiffy Roller can load user-created dice systems from `.set` files placed in the device's `templates` folder. A custom set can define:

- one or more custom die types;
- 2 to 64 faces per die;
- standard numeric faces;
- text or symbol labels;
- custom PNG or BMP face artwork;
- per-die and per-face colors;
- numeric values used for automatic totals;
- Genesys-style symbolic values exposed to rules scripts;
- custom result summaries written in Lua; and
- post-roll actions such as rerolling selected dice.

A `.set` file is simply a ZIP archive renamed with the `.set` extension.

---

## 1. Quick start

The smallest useful custom set needs only a `set.json` file.

Create this folder structure:

```text
my_example/
└── set.json
```

Put the following in `set.json`:

```json
{
  "format": "universal-dice-set",
  "format_version": 1,
  "id": "my_example",
  "name": "My Example",
  "dice": [
    {
      "id": "d6",
      "name": "Example D6",
      "sides": 6,
      "shape": "d6",
      "body_color": "#245A9A",
      "ink_color": "#FFFFFF",
      "faces": [
        { "value": 1 },
        { "value": 2 },
        { "value": 3 },
        { "value": 4 },
        { "value": 5 },
        { "value": 6 }
      ]
    }
  ]
}
```

ZIP the **`my_example` folder itself**, then rename the archive from:

```text
my_example.zip
```

to:

```text
my_example.set
```

Copy `my_example.set` into:

```text
/templates/
```

on Spiffy Roller's USB storage, exit transfer mode/restart the roller, then select the set from **Menu > Dice Set**.

For the example above, Spiffy Roller automatically displays each face's numeric `value` and totals all rolled values.

---

# 2. `.set` package structure

A typical full set looks like this:

```text
shadowrun_example/
├── set.json
├── rules.json
├── rules.lua
└── icons/
    ├── hit.png
    └── glitch.png
```

That directory is compressed into a ZIP and renamed:

```text
shadowrun_example.set
```

## Important: keep one top-level folder

The safest and intended archive layout is:

```text
set_name/
    set.json
    rules.json
    rules.lua
    icons/...
```

Spiffy Roller's archive extractor removes the first directory component when unpacking a `.set` file. A single top-level folder therefore keeps all internal relative paths intact.

Do **not** create an archive like this if you use subdirectories:

```text
set.json
icons/icon.png
```

With that layout, the archive extractor can strip `icons/` from the image entry and break the path referenced by `set.json`.

## Supported ZIP compression

The current firmware accepts normal ZIP entries using:

- Stored/no compression, ZIP method 0
- Deflate compression, ZIP method 8

Archives that rely on ZIP data descriptors are not supported by the current extractor. Ordinary ZIP files created by common desktop ZIP tools generally work, but if a set is not discovered, recreate it as a conventional ZIP archive.

## File and path safety

Internal paths must be relative. Do not use:

- absolute paths beginning with `/` or `\`;
- drive-letter paths such as `C:`;
- `..` parent-directory traversal.

Keep filenames and folder names simple. Lowercase letters, numbers, underscores, and hyphens are recommended.

---

# 3. `set.json`: the set manifest

Every custom set requires `set.json`.

Top-level example:

```json
{
  "format": "universal-dice-set",
  "format_version": 1,
  "id": "example_system",
  "name": "Example System",
  "rules": "rules.json",
  "dice": [
    ...
  ]
}
```

## Required top-level fields

| Field | Type | Requirements | Purpose |
|---|---|---|---|
| `format` | string | Must be `"universal-dice-set"` | Identifies the file format |
| `format_version` | integer | Must be `1` | Manifest format version |
| `id` | string | 1-31 chars; letters, numbers, `_`, `-` only | Stable internal set identifier |
| `name` | string | Up to 47 chars | Name shown in Spiffy Roller |
| `dice` | array | 1-16 die definitions | Dice available in the selector |

## Optional top-level field

| Field | Type | Purpose |
|---|---|---|
| `rules` | string | Relative path to a rules definition such as `rules.json` |

If `rules` is omitted, Spiffy Roller simply uses the sum of every face's numeric `value` as the result title:

```text
Total: 17
```

## Set IDs must be unique

The built-in dice set uses:

```text
standard
```

Every custom set should have its own unique `id`. Duplicate IDs are rejected during catalog loading.

Good IDs:

```text
fate_core
shadowrun_6e
my-homebrew
narrative_dice
```

Invalid IDs include spaces or punctuation:

```text
My Dice
shadowrun:6e
my/set
```

The display `name` can contain spaces and is independent of the ID.

---

# 4. Defining a die

Each item in the `dice` array has this structure:

```json
{
  "id": "attack",
  "name": "Attack Die",
  "sides": 6,
  "shape": "d6",
  "body_color": "#A02020",
  "ink_color": "#FFFFFF",
  "faces": [
    ...
  ]
}
```

All seven fields are required.

| Field | Type | Requirements |
|---|---|---|
| `id` | string | 1-31 chars; letters, numbers, `_`, `-` only; unique within the set |
| `name` | string | Up to 47 chars |
| `sides` | integer | 2-64 |
| `shape` | string | Up to 15 chars; see supported shapes below |
| `body_color` | string | `#RRGGBB` |
| `ink_color` | string | `#RRGGBB` |
| `faces` | array | Must contain exactly the number of entries specified by `sides` |

The order of dice in the `dice` array is also their order in the die-type selector.

## Supported visual shapes

The `shape` field controls how the die is drawn. It does **not** control how many faces the randomizer uses. `sides` and the number of entries in `faces` control probability.

Supported values are:

| `shape` value | Rendered shape |
|---|---|
| `d4` | Up-pointing triangle |
| `triangle_up` | Up-pointing triangle |
| `d6` | Square |
| any unrecognized string | Square |
| `d8` | Down-pointing triangle |
| `triangle_down` | Down-pointing triangle |
| `d10` | Diamond/rhombus |
| `diamond` | Diamond/rhombus |
| `d12` | Pentagon |
| `pentagon` | Pentagon |
| `d20` | Hexagon |
| `hexagon` | Hexagon |

This lets you create nonstandard dice while choosing whichever visual body is most useful. For example, a 12-face random table could still be drawn as a square:

```json
{
  "sides": 12,
  "shape": "d6",
  ...
}
```

Every face remains equally likely because the roller selects one entry uniformly from the 12-element `faces` array.

---

# 5. Colors

Colors use six-digit hexadecimal RGB notation:

```json
"body_color": "#183A62",
"ink_color": "#FFFFFF"
```

The format must be exactly:

```text
#RRGGBB
```

Examples:

```text
#000000
#FFFFFF
#18A85B
#FFD84D
```

Each die supplies default body and ink colors. An individual face can override either or both.

Example:

```json
{
  "value": 1,
  "body_color": "#8A1520",
  "ink_color": "#FFFFFF"
}
```

A face without overrides inherits the die defaults.

---

# 6. Defining faces

A face can contain several independent kinds of information:

- `value`: numeric value for totals and Lua rules;
- `label`: text available for display and Lua rules;
- `image`: custom artwork;
- `image_mode`: how artwork is interpreted;
- `display_mode`: what is visibly drawn on the die;
- `body_color` / `ink_color`: per-face color overrides;
- `symbols`: built-in narrative-symbol metadata available to Lua rules.

A simple numeric face:

```json
{
  "value": 5
}
```

A labeled face:

```json
{
  "label": "HIT"
}
```

A face can have both:

```json
{
  "value": 1,
  "label": "GLITCH",
  "display_mode": "label"
}
```

Here the visible face says `GLITCH`, while the Lua rules engine can still read the numeric value `1`.

## Face order and probability

Each entry in `faces` represents one equally probable physical face.

For a six-sided die:

```json
"sides": 6,
"faces": [
  { "label": "MISS" },
  { "label": "MISS" },
  { "label": "MISS" },
  { "label": "HIT" },
  { "label": "HIT" },
  { "label": "CRIT" }
]
```

produces:

- MISS: 3/6 chance
- HIT: 2/6 chance
- CRIT: 1/6 chance

You do not need unique values or labels for every face.

---

# 7. `value`

`value` is an integer:

```json
{ "value": 6 }
```

Negative values and zero are also accepted:

```json
{ "value": -1 }
{ "value": 0 }
```

If a face has a numeric value:

1. the value contributes to the roll's `numeric_total`;
2. it is exposed to Lua as `die.value`;
3. it can be displayed with `"display_mode": "value"`.

If no custom rules are installed, `numeric_total` becomes the standard custom-set result.

### Important distinction

`value` is machine-readable game data. `label` is text. If you want rules to perform numeric comparisons, give the face a numeric `value` even if you display a word or image instead.

---

# 8. `label`

`label` may contain up to 31 characters.

```json
{
  "label": "SUCCESS"
}
```

The label is:

- available to the Lua rules engine as `die.label`;
- displayed if `display_mode` is `label`; and
- used automatically as the visible face if no explicit `display_mode` is supplied and no image takes priority.

Short labels work best on the small die face. Long labels may fit the file format but will not necessarily fit cleanly on the physical display.

---

# 9. `display_mode`

`display_mode` determines what Spiffy Roller actually draws on the face.

Supported values:

```text
label
value
image
blank
```

## `label`

Displays the face's `label`:

```json
{
  "value": 5,
  "label": "HIT",
  "display_mode": "label"
}
```

## `value`

Displays the numeric `value`:

```json
{
  "value": 5,
  "label": "HIT",
  "display_mode": "value"
}
```

The screen displays `5`, but Lua can still access both `value` and `label`.

## `image`

Displays the referenced image:

```json
{
  "value": 1,
  "label": "Failure",
  "image": "icons/failure.png",
  "image_mode": "mask",
  "display_mode": "image"
}
```

## `blank`

Draws no face text or artwork:

```json
{
  "value": 0,
  "display_mode": "blank"
}
```

The value still exists for rules and numeric totals.

## Automatic display selection

If `display_mode` is omitted, Spiffy Roller chooses in this order:

1. `image`, if one is present;
2. `label`, if non-empty;
3. `value`, if present;
4. otherwise blank.

Therefore:

```json
{ "value": 3, "label": "HIT" }
```

will display `HIT`, not `3`.

Use an explicit `display_mode` whenever a face contains more than one possible display source and you want deterministic behavior.

---

# 10. Custom face images

Current Spiffy Roller firmware can decode both **PNG** and **BMP** artwork.

Example:

```json
{
  "image": "icons/success.png",
  "image_mode": "mask"
}
```

The image path is relative to the set folder.

## Image size

The decoder accepts images up to:

```text
96 x 96 pixels
```

A **48 x 48** canvas is recommended because the face overlay system is designed around that size. Artwork is scaled to the final die size as required.

## PNG

PNG is generally the easiest format to author and is recommended for new sets. PNGs are decoded to RGBA internally.

Transparency comes from the PNG alpha channel.

## BMP

The firmware also supports common uncompressed BMP formats, including:

- 1-bit indexed;
- 4-bit indexed;
- 8-bit indexed;
- 24-bit RGB;
- 32-bit RGB/bitfields.

For new artwork, PNG usually avoids BMP palette and alpha-format complications.

---

# 11. Image modes

Images have two rendering modes:

```text
mask
indexed
```

## `mask`

Use `mask` for a one-color icon that should inherit the face's `ink_color`.

```json
{
  "image": "icons/success.png",
  "image_mode": "mask",
  "ink_color": "#FFFFFF"
}
```

For PNG mask images, the **alpha channel** determines which pixels are present. The RGB color of the source artwork is ignored for opaque pixels. Every visible pixel is recolored using `ink_color`.

This means an opaque black icon works correctly as a mask. You do not need to make the source icon white.

Use mask mode for:

- success/failure glyphs;
- pips;
- monochrome RPG symbols;
- icons whose color should change per face.

## `indexed`

Use `indexed` when the image's own colors should be preserved:

```json
{
  "image": "icons/special.png",
  "image_mode": "indexed"
}
```

Despite the historical name `indexed`, current PNG handling preserves the RGBA colors supplied by the decoded image. Transparency is determined by alpha. Black is therefore a valid visible color in indexed PNG artwork.

Use indexed mode for:

- multicolor symbols;
- logos;
- icons with several colors;
- artwork that should not be recolored by `ink_color`.

## Image mode default

If an `image` is present but `image_mode` is omitted, current firmware treats it as a **mask**.

For clarity and forward compatibility, explicitly specify the mode anyway.

---

# 12. Per-face artwork and color example

```json
{
  "id": "special_d6",
  "name": "Special Die",
  "sides": 6,
  "shape": "d6",
  "body_color": "#193B65",
  "ink_color": "#FFFFFF",
  "faces": [
    {
      "value": 0,
      "label": "Blank",
      "display_mode": "blank"
    },
    {
      "value": 1,
      "label": "Hit",
      "image": "icons/hit.png",
      "image_mode": "mask"
    },
    {
      "value": 1,
      "label": "Hit",
      "image": "icons/hit.png",
      "image_mode": "mask"
    },
    {
      "value": 2,
      "label": "Crit",
      "image": "icons/crit.png",
      "image_mode": "indexed",
      "body_color": "#741A25"
    },
    {
      "value": 2,
      "label": "Crit",
      "display_mode": "label",
      "ink_color": "#FFD84D"
    },
    {
      "value": 3,
      "display_mode": "value"
    }
  ]
}
```

---

# 13. Built-in symbolic face metadata

Faces can optionally define a `symbols` object:

```json
{
  "label": "Success + Advantage",
  "symbols": {
    "success": 1,
    "advantage": 1
  }
}
```

Supported symbol names are:

```text
success
advantage
triumph
failure
threat
despair
```

Each symbol count must be an integer from **0 through 4**.

Unspecified symbol counts default to zero.

A more complex example:

```json
{
  "label": "Triumph",
  "symbols": {
    "success": 1,
    "triumph": 1
  },
  "image": "icons/triumph.png",
  "image_mode": "mask"
}
```

The metadata is independent of what is visibly drawn. It exists so Lua rules can evaluate narrative-symbol systems without trying to infer game meaning from filenames or labels.

In Lua, every rolled face exposes a `symbols` table containing all six values.

---

# 14. Creating custom result rules

Simple dice systems do not need a rules script. If numeric addition is enough, omit `rules` entirely.

For dice pools, success-counting systems, cancellation systems, glitches, special results, or other logic, add:

```text
rules.json
rules.lua
```

and reference the definition from `set.json`:

```json
"rules": "rules.json"
```

---

# 15. `rules.json`

The current rules definition format is version 2.

Minimal example:

```json
{
  "format": "spiffy-roller-rules",
  "format_version": 2,
  "engine": "lua",
  "script": "rules.lua",
  "entry": "resolve"
}
```

Required fields:

| Field | Required value / type | Purpose |
|---|---|---|
| `format` | `"spiffy-roller-rules"` | Rules format identifier |
| `format_version` | `2` | Current rules version |
| `engine` | `"lua"` | Current scripting engine |
| `script` | string | Relative path to Lua file |
| `entry` | string | Name of the Lua result function |

Optional fields:

```json
"limits": {
  "instructions": 50000,
  "memory_kb": 96
}
```

and:

```json
"actions": [
  ...
]
```

## Default Lua limits

If no custom values are supplied:

```text
Instruction limit: 50,000
Memory limit:      96 KiB
```

The rules JSON and Lua files are each limited to **64 KiB** by the current loader.

The limits exist to keep an accidental or poorly written script from consuming the ESP32 indefinitely.

---

# 16. Lua sandbox

Rules execute inside a restricted Lua environment.

Standard Lua libraries are initially opened, but the firmware removes access to:

```text
io
os
package
debug
dofile
loadfile
require
load
collectgarbage
```

A rules script therefore cannot read arbitrary files, run operating-system commands, dynamically load modules, or bypass the package structure through those functions.

Keep rules self-contained in the Lua file included with the set.

---

# 17. The roll table passed to Lua

Spiffy Roller calls the configured entry function like this conceptually:

```lua
resolve(roll, options)
```

The `roll` table has this structure:

```lua
roll = {
    dice = {
        {
            die_id = "pool",
            die_name = "Pool Die",
            face_index = 5,
            label = "5",
            value = 5,
            symbols = {
                success = 0,
                advantage = 0,
                triumph = 0,
                failure = 0,
                threat = 0,
                despair = 0
            }
        },
        ...
    },
    numeric_total = 17
}
```

## `roll.dice`

`roll.dice` is a standard Lua array. Use `ipairs()`:

```lua
for index, die in ipairs(roll.dice) do
    ...
end
```

Each entry contains:

| Field | Meaning |
|---|---|
| `die_id` | `id` from the die definition |
| `die_name` | display name from the die definition |
| `face_index` | 1-based position of the rolled face in its `faces` array |
| `label` | face label, or an empty string if no label exists |
| `value` | numeric face value, only present if the face defines one |
| `symbols` | table containing all six supported symbolic counts |

### Testing optional numeric values

Because `value` may be absent, check its type before comparing it:

```lua
if type(die.value) == "number" and die.value >= 5 then
    hits = hits + 1
end
```

## `roll.numeric_total`

This is the sum of all numeric face values rolled.

Faces without `value` do not affect it.

---

# 18. The `options` table

The entry function receives a second table:

```lua
function resolve(roll, options)
```

The current v1.0 rules loader does not populate user-configurable rule controls, so `options` is normally an empty table.

The firmware contains groundwork for rule controls, but they are **not part of the currently implemented custom-set authoring interface**. Do not rely on `controls` entries in a custom rules file for v1.0.

---

# 19. Returning a result from `resolve()`

The entry function should return a table containing:

```lua
return {
    title = "Hits: 4",
    lines = {
        "Ones: 1",
        "GLITCH"
    }
}
```

## `title`

`title` is the primary result line.

The UI buffer allows up to 63 visible characters plus its terminator. Keep titles substantially shorter for readability.

## `lines`

`lines` is an array of secondary text lines:

```lua
lines = {
    "Successes: 3",
    "Advantages: 2"
}
```

Spiffy Roller joins these entries with newline characters into a 95-character detail buffer. Long or numerous lines will therefore be truncated.

Keep result output concise enough to fit the 368x448 display.

## Minimal rules example

```lua
function resolve(roll, options)
    local hits = 0

    for _, die in ipairs(roll.dice) do
        if type(die.value) == "number" and die.value >= 5 then
            hits = hits + 1
        end
    end

    return {
        title = "Hits: " .. tostring(hits),
        lines = {}
    }
end
```

---

# 20. Example: success-counting dice pool

### `set.json`

```json
{
  "format": "universal-dice-set",
  "format_version": 1,
  "id": "simple_pool",
  "name": "Simple Dice Pool",
  "rules": "rules.json",
  "dice": [
    {
      "id": "pool",
      "name": "Pool Die",
      "sides": 6,
      "shape": "d6",
      "body_color": "#1C6C46",
      "ink_color": "#FFFFFF",
      "faces": [
        { "value": 1 },
        { "value": 2 },
        { "value": 3 },
        { "value": 4 },
        { "value": 5 },
        { "value": 6 }
      ]
    }
  ]
}
```

### `rules.json`

```json
{
  "format": "spiffy-roller-rules",
  "format_version": 2,
  "engine": "lua",
  "script": "rules.lua",
  "entry": "resolve"
}
```

### `rules.lua`

```lua
function resolve(roll, options)
    local hits = 0

    for _, die in ipairs(roll.dice) do
        if type(die.value) == "number" and die.value >= 5 then
            hits = hits + 1
        end
    end

    return {
        title = "Hits: " .. tostring(hits),
        lines = {}
    }
end
```

---

# 21. Example: evaluate different die types

A set can contain several die types, and rules can distinguish them using `die_id`.

```lua
function resolve(roll, options)
    local normal_hits = 0
    local bonus_hits = 0

    for _, die in ipairs(roll.dice) do
        if type(die.value) == "number" and die.value >= 5 then
            if die.die_id == "bonus" then
                bonus_hits = bonus_hits + 1
            else
                normal_hits = normal_hits + 1
            end
        end
    end

    return {
        title = "Hits: " .. tostring(normal_hits + bonus_hits),
        lines = {
            "Bonus hits: " .. tostring(bonus_hits)
        }
    }
end
```

Use stable die IDs for game logic. Do not compare `die_name` unless you specifically want the visible name to be part of the rules contract.

---

# 22. Example: symbolic cancellation

Because symbol counts are supplied directly to Lua, a narrative system can total opposing symbols:

```lua
function resolve(roll, options)
    local success = 0
    local failure = 0
    local advantage = 0
    local threat = 0
    local triumph = 0
    local despair = 0

    for _, die in ipairs(roll.dice) do
        local s = die.symbols
        success = success + s.success
        failure = failure + s.failure
        advantage = advantage + s.advantage
        threat = threat + s.threat
        triumph = triumph + s.triumph
        despair = despair + s.despair
    end

    local net_success = success - failure
    local net_advantage = advantage - threat

    return {
        title = "Success: " .. tostring(net_success),
        lines = {
            "Advantage: " .. tostring(net_advantage),
            "Triumph: " .. tostring(triumph) ..
            "  Despair: " .. tostring(despair)
        }
    }
end
```

The exact interpretation of Triumph, Despair, or any other symbol remains entirely under the set author's control.

---

# 23. Post-roll actions

Rules format v2 supports up to **4 declared post-roll actions**.

An action can make a button appear in Spiffy Roller's **Roll Options** menu after a roll. The current action mechanism is specifically implemented around **rerolling selected dice**.

Example `rules.json`:

```json
{
  "format": "spiffy-roller-rules",
  "format_version": 2,
  "engine": "lua",
  "script": "rules.lua",
  "entry": "resolve",
  "actions": [
    {
      "id": "reroll_failures",
      "label": "Reroll failures",
      "available": "can_reroll_failures",
      "apply": "reroll_failures"
    }
  ]
}
```

Action fields:

| Field | Required? | Purpose |
|---|---|---|
| `id` | Yes | Internal action identifier |
| `label` | Yes | Button text shown to the user |
| `available` | No | Lua function deciding whether the action should currently appear |
| `apply` | Yes | Lua function returning the dice to reroll |

If `available` is omitted, the action is considered available until it has been used.

### Current UI behavior with several actions

Although the rules definition can contain up to four actions, the current UI shows the **first available action** in the array. After that action is used, it is marked used and the next available action can become visible when results are refreshed.

Design action order intentionally.

---

# 24. Action availability functions

An availability function receives the same two arguments as `resolve`:

```lua
function can_reroll_failures(roll, options)
    for _, die in ipairs(roll.dice) do
        if type(die.value) == "number" and die.value < 5 then
            return true
        end
    end

    return false
end
```

It should return a Lua truth value.

If the function:

- returns false;
- is missing;
- throws an error; or
- exceeds its limits,

the action is not shown.

---

# 25. Applying a reroll action

The `apply` function returns a table with a `reroll` array containing **1-based positions in `roll.dice`**.

Example:

```lua
function reroll_failures(roll, options)
    local reroll = {}

    for index, die in ipairs(roll.dice) do
        if type(die.value) == "number" and die.value < 5 then
            table.insert(reroll, index)
        end
    end

    return {
        reroll = reroll
    }
end
```

If the current roll is:

```text
roll.dice[1] = 6
roll.dice[2] = 2
roll.dice[3] = 5
roll.dice[4] = 1
```

then:

```lua
return { reroll = {2, 4} }
```

rerolls only the second and fourth dice.

## What Spiffy Roller does with the reroll list

For each valid unique requested index, the firmware:

1. removes the old face's numeric value from `numeric_total` if it had one;
2. rolls a new random face on the **same die type**;
3. replaces that die's face and face index;
4. adds the new numeric value if present;
5. marks that die for the reroll animation;
6. recalculates the displayed result by calling the normal rules entry again.

Duplicate indices are ignored after the first instance.

Indices outside the roll are ignored.

The action itself is then marked as used and cannot be used a second time on that roll.

A completely new shake/roll resets action-used state.

---

# 26. Complete Shadowrun-style action example

This illustrates a threshold-counting pool plus a one-use "reroll failures" action.

### `rules.json`

```json
{
  "format": "spiffy-roller-rules",
  "format_version": 2,
  "engine": "lua",
  "script": "rules.lua",
  "entry": "resolve",
  "limits": {
    "instructions": 50000,
    "memory_kb": 96
  },
  "actions": [
    {
      "id": "reroll_failures",
      "label": "Reroll failures",
      "available": "can_reroll_failures",
      "apply": "reroll_failures"
    }
  ]
}
```

### `rules.lua`

```lua
local function count_hits(roll)
    local hits = 0

    for _, die in ipairs(roll.dice) do
        if type(die.value) == "number" and die.value >= 5 then
            hits = hits + 1
        end
    end

    return hits
end

function resolve(roll, options)
    return {
        title = "Hits: " .. tostring(count_hits(roll)),
        lines = {}
    }
end

function can_reroll_failures(roll, options)
    for _, die in ipairs(roll.dice) do
        if type(die.value) == "number" and die.value < 5 then
            return true
        end
    end

    return false
end

function reroll_failures(roll, options)
    local reroll = {}

    for index, die in ipairs(roll.dice) do
        if type(die.value) == "number" and die.value < 5 then
            table.insert(reroll, index)
        end
    end

    return { reroll = reroll }
end
```

---

# 27. Multiple die types

A custom set can contain up to **16 die types**.

Example:

```json
"dice": [
  {
    "id": "ability",
    "name": "Ability",
    ...
  },
  {
    "id": "difficulty",
    "name": "Difficulty",
    ...
  },
  {
    "id": "boost",
    "name": "Boost",
    ...
  }
]
```

Each appears as a separate item in the horizontal die-type selector. The user can assign an independent quantity to each and roll the combined pool.

The set format currently does **not** provide a per-die selector quantity limit. If a special die is intended to be used only once, document that rule for the user. The selector itself can still be increased beyond one.

---

# 28. Practical roll-size limit

The UI/runtime stores detailed information for up to **64 rolled custom dice**.

For reliable custom rules and display behavior, design and use custom pools with no more than 64 total dice.

The quantity control itself is not hard-capped at 64 per die, so authors should not treat the selector's ability to enter a larger number as evidence that a larger detailed roll is supported.

This distinction matters because the runtime's overall numeric accumulator can continue counting additional rolled values while the detailed `roll.dice` table used by Lua is limited to the stored results. A rules script that counts entries in `roll.dice` therefore should be used with pools of 64 dice or fewer.

---

# 29. Current format limits

| Item | Current limit |
|---|---:|
| Total dice sets in catalog, including built-in Standard | 16 |
| Custom sets available when Standard is present | Up to 15 |
| Die types in one custom set | 16 |
| Faces on one die | 64 |
| Detailed dice in one custom roll | 64 |
| `set.json` size | 64 KiB |
| `rules.json` size | 64 KiB |
| `rules.lua` size | 64 KiB |
| Set ID | 31 chars |
| Die ID | 31 chars |
| Set/die display name | 47 chars |
| Face label | 31 chars |
| Shape name | 15 chars |
| Relative asset path | 127 chars |
| Image dimensions | 96x96 max |
| Declared post-roll actions | 4 |
| Result title buffer | 63 chars plus terminator |
| Result details buffer | 95 chars plus terminator |
| Default Lua instruction limit | 50,000 |
| Default Lua memory limit | 96 KiB |

---

# 30. Fields accepted but not necessarily visible

A useful design principle is to separate **game data** from **display data**.

For example:

```json
{
  "value": 6,
  "label": "EXPLOIT",
  "image": "icons/exploit.png",
  "image_mode": "mask",
  "display_mode": "image"
}
```

The player sees only the image, but Lua still receives:

```lua
die.value == 6
die.label == "EXPLOIT"
```

This is preferable to making Lua deduce meaning from the image filename.

Likewise, a blank face can still carry data:

```json
{
  "value": 0,
  "label": "blank",
  "display_mode": "blank"
}
```

---

# 31. Recommended authoring conventions

These are recommendations rather than parser requirements.

## IDs

Use lowercase machine-readable IDs:

```text
attack
challenge
glitch
force_light
force_dark
```

Avoid encoding visible wording into IDs unless it is part of the stable game logic.

## Names

Use concise human-readable names:

```text
Attack Die
Challenge Die
Glitch Die
```

## Face data

Give every mechanically meaningful face a `label`, even if an image is displayed. This makes Lua scripts and future debugging much easier.

If numeric logic applies, also provide `value`.

## Display mode

Specify `display_mode` explicitly on complex faces. Automatic selection is convenient for simple sets, but explicit display intent makes templates easier for other authors to understand.

## Artwork

Prefer:

- PNG;
- 48x48 canvas;
- transparent background;
- `mask` for one-color symbols;
- `indexed` for multicolor artwork.

## Rules

Keep the game logic in small helper functions and keep `resolve()` focused on producing output. This makes the same calculations reusable by availability functions.

---

# 32. Recommended development workflow

1. **Start without custom rules.** Create one die and verify that the set appears and rolls.
2. **Add all die types.** Verify selector order, names, shapes, and colors.
3. **Add labels and numeric values.** Confirm faces look correct.
4. **Add artwork one image at a time.** This makes bad paths or incompatible files easy to identify.
5. **Add `rules.json` and a minimal `resolve()` function.** Confirm result text appears.
6. **Add game-specific calculations.** Test edge cases such as zero successes or special symbols.
7. **Add actions last.** Test both availability and reroll selection.
8. **Test a fresh `.set` archive**, not only an unpacked development directory.

---

# 33. Validating JSON before copying it to the roller

JSON is strict. Common mistakes include:

- a trailing comma;
- missing quotation marks;
- mismatched `{}` or `[]`;
- comments inside JSON;
- writing hexadecimal colors without quotation marks.

Invalid:

```json
{
  "name": "Example",
  "id": "example",
}
```

Valid:

```json
{
  "name": "Example",
  "id": "example"
}
```

JSON does not allow `// comments` or `/* comments */`.

Lua, on the other hand, does allow comments:

```lua
-- This is a Lua comment.
```

---

# 34. Troubleshooting: set does not appear

Check the following:

1. The file ends in `.set`.
2. It is directly inside `/templates/`.
3. The archive contains `set.json` in its single top-level set folder.
4. `set.json` is valid JSON.
5. `format` is exactly `universal-dice-set`.
6. `format_version` is exactly `1`.
7. The set ID uses only letters, numbers, `_`, or `-`.
8. The set ID does not duplicate another installed set.
9. There are 1-16 dice definitions.
10. Every die has exactly as many `faces` entries as its `sides` value.
11. Every color uses valid `#RRGGBB` format.
12. Referenced rules and artwork paths are relative and exist.
13. The set does not push the total catalog beyond 16 entries including Standard.

Invalid sets are skipped rather than partially loaded.

The serial console can provide more clues during startup.

---

# 35. Troubleshooting: die appears as a square

An unrecognized `shape` falls back to the D6/square shape.

Check the spelling and case of:

```text
d4
d6
d8
d10
d12
d20
triangle_up
triangle_down
diamond
pentagon
hexagon
```

Shape matching is case-sensitive.

---

# 36. Troubleshooting: wrong text appears on a face

Remember the automatic display priority:

```text
image > label > value > blank
```

If a face contains both a label and value but you want the number, add:

```json
"display_mode": "value"
```

If you want no visible content:

```json
"display_mode": "blank"
```

---

# 37. Troubleshooting: image does not appear

Check:

- path spelling and capitalization;
- that the file is actually inside the `.set` archive;
- that the archive has the recommended single top-level folder;
- image dimensions are no larger than 96x96;
- the file is a valid PNG or supported BMP;
- `display_mode` is `image`, or is omitted so automatic image selection occurs;
- `image_mode` is `mask` or `indexed`.

For a PNG mask that seems invisible, verify it has nonzero alpha on the symbol. Source RGB color does not matter in mask mode, but alpha does.

---

# 38. Troubleshooting: rules do not run

If the set rolls but displays only a numeric total, check:

```json
"rules": "rules.json"
```

in `set.json` and verify that `rules.json` contains:

```json
{
  "format": "spiffy-roller-rules",
  "format_version": 2,
  "engine": "lua",
  "script": "rules.lua",
  "entry": "resolve"
}
```

Then verify `rules.lua` actually defines the configured entry function:

```lua
function resolve(roll, options)
    ...
end
```

Runtime failures may display messages such as:

```text
Rules file error
Rules memory error
Rules script error
Rules runtime error
Missing resolve()
```

The serial log normally contains the underlying Lua error text for script/runtime failures.

---

# 39. Troubleshooting: action button does not appear

Check that:

1. the action is declared in `rules.json`;
2. `apply` names an existing Lua function;
3. if `available` is specified, that function exists and returns true for the current roll;
4. the action has not already been used on the current roll;
5. an earlier action in the action array is not currently the first available action.

Remember that the UI displays one available post-roll action at a time.

---

# 40. Troubleshooting: reroll action does nothing

The apply function must return:

```lua
return {
    reroll = {1, 3, 5}
}
```

The numbers are positions in `roll.dice`, starting at **1**, not face indices and not die-type indices.

Wrong:

```lua
return { reroll = {0, 2, 4} }
```

if you intended the first, third, and fifth rolled dice.

Correct:

```lua
return { reroll = {1, 3, 5} }
```

---

# 41. Updating a set

Keep the set `id` stable when publishing a new version of the same set. That lets Spiffy Roller treat it as the same logical dice system.

Replace the old `.set` file with the revised archive, keeping the archive's modification time newer than the previous version so the preparation cache is refreshed.

During active development, avoid leaving both an unpacked template directory and a `.set` package with the same internal set ID in `/templates/`, because duplicate IDs are rejected and whichever copy is discovered first can obscure the other.

---

# 42. Current cache behavior

Packaged sets are unpacked internally into Spiffy Roller's `/set_cache/` directory. The cache lets the firmware read metadata quickly and extract artwork/rules when needed.

Because the current v1.0 implementation does not perform a general stale-cache cleanup pass, authors testing many renamed or removed archives should be aware that old cached entries can remain until storage is cleaned manually.

For normal end-user installation, simply replacing an existing `.set` file of the same filename with a newer version is the most predictable update path.

---

# 43. Full manifest example

```json
{
  "format": "universal-dice-set",
  "format_version": 1,
  "id": "example_narrative",
  "name": "Example Narrative Dice",
  "rules": "rules.json",
  "dice": [
    {
      "id": "positive",
      "name": "Positive",
      "sides": 6,
      "shape": "d6",
      "body_color": "#245FA0",
      "ink_color": "#FFFFFF",
      "faces": [
        {
          "label": "Blank",
          "display_mode": "blank"
        },
        {
          "label": "Success",
          "image": "icons/success.png",
          "image_mode": "mask",
          "symbols": { "success": 1 }
        },
        {
          "label": "Success",
          "image": "icons/success.png",
          "image_mode": "mask",
          "symbols": { "success": 1 }
        },
        {
          "label": "Advantage",
          "image": "icons/advantage.png",
          "image_mode": "mask",
          "symbols": { "advantage": 1 }
        },
        {
          "label": "Success + Advantage",
          "image": "icons/success_advantage.png",
          "image_mode": "mask",
          "symbols": {
            "success": 1,
            "advantage": 1
          }
        },
        {
          "label": "Double Success",
          "image": "icons/double_success.png",
          "image_mode": "mask",
          "symbols": { "success": 2 },
          "body_color": "#173C69",
          "ink_color": "#FFD84D"
        }
      ]
    }
  ]
}
```

---

# 44. Minimal author checklist

Before distributing a custom `.set` file, verify:

- [ ] The archive has one top-level folder.
- [ ] That folder contains `set.json`.
- [ ] `format` is `universal-dice-set`.
- [ ] `format_version` is `1`.
- [ ] Set and die IDs are valid and unique.
- [ ] Every die has 2-64 faces.
- [ ] `sides` exactly matches the number of face objects.
- [ ] Colors use `#RRGGBB`.
- [ ] Image paths are relative and files exist.
- [ ] PNG/BMP images are no larger than 96x96.
- [ ] Complex faces explicitly specify `display_mode`.
- [ ] Mechanically meaningful faces have labels and/or values useful to Lua.
- [ ] `rules.json` uses rules format version 2 if custom rules are used.
- [ ] The configured Lua entry function exists.
- [ ] Post-roll action function names match exactly.
- [ ] Reroll lists use 1-based rolled-die indices.
- [ ] Normal play keeps custom pools at 64 dice or fewer.
- [ ] The final archive has been tested on the actual Spiffy Roller hardware.

---

# 45. Reference summary

## `set.json`

```json
{
  "format": "universal-dice-set",
  "format_version": 1,
  "id": "set_id",
  "name": "Visible Set Name",
  "rules": "rules.json",
  "dice": [
    {
      "id": "die_id",
      "name": "Visible Die Name",
      "sides": 6,
      "shape": "d6",
      "body_color": "#123456",
      "ink_color": "#FFFFFF",
      "faces": [
        {
          "value": 1,
          "label": "Example",
          "display_mode": "label",
          "image": "icons/example.png",
          "image_mode": "mask",
          "body_color": "#654321",
          "ink_color": "#FFD84D",
          "symbols": {
            "success": 1,
            "advantage": 0,
            "triumph": 0,
            "failure": 0,
            "threat": 0,
            "despair": 0
          }
        }
      ]
    }
  ]
}
```

Only `value`, `label`, `display_mode`, `image`, `image_mode`, per-face colors, and `symbols` are optional within a face. A face may intentionally be blank, but supplying meaningful data is recommended when rules need to recognize it.

## `rules.json`

```json
{
  "format": "spiffy-roller-rules",
  "format_version": 2,
  "engine": "lua",
  "script": "rules.lua",
  "entry": "resolve",
  "limits": {
    "instructions": 50000,
    "memory_kb": 96
  },
  "actions": [
    {
      "id": "action_id",
      "label": "Visible action label",
      "available": "availability_function",
      "apply": "apply_function"
    }
  ]
}
```

## `rules.lua`

```lua
function resolve(roll, options)
    return {
        title = "Result",
        lines = {"Detail line"}
    }
end

function availability_function(roll, options)
    return true
end

function apply_function(roll, options)
    return {
        reroll = {1}
    }
end
```

---

## Compatibility note

This guide documents the custom-set behavior implemented by **Spiffy Roller v1.0**. Future firmware versions may add fields or capabilities. Set authors should retain the explicit `format` and `format_version` values so future firmware can distinguish formats cleanly.
