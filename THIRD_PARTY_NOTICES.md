# Third-Party Notices

This repository does not vendor third-party library source code. PlatformIO
downloads the libraries listed in `platformio.ini` during setup.

## Direct PlatformIO Dependencies

| Dependency | License | Notes |
| --- | --- | --- |
| `m5stack/M5Unified` | MIT | M5Stack unified device library. |
| `m5stack/M5GFX` | MIT, with bundled components under additional permissive/font licenses | See the M5GFX repository for bundled component license details. |
| `bblanchon/ArduinoJson` | MIT | Header-only JSON library. |
| `links2004/WebSockets` | LGPL-2.1 | Also includes public-domain `libb64`. |
| `m5stack/StackChan-BSP` | MIT | StackChan board support package. |

## Runtime Images

This repository does not include third-party character image files.

The local development image set was prepared from:

```text
フリー素材キャラクター「つくよみちゃん」
https://tyc.rei-yumesaki.net/
つくよみちゃん万能立ち絵素材（花兎*様）
https://tyc.rei-yumesaki.net/material/illust/
```

Prepare runtime images locally and follow the terms of the material you use.

## Firmware License

The firmware source code in this repository is licensed under the MIT License.
See `LICENSE`.
