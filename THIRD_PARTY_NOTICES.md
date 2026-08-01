# Third-Party Notices

This repository vendors the QR Code generator source listed below. PlatformIO
downloads the libraries listed in `platformio.ini` during setup.

## Direct PlatformIO Dependencies

| Dependency | License | Notes |
| --- | --- | --- |
| `m5stack/M5Unified` | MIT | M5Stack unified device library. |
| `m5stack/M5GFX` | MIT, with bundled components under additional permissive/font licenses | See the M5GFX repository for bundled component license details. |
| `bblanchon/ArduinoJson` | MIT | Header-only JSON library. |
| `h2zero/NimBLE-Arduino` | Apache-2.0 | BLE stack wrapper used for StreetPass advertising, scanning, and GATT. Includes NimBLE components and a NOTICE file from upstream. |
| `m5stack/StackChan-BSP` | MIT | StackChan board support package. |
| `m5stack/M5PM1` | MIT | StopWatch power-management library. Copyright (c) 2025 M5Stack Technology CO LTD. |
| `m5stack/M5IOE1` | MIT | StopWatch I/O expander library. Copyright (c) 2026 M5Stack Technology CO LTD. |

The WebSocket endpoint is implemented with ESP-IDF `esp_http_server`, which is
provided by the Arduino-ESP32 framework.

The firmware uses M5GFX's `efontJA_12` and `efontJA_16` data. Its upstream
notice is reproduced below as required for binary distribution:

> Copyright 2000-2001 /efont/ The Electronic Font Open Laboratory. All rights
> reserved.
>
> Redistribution and use in source and binary forms, with or without
> modification, are permitted provided that the following conditions are met:
>
> 1. Redistributions of source code must retain the above copyright notice,
> this list of conditions and the following disclaimer.
> 2. Redistributions in binary form must reproduce the above copyright notice,
> this list of conditions and the following disclaimer in the documentation
> and/or other materials provided with the distribution.
> 3. Neither the name of the team nor the names of its contributors may be used
> to endorse or promote products derived from this font without specific prior
> written permission.
>
> THIS FONT IS PROVIDED BY THE TEAM AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
> IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
> MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
> EVENT SHALL THE TEAM OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
> INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
> LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
> OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
> LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
> NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS FONT, EVEN
> IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

M5GFX also carries GFXFF and IPA font license texts in its pinned source tree.

NimBLE-Arduino's upstream NOTICE states:

> esp-nimble-cpp / NimBLE-Arduino. Copyright 2020-2026 Ryan Powell and
> contributors. Some parts were copied from, derived from, or inspired by
> esp32-snippets, Copyright 2017 Neil Kolban.

## Vendored Source

| Source | License | Notes |
| --- | --- | --- |
| `lib/qrcodegen` from Nayuki QR Code generator library | MIT | C QR Code generator used for on-device Wi-Fi setup QR display. |

The vendored QR Code generator source retains its upstream copyright and MIT
license notice in `lib/qrcodegen/qrcodegen.h` and `lib/qrcodegen/qrcodegen.c`.

## Runtime Images

Tracked runtime images, sprite-sheet samples, and image-generation references
were created and provided by Ailog. They use the same MIT License as the
firmware source, as described in the repository's `LICENSE`.

The tracked face images introduced in tag `0.3.0`, the expanded image set in
tag `v0.3.1`, and the face asset v2 set introduced in `v0.4.0` and retained in
`v0.4.1` were created and provided by Ailog. These original image sets use the
repository's MIT License.

The current source tree and the tracked images in tags `0.3.0` and `v0.3.1`
do not include the previously used third-party Tsukuyomi-chan standing
illustration material. Documentation in older revisions may retain references
to that material, and release binaries from v0.2.3 or earlier may contain
locally prepared runtime images. Any third-party images are not covered by
Ailog's MIT license grant. Users who install replacement images must follow
the rights and terms for those images.

## Historical Release Images

The current MIT grant for Ailog-created image assets does not retroactively
relicense third-party images embedded in historical firmware or factory
binaries. A historical release containing Tsukuyomi-chan images remains
subject to the notice bundled with that release and the original material
provider's terms. Do not treat the current repository's MIT License as
permission to extract, reuse, or redistribute those third-party images.

The tracked image sets introduced in `0.3.0` and expanded in `v0.3.1` are
Ailog-created images and use the MIT License, even though documentation in
those tags retained obsolete references to the earlier development material.

## Firmware License

The firmware source code and Ailog-created image assets in this repository use
the MIT License. See `LICENSE`.
