# Spiffy Roller Third-Party Licenses and Attributions

Spiffy Roller incorporates or depends on third-party open-source software and resources. This document provides attribution and license information for those components.

The presence of a project in this file does not imply that its authors or copyright holders endorse Spiffy Roller.

Component versions listed below reflect the dependencies used by Spiffy Roller v1.0 or the version ranges declared by its ESP-IDF component manifest. Transitive dependency versions may be selected by the ESP-IDF Component Manager at build time.

## Summary

| Component / Resource | Copyright / Author | License |
|---|---|---|
| ESP-IDF | Espressif Systems and contributors | Apache License 2.0 |
| Waveshare ESP32-S3-Touch-AMOLED-1.8 BSP | Waveshare and contributors | Apache License 2.0 |
| Waveshare QMI8658 component | Waveshare and contributors | Apache License 2.0 |
| cJSON | Dave Gamble and cJSON contributors | MIT License |
| ESP TinyUSB integration | Espressif Systems and contributors | Apache License 2.0 |
| TinyUSB | Ha Thach / TinyUSB contributors | MIT License |
| Lua 5.4.7 | Lua.org, PUC-Rio | MIT-style Lua License |
| libpng 1.6.x | PNG Reference Library Authors and contributors | PNG Reference Library License v2 |
| zlib 1.3.x | Jean-loup Gailly and Mark Adler | zlib License |
| LVGL | LVGL Kft and contributors | MIT License |
| Espressif LVGL Port | Espressif Systems and contributors | Apache License 2.0 |
| ESP Codec Device | Espressif Systems and contributors | Apache License 2.0 |
| ESP IO Expander | Espressif Systems and contributors | Apache License 2.0 |
| ESP IO Expander TCA9554 | Espressif Systems and contributors | Apache License 2.0 |
| Montserrat typeface data distributed with LVGL | Julieta Ulanovsky and contributors | SIL Open Font License 1.1 |
| Freeform Universal RPG, classic rules | Nathan Russell / Peril Planet | Creative Commons Attribution 4.0 International |

---

## ESP-IDF

Spiffy Roller is built using the Espressif IoT Development Framework (ESP-IDF).

Copyright (c) Espressif Systems (Shanghai) CO LTD and contributors.

License: Apache License 2.0

Project: https://github.com/espressif/esp-idf

---

## Waveshare ESP32-S3-Touch-AMOLED-1.8 Board Support Package

Spiffy Roller uses the Waveshare board support package for the ESP32-S3-Touch-AMOLED-1.8 hardware platform.

Component: `waveshare/esp32_s3_touch_amoled_1_8`

Declared Spiffy Roller dependency: `^2.0.3`

License: Apache License 2.0

Component page: https://components.espressif.com/components/waveshare/esp32_s3_touch_amoled_1_8

Waveshare: https://www.waveshare.com/

---

## Waveshare QMI8658 Driver

Spiffy Roller uses the Waveshare QMI8658 component for accelerometer and gyroscope access.

Component: `waveshare/qmi8658`

Declared Spiffy Roller dependency: `^2.0.0`

License: Apache License 2.0

Component page: https://components.espressif.com/components/waveshare/qmi8658

---

## cJSON

Spiffy Roller uses cJSON to parse custom dice-set and rules metadata.

Component: `espressif/cjson`

Declared Spiffy Roller dependency: `^1.7.19~2`

Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

License: MIT License

Project: https://github.com/DaveGamble/cJSON

### MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## ESP TinyUSB and TinyUSB

Spiffy Roller uses USB Mass Storage functionality through Espressif's TinyUSB integration and the upstream TinyUSB stack.

### ESP TinyUSB integration

Component: `espressif/esp_tinyusb`

Declared Spiffy Roller dependency: `^2.0.1~1`

Copyright (c) Espressif Systems and contributors.

License: Apache License 2.0

Component page: https://components.espressif.com/components/espressif/esp_tinyusb

### TinyUSB

Copyright (c) 2018, hathach (tinyusb.org) and contributors

License: MIT License

Project: https://github.com/hathach/tinyusb

TinyUSB is covered by the MIT License text reproduced in the cJSON section above, subject to TinyUSB's own copyright notice.

---

## Lua

Spiffy Roller's custom dice rules engine embeds Lua.

Component: `georgik/lua`

Declared Spiffy Roller dependency: `^5.4.7`

Upstream Lua version: 5.4.7

Copyright (c) 1994-2024 Lua.org, PUC-Rio.

License: Lua License, an MIT-style permissive license.

Lua: https://www.lua.org/

Component: https://components.espressif.com/components/georgik/lua

### Lua License

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## libpng

Spiffy Roller uses libpng to decode PNG artwork contained in custom dice sets.

Component: `espressif/libpng`

Declared Spiffy Roller dependency: `^1.6.56`

License: PNG Reference Library License version 2

Project: http://www.libpng.org/pub/png/libpng.html

### PNG Reference Library License version 2

Copyright (c) 1995-2019 The PNG Reference Library Authors.
Copyright (c) 2018-2019 Cosmin Truta.
Copyright (c) 2000-2002, 2004, 2006-2018 Glenn Randers-Pehrson.
Copyright (c) 1996-1997 Andreas Dilger.
Copyright (c) 1995-1996 Guy Eric Schalnat, Group 42, Inc.

The software is supplied "as is", without warranty of any kind, express or implied, including, without limitation, the warranties of merchantability, fitness for a particular purpose, title, and non-infringement. In no event shall the Copyright owners, or anyone distributing the software, be liable for any damages or other liability, whether in contract, tort or otherwise, arising from, out of, or in connection with the software, or the use or other dealings in the software, even if advised of the possibility of such damage.

Permission is hereby granted to use, copy, modify, and distribute this software, or portions hereof, for any purpose, without fee, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated, but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This Copyright notice may not be removed or altered from any source or altered source distribution.

---

## zlib

Spiffy Roller uses zlib for compressed data handling in packaged `.set` archives.

Component: `espressif/zlib`

Declared Spiffy Roller dependency: `^1.3.2`

Copyright (c) 1995-2022 Jean-loup Gailly and Mark Adler

License: zlib License

Project: https://zlib.net/

### zlib License

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

---

## LVGL

Spiffy Roller uses LVGL for its graphical user interface.

Copyright (c) LVGL Kft and contributors.

License: MIT License

Project: https://github.com/lvgl/lvgl

LVGL is covered by the MIT License text reproduced in the cJSON section above, subject to LVGL's own copyright notice.

---

## Espressif LVGL Port

The Waveshare board support package uses Espressif's LVGL integration component.

Component: `espressif/esp_lvgl_port`

Copyright (c) Espressif Systems and contributors.

License: Apache License 2.0

Component page: https://components.espressif.com/components/espressif/esp_lvgl_port

---

## ESP Codec Device

Spiffy Roller uses Espressif's codec-device component to drive the board's audio hardware, including the ES8311 codec.

Component: `espressif/esp_codec_dev`

Copyright (c) Espressif Systems and contributors.

License: Apache License 2.0

Component page: https://components.espressif.com/components/espressif/esp_codec_dev

---

## ESP IO Expander Components

The Waveshare hardware support stack uses Espressif IO-expander components for the board's TCA9554 I/O expander.

Components include:

- `espressif/esp_io_expander`
- `espressif/esp_io_expander_tca9554`

Copyright (c) Espressif Systems and contributors.

License: Apache License 2.0

Component pages:

- https://components.espressif.com/components/espressif/esp_io_expander
- https://components.espressif.com/components/espressif/esp_io_expander_tca9554

---

## Montserrat Typeface

Spiffy Roller uses LVGL's compiled-in Montserrat font resources for interface text.

Montserrat was designed by Julieta Ulanovsky and developed with contributions from the Montserrat project contributors.

License: SIL Open Font License 1.1

Project: https://github.com/JulietaUla/Montserrat

### SIL Open Font License 1.1

Copyright holders grant permission to use, study, copy, merge, embed, modify, redistribute, and sell modified and unmodified copies of the font software, subject to the conditions of the SIL Open Font License 1.1. The font software may not be sold by itself, reserved font names may not be used by derivative versions without permission, and redistributed font software must remain under the OFL. The complete license is available at:

https://openfontlicense.org/open-font-license-official-text/

---

## Freeform Universal RPG

Spiffy Roller's example dice-set collection includes a dice set implementing the resolution dice used by the classic **FU: Freeform Universal RPG**.

**Freeform Universal RPG was created by Nathan Russell of Peril Planet.**

The classic Freeform Universal rules are licensed under the **Creative Commons Attribution 4.0 International License (CC BY 4.0)**.

Attribution:

> This work includes a Spiffy Roller dice-set implementation based on FU: the Freeform Universal RPG by Nathan Russell / Peril Planet, licensed under the Creative Commons Attribution 4.0 International License.

Official site: https://www.perilplanet.com/freeform-universal/

CC BY 4.0: https://creativecommons.org/licenses/by/4.0/

The included Spiffy Roller `.set` file is an independent digital implementation intended as an example of Spiffy Roller's custom dice system. Spiffy Roller is not affiliated with or endorsed by Nathan Russell or Peril Planet.

This attribution applies to the **classic Freeform Universal rules**. The Freeform Universal Second Edition beta has separate copyright terms and is not the source used for this example set.

---

## Apache License 2.0

The following Spiffy Roller dependencies are distributed under the Apache License, Version 2.0, including ESP-IDF, the Waveshare board-support components listed above, and multiple Espressif support components.

Copyright ownership remains with the respective component authors and contributors.

                                 Apache License
                           Version 2.0, January 2004
                        http://www.apache.org/licenses/

TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

1. Definitions.

"License" shall mean the terms and conditions for use, reproduction, and distribution as defined by Sections 1 through 9 of this document.

"Licensor" shall mean the copyright owner or entity authorized by the copyright owner that is granting the License.

"Legal Entity" shall mean the union of the acting entity and all other entities that control, are controlled by, or are under common control with that entity. For the purposes of this definition, "control" means (i) the power, direct or indirect, to cause the direction or management of such entity, whether by contract or otherwise, or (ii) ownership of fifty percent (50%) or more of the outstanding shares, or (iii) beneficial ownership of such entity.

"You" (or "Your") shall mean an individual or Legal Entity exercising permissions granted by this License.

"Source" form shall mean the preferred form for making modifications, including but not limited to software source code, documentation source, and configuration files.

"Object" form shall mean any form resulting from mechanical transformation or translation of a Source form, including but not limited to compiled object code, generated documentation, and conversions to other media types.

"Work" shall mean the work of authorship, whether in Source or Object form, made available under the License, as indicated by a copyright notice that is included in or attached to the work.

"Derivative Works" shall mean any work, whether in Source or Object form, that is based on or derived from the Work and for which the editorial revisions, annotations, elaborations, or other modifications represent, as a whole, an original work of authorship. For the purposes of this License, Derivative Works shall not include works that remain separable from, or merely link to the interfaces of, the Work and Derivative Works thereof.

"Contribution" shall mean any work of authorship, including the original version of the Work and any modifications or additions to that Work or Derivative Works thereof, that is intentionally submitted to Licensor for inclusion in the Work by the copyright owner or by an individual or Legal Entity authorized to submit on behalf of the copyright owner. For the purposes of this definition, "submitted" means any form of electronic, verbal, or written communication sent to the Licensor or its representatives, including but not limited to communication on electronic mailing lists, source-code control systems, and issue-tracking systems that are managed by, or on behalf of, the Licensor for the purpose of discussing and improving the Work, but excluding communication that is conspicuously marked or otherwise designated in writing by the copyright owner as "Not a Contribution."

"Contributor" shall mean Licensor and any individual or Legal Entity on behalf of whom a Contribution has been received by Licensor and subsequently incorporated within the Work.

2. Grant of Copyright License. Subject to the terms and conditions of this License, each Contributor hereby grants to You a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable copyright license to reproduce, prepare Derivative Works of, publicly display, publicly perform, sublicense, and distribute the Work and such Derivative Works in Source or Object form.

3. Grant of Patent License. Subject to the terms and conditions of this License, each Contributor hereby grants to You a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable (except as stated in this section) patent license to make, have made, use, offer to sell, sell, import, and otherwise transfer the Work, where such license applies only to those patent claims licensable by such Contributor that are necessarily infringed by their Contribution(s) alone or by combination of their Contribution(s) with the Work to which such Contribution(s) was submitted. If You institute patent litigation against any entity (including a cross-claim or counterclaim in a lawsuit) alleging that the Work or a Contribution incorporated within the Work constitutes direct or contributory patent infringement, then any patent licenses granted to You under this License for that Work shall terminate as of the date such litigation is filed.

4. Redistribution. You may reproduce and distribute copies of the Work or Derivative Works thereof in any medium, with or without modifications, and in Source or Object form, provided that You meet the following conditions:

(a) You must give any other recipients of the Work or Derivative Works a copy of this License; and

(b) You must cause any modified files to carry prominent notices stating that You changed the files; and

(c) You must retain, in the Source form of any Derivative Works that You distribute, all copyright, patent, trademark, and attribution notices from the Source form of the Work, excluding those notices that do not pertain to any part of the Derivative Works; and

(d) If the Work includes a "NOTICE" text file as part of its distribution, then any Derivative Works that You distribute must include a readable copy of the attribution notices contained within such NOTICE file, excluding those notices that do not pertain to any part of the Derivative Works, in at least one of the following places: within a NOTICE text file distributed as part of the Derivative Works; within the Source form or documentation, if provided along with the Derivative Works; or, within a display generated by the Derivative Works, if and wherever such third-party notices normally appear. The contents of the NOTICE file are for informational purposes only and do not modify the License. You may add Your own attribution notices within Derivative Works that You distribute, alongside or as an addendum to the NOTICE text from the Work, provided that such additional attribution notices cannot be construed as modifying the License.

You may add Your own copyright statement to Your modifications and may provide additional or different license terms and conditions for use, reproduction, or distribution of Your modifications, or for any such Derivative Works as a whole, provided Your use, reproduction, and distribution of the Work otherwise complies with the conditions stated in this License.

5. Submission of Contributions. Unless You explicitly state otherwise, any Contribution intentionally submitted for inclusion in the Work by You to the Licensor shall be under the terms and conditions of this License, without any additional terms or conditions. Notwithstanding the above, nothing herein shall supersede or modify the terms of any separate license agreement you may have executed with Licensor regarding such Contributions.

6. Trademarks. This License does not grant permission to use the trade names, trademarks, service marks, or product names of the Licensor, except as required for reasonable and customary use in describing the origin of the Work and reproducing the content of the NOTICE file.

7. Disclaimer of Warranty. Unless required by applicable law or agreed to in writing, Licensor provides the Work (and each Contributor provides its Contributions) on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied, including, without limitation, any warranties or conditions of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A PARTICULAR PURPOSE. You are solely responsible for determining the appropriateness of using or redistributing the Work and assume any risks associated with Your exercise of permissions under this License.

8. Limitation of Liability. In no event and under no legal theory, whether in tort (including negligence), contract, or otherwise, unless required by applicable law (such as deliberate and grossly negligent acts) or agreed to in writing, shall any Contributor be liable to You for damages, including any direct, indirect, special, incidental, or consequential damages of any character arising as a result of this License or out of the use or inability to use the Work (including but not limited to damages for loss of goodwill, work stoppage, computer failure or malfunction, or any and all other commercial damages or losses), even if such Contributor has been advised of the possibility of such damages.

9. Accepting Warranty or Additional Liability. While redistributing the Work or Derivative Works thereof, You may choose to offer, and charge a fee for, acceptance of support, warranty, indemnity, or other liability obligations and/or rights consistent with this License. However, in accepting such obligations, You may act only on Your own behalf and on Your sole responsibility, not on behalf of any other Contributor, and only if You agree to indemnify, defend, and hold each Contributor harmless for any liability incurred by, or claims asserted against, such Contributor by reason of your accepting any such warranty or additional liability.

END OF TERMS AND CONDITIONS

---

## Creative Commons Attribution 4.0 International

The Freeform Universal attribution above is provided under CC BY 4.0. The authoritative license text is maintained by Creative Commons:

https://creativecommons.org/licenses/by/4.0/legalcode

In summary, CC BY 4.0 permits sharing and adaptation for any purpose, including commercial use, provided appropriate credit is given, a link to the license is supplied, and changes are indicated. The official license text governs if this summary differs from the license.

---

## Notes for Distributors

When distributing Spiffy Roller source or binaries, retain this file with the distribution. Third-party components may also contain their own `LICENSE`, `COPYING`, or `NOTICE` files in ESP-IDF or in ESP-IDF Component Manager packages. Those original files remain authoritative and should be preserved when redistributing the corresponding third-party source.

Spiffy Roller's own license, if any, is separate from the licenses listed here and does not replace the licenses of third-party works.
