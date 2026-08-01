# Spiffy Roller v1.0 User Manual

Spiffy Roller is a handheld electronic dice roller for the Waveshare ESP32-S3-Touch-AMOLED-1.8 V2. It supports the standard tabletop polyhedral dice set, percentile dice, installable custom dice sets, custom result rules, post-roll actions, motion-triggered rolling, touch controls, sound, battery monitoring, and USB file transfer.

This manual describes how to use Spiffy Roller v1.0 after the firmware has been installed.

## Contents

1. [Hardware controls](#hardware-controls)
2. [Starting Spiffy Roller](#starting-spiffy-roller)
3. [The main screen](#the-main-screen)
4. [Choosing dice](#choosing-dice)
5. [Rolling dice](#rolling-dice)
6. [Reading roll results](#reading-roll-results)
7. [Clearing the dice pool](#clearing-the-dice-pool)
8. [Roll Options](#roll-options)
9. [Main menu](#main-menu)
10. [Dice sets](#dice-sets)
11. [Custom dice set options and actions](#custom-dice-set-options-and-actions)
12. [USB transfer mode](#usb-transfer-mode)
13. [Installing and removing custom dice sets](#installing-and-removing-custom-dice-sets)
14. [Custom roll sound](#custom-roll-sound)
15. [Display and power behavior](#display-and-power-behavior)
16. [Battery monitoring](#battery-monitoring)
17. [Saved settings and state](#saved-settings-and-state)
18. [Limits and useful notes](#limits-and-useful-notes)
19. [Troubleshooting](#troubleshooting)
20. [Project links](#project-links)

---

## Hardware controls

Spiffy Roller primarily uses the touchscreen and motion sensor. The two physical buttons on the board also have specific functions.

### PWR button

The **PWR** button is the board's hardware power control.

Use it to power or wake the device after it has fully shut down.

When Spiffy Roller performs its five-minute inactivity shutdown, the screen briefly displays:

> Sleeping - press PWR to wake

### BOOT button

The **BOOT** button is used by Spiffy Roller as an application button during normal operation.

- **Short press:** open or close the main menu.
- **Hold for approximately 3 seconds:** restart directly into USB transfer mode.

The BOOT button can also be pressed while USB transfer mode is active to leave transfer mode and return to the normal dice application.

---

## Starting Spiffy Roller

Power on the device using the PWR button.

During startup, Spiffy Roller initializes the display and hardware, mounts its internal storage, scans installed dice sets, loads custom rules, and prepares the currently selected set.

When startup is complete, the normal rolling screen appears.

On a new installation, the built-in **Standard** dice set starts with one D20 selected.

If a different dice set was selected previously, Spiffy Roller remembers that set and loads it again at startup.

---

## The main screen

The main screen is the normal play area. It is used to build a dice pool, roll it, and view the result.

The interface is deliberately gesture-driven so most of the screen remains available for the dice animation and final results.

The important touch areas are:

- **Bottom portion of the screen:** change die type with a horizontal swipe.
- **Right portion of the screen:** change quantity with a vertical swipe.
- **Bottom-right corner:** supports either horizontal die-type changes or vertical quantity changes.
- **Upper-left corner:** open Roll Options.
- **Center play area:** triple-tap to clear the entire dice pool.

Once a selector gesture begins, the selector overlay appears and shows the current die type and quantity.

---

## Choosing dice

### Selecting a die type

Swipe **horizontally along the bottom of the screen**.

The die-type carousel appears while you swipe. Continue moving left or right until the desired type is centered in the selection box.

Each committed selector step produces an audible tick when sound is enabled.

For the built-in Standard set, the available types are:

- D4
- D6
- D8
- D10
- D12
- D20
- D% percentile pair

Custom dice sets replace this list with the die types defined by that set.

### Changing the quantity

Swipe **vertically along the right side of the screen**.

- Swipe upward to increase the quantity.
- Swipe downward to decrease the quantity.

The quantity wheel appears while it is being adjusted.

A die type may be reduced to zero, which removes that type from the current pool.

### Using the bottom-right corner

The bottom-right corner accepts both gestures:

- predominantly horizontal movement changes die type;
- predominantly vertical movement changes quantity.

Spiffy Roller locks onto the direction of the gesture once the swipe begins, which helps prevent an accidental quantity change while changing die types or vice versa.

### Selector return delay

After you release the screen, the selector remains visible briefly and then returns to the normal playfield.

This delay can be changed in the main menu using **Selector return**. See [Selector return](#selector-return).

### Percentile dice

The Standard set's **D%** entry represents a percentile pair rather than a single physical die.

Selecting one D% rolls:

- one tens die showing 00, 10, 20, and so on; and
- one ones die showing 0 through 9.

The two dice are displayed in different colors.

A result of `00 + 0` is treated as **100**.

Because a percentile roll is a pair, its quantity is limited to either **0 or 1**.

---

## Rolling dice

Once at least one die is in the pool, **shake the device** to roll.

Spiffy Roller plays the roll sound, animates the dice for approximately two seconds, then arranges the final dice on the screen.

You can shake again to reroll the same pool without rebuilding it.

If the pool is empty, shaking does not roll and Spiffy Roller displays:

> Dice pool is empty

### Shake sensitivity

If rolls trigger too easily or require too much movement, change **Shake sensitivity** in the main menu.

The available settings are:

- **Gentle** - easiest to trigger.
- **Normal** - default.
- **Firm** - requires a more deliberate shake.

---

## Reading roll results

### Standard dice

After a Standard-set roll, the top of the screen displays the numeric total of all dice rolled.

The individual dice are arranged by die type and value for easier reading after the animation ends.

### Custom dice sets

Custom sets can define their own result interpretation. Depending on the set, the result area may show things such as:

- successes or hits;
- failures;
- advantages or threats;
- special symbols;
- glitches;
- exploits;
- numeric totals;
- other game-specific summaries.

The exact result text and available post-roll actions are controlled by the installed dice set.

### Large dice pools

Spiffy Roller calculates the complete result even when more dice are rolled than can be retained for individual on-screen rendering.

Up to **64 individual dice results** are retained and displayed. If the total pool is larger, the result screen reports that only part of the pool is being shown, for example:

> Showing 64 of 80 dice

The overall numeric total still includes the complete roll where applicable.

---

## Clearing the dice pool

There are two ways to clear all selected dice.

### Method 1: Roll Options

1. Tap the **upper-left corner** of the main play screen.
2. Tap **Clear dice pool**.

### Method 2: triple-tap shortcut

Triple-tap the **center play area**.

When the pool is cleared, Spiffy Roller displays:

> Dice pool cleared

For the Standard set, all standard-die quantities are reset to zero.

For a custom set, all die quantities for that custom set are reset to zero.

---

## Roll Options

Tap the **upper-left corner of the main play screen** to open **Roll Options**.

Roll Options is always available because it contains **Clear dice pool**, even when the active dice set does not define any game-specific settings.

Depending on the active custom set, this screen may also contain:

- toggle options;
- adjustable numeric options;
- game-specific rule settings;
- a post-roll action such as rerolling certain dice.

To leave Roll Options, **swipe left**.

### Main-screen rule option

A custom dice set can designate one rule control for quick access from the normal result screen. If a set defines such a control, its button is shown on the main screen without requiring you to enter the full Roll Options panel.

The exact label and behavior depend on the installed set.

### Post-roll actions

Some custom sets define an action that becomes available only after a roll. Examples include rerolling failures or applying a game-specific reroll rule.

When available, the action is displayed as a button on the result screen.

Tap the button to apply it. Only the affected dice are animated again; dice that are being kept remain fixed.

Actions can also be limited by the dice set to one use per roll or by other game-specific conditions.

---

## Main menu

Short-press the **BOOT** button to open the main menu.

The menu is vertically scrollable. Drag up or down to reach items that are off-screen.

Short-press BOOT again to close the menu.

The menu contains:

- Set
- Set options
- Sound
- Brightness
- Shake sensitivity
- Display timeout
- Selector return
- USB transfer mode
- About / status

### Set

Displays the currently active dice set, for example:

> Set: Standard

Tap it to open the dice-set selection screen.

See [Dice sets](#dice-sets).

### Set options

Opens the game-specific controls defined by the current custom dice set.

If the Standard set is active, or if the current custom set defines no options, Spiffy Roller displays:

> This set has no options

The same custom controls are also available through Roll Options from the main screen.

### Sound

Toggles interface and rolling sounds.

Available states:

- **On**
- **Off**

When sound is on, Spiffy Roller can play:

- selector ticks;
- a dice-roll sound;
- reroll sounds.

The setting is saved across restarts.

### Brightness

Cycles through three AMOLED brightness levels:

- **35%**
- **65%**
- **100%**

The default is 65%.

Lower brightness can substantially reduce power use on the small onboard battery.

The setting is saved across restarts.

### Shake sensitivity

Cycles through:

- **Gentle**
- **Normal**
- **Firm**

The default is Normal.

The setting is saved across restarts.

### Display timeout

Controls how long the device may remain inactive before the AMOLED display is blanked.

Use the on-screen minus and plus controls to adjust the timeout in **15-second increments**.

Available range:

- **Off**
- **15 seconds through 300 seconds**

The default is **60 seconds**.

This setting controls the display only. It is separate from the device's five-minute automatic power shutdown.

When the display has timed out, movement of the device wakes the screen and restores the selected brightness.

The display does not time out while a roll animation or menu is active.

The setting is saved across restarts.

### Selector return

Controls how long the dice selector remains visible after you finish changing a die type or quantity.

Use the on-screen minus and plus controls to change the delay in **0.5-second increments**.

Available range:

- **0.5 seconds through 5.0 seconds**

The default is **1.0 second**.

The setting is saved across restarts.

### USB transfer mode

Restarts Spiffy Roller as a USB mass-storage device so files can be copied to or from its internal storage.

See [USB transfer mode](#usb-transfer-mode).

### About / status

Displays:

- Spiffy Roller version;
- internal flash-storage total and used space;
- internal RAM total and used space;
- PSRAM total and used space;
- project links.

Tap **Back** to return to the main menu.

---

## Dice sets

Spiffy Roller always includes the built-in **Standard** set. Additional custom sets can be installed in internal storage.

### Selecting a set

1. Short-press **BOOT** to open the main menu.
2. Tap **Set: _current set_**.
3. Scroll through the installed sets if necessary.
4. Tap the desired set.

Spiffy Roller displays:

> Switching dice set...

It then restarts and loads the selected set.

The restart is intentional. It allows Spiffy Roller to safely unload the previous set's artwork and prepare the new set's assets.

The selected set is saved and will remain active after future restarts until another set is selected.

To leave the set-selection screen without changing sets, **swipe left**.

### Standard set

The built-in Standard set contains:

- D4
- D6
- D8
- D10
- D12
- D20
- D% percentile pair

It uses ordinary numeric totals and does not have game-specific Set Options.

### Custom sets

A custom set may change all of the following:

- available die types;
- die names;
- die colors;
- face labels;
- face artwork;
- numeric values;
- result calculations;
- game-specific rule settings;
- post-roll actions.

For example, a custom system may total successes rather than adding face values, use symbols instead of numbers, or provide a reroll button after the initial roll.

---

## Custom dice set options and actions

Spiffy Roller's custom-set format supports optional game logic in addition to custom die faces.

The author of a set can define controls such as:

- an on/off rule toggle;
- an adjustable value;
- a target or threshold;
- a modifier;
- another game-specific setting.

A set can define up to **8 rule controls** and up to **4 actions** in the current firmware.

The labels and meanings are specific to the set. Consult documentation supplied with a particular dice set when its controls are not self-explanatory.

### Rule options versus actions

A **rule option** changes how a roll is interpreted or processed. It can generally be adjusted before rolling.

A **post-roll action** operates on a completed roll. For example, a set can provide a button that rerolls dice meeting a particular condition.

Post-roll actions are displayed only when the set reports that the action is currently available.

Starting a completely new shake roll resets the action-use state for that new roll.

---

## USB transfer mode

USB transfer mode exposes Spiffy Roller's internal FAT storage partition to a computer as a removable USB drive.

The normal dice application does **not** run while USB transfer mode is active.

### Entering USB transfer mode from the menu

1. Connect Spiffy Roller to the computer by USB.
2. Short-press **BOOT** to open the main menu.
3. Scroll to **USB transfer mode**.
4. Tap it.
5. Spiffy Roller saves its current state and restarts.
6. The device appears on the computer as **Spiffy Roller Storage**.

### Entering USB transfer mode with BOOT

You can also hold the **BOOT** button for approximately **3 seconds** during normal operation.

Spiffy Roller saves its state and restarts directly into USB transfer mode.

### Leaving USB transfer mode

Use either method:

- press the **BOOT** button; or
- disconnect the USB cable.

Spiffy Roller exits mass-storage mode and restarts into the normal dice application.

If using BOOT, release the button when exiting. The firmware waits for the button to be released before completing the restart.

### Important file-copy practice

Treat the device like any other removable drive.

Before unplugging it after making changes, allow pending file copies to finish. If your operating system provides an eject/safely-remove function, using it is recommended.

---

## Installing and removing custom dice sets

Custom dice sets are stored in the **templates** directory on the Spiffy Roller USB drive.

The easiest distribution format is a `.set` file created for Spiffy Roller.

### Installing a `.set` package

1. Enter **USB transfer mode**.
2. Open the Spiffy Roller Storage drive on the computer.
3. Open the `templates` folder.
4. Copy the desired `.set` file into `templates`.
5. Exit USB transfer mode.
6. After Spiffy Roller restarts, open **Menu > Set**.
7. Select the newly installed set.

`.set` packages are archives containing the set manifest, artwork, optional rules, and other resources. Spiffy Roller prepares the selected archive automatically.

### Installing an unpacked set

Spiffy Roller also recognizes unpacked set directories placed directly inside `templates`.

An unpacked set must contain a valid `set.json` manifest and any assets referenced by that manifest.

Example layout:

```text
templates/
    my_dice_set/
        set.json
        rules.lua
        icons/
            special.bmp
```

For ordinary users, `.set` packages are preferable because they keep each dice set in a single file.

### Removing a custom set

1. Switch to another dice set first, preferably **Standard**.
2. Enter USB transfer mode.
3. Open `templates`.
4. Delete the unwanted `.set` file or unpacked set folder.
5. Exit USB transfer mode.

The removed set will no longer appear in the set list after restart.

### Replacing or updating a set

To update a `.set` package, replace the old file in `templates` with the new version, then leave USB transfer mode so Spiffy Roller can restart and rescan the catalog.

If possible, keep the same set ID when the new package is intended to be an update of the same set.

---

## Custom roll sound

Spiffy Roller can use a user-supplied WAV file for the rolling sound.

Place the file at:

```text
audio/dice_roll.wav
```

The WAV file must be:

- mono;
- 16-bit PCM;
- 16,000 Hz sample rate.

If the file is missing or invalid, Spiffy Roller automatically uses its built-in generated dice-rolling sound instead.

The custom sound affects dice rolls and custom reroll actions. Selector ticks are generated separately.

To install or replace the sound:

1. Enter USB transfer mode.
2. Open the `audio` folder.
3. Copy the WAV file there as exactly `dice_roll.wav`.
4. Exit USB transfer mode.

---

## Display and power behavior

Spiffy Roller uses two separate inactivity mechanisms: **display timeout** and **automatic shutdown**.

### Display timeout

The configurable Display timeout turns the AMOLED brightness down to zero after the selected period of inactivity.

This does **not** shut down the ESP32.

Moving the device wakes the display immediately and restores the configured brightness.

The timeout can be disabled by setting **Display timeout** to **Off**.

### Five-minute automatic shutdown

When running on battery power, Spiffy Roller fully powers itself off after approximately **five minutes without activity**.

Before shutdown, the current settings and state are saved and the screen displays:

> Sleeping - press PWR to wake

Press **PWR** to turn the unit back on.

The five-minute shutdown is independent of the configurable display timeout and cannot be changed from the v1.0 menu.

### Behavior while connected to USB power

The automatic five-minute full shutdown is disabled while USB power is present.

The configurable display timeout still controls AMOLED blanking during normal dice operation.

---

## Battery monitoring

Spiffy Roller monitors the onboard LiPo battery and estimates its charge percentage from battery voltage.

Open the main menu to see the current battery reading, displayed approximately as:

```text
BATTERY  3.85 V  64%
```

If the battery reading is unavailable, the menu displays dashes instead.

### Low-battery warning

When running from the battery, Spiffy Roller warns when the measured voltage becomes low:

> Low battery - please charge

### Low-battery protection

If voltage falls far enough to risk over-discharging the battery, Spiffy Roller automatically saves its state and powers down.

The screen briefly displays:

> Battery low - shutting down

Connect the device to USB power to recharge it before continuing use.

The displayed battery percentage is an estimate. LiPo voltage varies with load, charging state, temperature, battery age, and other conditions, so the voltage readout is also shown in the menu.

---

## Saved settings and state

Spiffy Roller stores important user settings in nonvolatile memory.

The following are restored after shutdown or restart:

- selected dice set;
- Standard-set dice pool;
- sound on/off;
- brightness;
- shake sensitivity;
- display timeout;
- selector return delay.

### Custom dice pools

The selected **custom set** is remembered, but the current die quantities for custom sets are not persisted across a restart in v1.0.

After restarting into a custom set, rebuild the desired pool before rolling.

### When state is saved

Spiffy Roller saves current state during normal controlled shutdown and before entering USB transfer mode. Selecting another dice set also saves the new active-set selection before the restart.

---

## Limits and useful notes

Current Spiffy Roller v1.0 firmware has the following relevant limits:

- **16 total dice sets**, including the built-in Standard set.
- **16 die types** per custom set.
- **64 faces** per custom die type.
- **64 individual retained/displayed dice** per roll.
- **8 custom rule controls** per set.
- **4 custom post-roll actions** per set.
- `set.json` maximum size: **64 KiB**.

### Pools larger than 64 dice

The selector can represent quantities above 64, and the roller can calculate a roll containing more than 64 dice. Only the first 64 individual results are retained for rendering and rule processing, however.

For custom systems whose rule logic depends on examining every individual die, pools should normally remain at or below 64 dice unless that particular set's documentation says otherwise.

### Custom-set quantity restrictions

The built-in percentile die has firmware-enforced quantity limits because it represents one two-die percentile pair.

Custom die types do not currently have a general per-type maximum quantity setting. If a game uses a special die that should normally appear only once, the user must keep that die's quantity at one unless the custom rule implementation handles otherwise.

---

## Troubleshooting

### Shaking does not roll

Check the following:

- Make sure at least one die is selected.
- Make sure the main menu or another overlay is not open.
- Try **Menu > Shake sensitivity > Gentle**.

If the pool is empty, Spiffy Roller displays **Dice pool is empty**.

### Rolls trigger too easily

Change **Shake sensitivity** to **Firm**.

### The screen went black but the device did not shut down

This is probably the configured **Display timeout**. Move the device to wake the AMOLED.

### The device will not wake from movement

If it has been inactive for approximately five minutes on battery power, it may have fully powered down. Press **PWR**.

### A custom set does not appear

Check that:

- the file is inside `templates`;
- a packaged set has the `.set` extension;
- an unpacked set has its own folder containing `set.json`;
- no more than 15 custom sets are already installed, because the 16-set limit includes Standard;
- the set uses a unique internal ID.

If a set is malformed, Spiffy Roller ignores it rather than adding a broken entry to the set list.

### A newly copied set does not appear immediately

Installed sets are scanned during startup. Exit USB transfer mode or restart Spiffy Roller after copying the set.

### A custom set switches slowly the first time

A packaged `.set` file may need to have its artwork and other assets prepared the first time it is selected. Later loads can use the prepared cache when the source package has not changed.

### Custom dice artwork is missing

The `.set` package may be incomplete, an asset path may be invalid, or the artwork may use an unsupported format. Reinstall a known-good copy of the dice set.

### No custom dice-roll sound plays

Verify the file is named exactly:

```text
audio/dice_roll.wav
```

and is mono, 16-bit PCM, 16,000 Hz WAV.

If it is not valid, Spiffy Roller deliberately falls back to its generated roll sound rather than failing to roll.

### USB storage does not appear

Try the following:

1. Confirm the USB cable supports data, not just charging.
2. Enter transfer mode again using **Menu > USB transfer mode** or a 3-second BOOT hold.
3. Allow the device to restart before checking the computer for the removable drive.
4. If necessary, disconnect and reconnect USB and try again.

### Need to return from USB transfer mode

Press **BOOT** once, or unplug the USB cable. The device restarts into normal dice mode.

---

## Project links

Spiffy Roller source code:

<https://github.com/candre23/SpiffyRoller>

Spiffy Roller Dice Set Maker:

<https://github.com/candre23/SpiffyRoller_SetMaker>

Spiffy Roller community dice sets:

<https://github.com/candre23/SpiffyRoller_DiceSets>

---

## Quick reference

| Action | Control |
|---|---|
| Roll current pool | Shake device |
| Change die type | Horizontal swipe along bottom |
| Change quantity | Vertical swipe along right side |
| Open Roll Options | Tap upper-left corner |
| Clear pool | Roll Options > Clear dice pool |
| Clear pool shortcut | Triple-tap center play area |
| Open/close main menu | Short press BOOT |
| Enter USB transfer mode | Menu option or hold BOOT about 3 seconds |
| Exit USB transfer mode | Press BOOT or disconnect USB |
| Wake blanked display | Move device |
| Wake after full shutdown | Press PWR |
| Change dice set | Menu > Set |
| Adjust game-specific rules | Roll Options or Menu > Set options |

