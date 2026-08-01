# Spiffy Roller

**Spiffy Roller** is a featureful, extensible digital dice roller for tabletop role-playing games, designed specifically for the [Waveshare ESP32-S3 1.8" AMOLED Touch Display Development Board](https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm).

It combines animated traditional polyhedral dice with support for fully custom dice sets, image-based faces, narrative dice systems, and portable Lua rules.

## Features

- Built-in standard polyhedral dice
- Animated dice rolling with motion, collisions, and sound
- Shake-to-roll using the onboard motion sensor
- Touchscreen dice type and quantity controls
- Arbitrary mixed dice pools
- Automatic result arrangement and totals
- Custom `.set` dice packages
- Custom colors and PNG face artwork
- Numeric, text, image, and blank die faces
- Lua-based result interpretation for narrative and symbolic dice systems
- Multiple custom die types within a single set
- USB mass-storage mode for adding and removing dice sets

Spiffy Roller is intended to work both as a straightforward electronic replacement for a conventional RPG dice bag and as an open platform for unusual or system-specific dice mechanics.

## The Doohicky

<img width="350" alt="doohicky" src="https://github.com/user-attachments/assets/c423da78-1d18-4532-9c9f-8baefe7a34a4" />   

<img width="350" alt="settings menu" src="https://github.com/user-attachments/assets/b52cac6e-f1eb-4b7a-bd11-8cf9fbf12caa" />

<img width="350" alt="dice selector" src="https://github.com/user-attachments/assets/6fb6d078-e7e0-4298-8560-6d4915044e97" />

<img width="350" alt="controls" src="https://github.com/user-attachments/assets/d1ad2022-8c0b-4c81-8f96-b002a3da224a" />

<img width="350" alt="set options" src="https://github.com/user-attachments/assets/b24cf0e3-a6a5-4f70-871c-f38fb040a2bf" />

<img width="350" alt="set selector" src="https://github.com/user-attachments/assets/63da3421-d30e-4284-b06a-2cb0f1e5a8f9" />




## Supported Hardware

Spiffy Roller v1.0 targets:

**Waveshare ESP32-S3-Touch-AMOLED-1.8**

https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm

Other ESP32 boards and Waveshare display modules are not currently supported.

## Installation

The easiest installation method is the browser-based Spiffy Roller flasher:

**https://candre23.github.io/SpiffyRoller/**

A current desktop version of Chrome or Microsoft Edge is recommended.

The installer flashes a precompiled firmware image directly over USB, so ESP-IDF, Python, an IDE, and a local build environment are not required.

Precompiled firmware and source archives are also available from the [GitHub Releases](https://github.com/candre23/SpiffyRoller/releases) page.

## Custom Dice Sets

Spiffy Roller supports portable `.set` archives containing custom dice definitions, artwork, colors, values, and optional Lua rules.

Custom sets can be created with the companion graphical editor:

**Spiffy Roller Set Maker**  
https://github.com/candre23/SpiffyRoller_SetMaker

Premade and example sets are available here:

**Spiffy Roller Dice Sets**  
https://github.com/candre23/SpiffyRoller_DiceSets

The `.set` format is open and documented so sets may also be created by hand or by third-party tools.

See [dice_templates.md](dice_templates.md) for the complete format specification.

## Documentation

Detailed operating instructions are available in:

**[manual.md](manual.md)**

The manual covers normal use, dice selection, rolling, settings, USB storage mode, custom sets, and other device functions.

## Building From Source

Spiffy Roller v1.0 was developed and tested with:

- ESP-IDF 6.0.2
- Target: `esp32s3`

Basic build commands:

```powershell
idf.py set-target esp32s3
idf.py build
```

To generate a single merged firmware image:

```powershell
idf.py merge-bin
```

## Related Repositories

- **Spiffy Roller firmware:** https://github.com/candre23/SpiffyRoller
- **Spiffy Roller Set Maker:** https://github.com/candre23/SpiffyRoller_SetMaker
- **Spiffy Roller Dice Sets:** https://github.com/candre23/SpiffyRoller_DiceSets

## License

Spiffy Roller is released into the public domain under [The Unlicense](LICENSE).

Third-party libraries and dependencies remain subject to their respective licenses. See [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for attribution and licensing information.

Spiffy Roller is public domain software. Copyleft 2026. Do what thou wilt shall be the whole of the law. One step closer to AGI.
