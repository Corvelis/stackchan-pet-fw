# Binary Installation Guide

This document explains how to install firmware binaries distributed through
GitHub Releases. Always choose the file that matches your device.

## Release Assets

GitHub Releases include the following firmware bin and factory image for each
device. Download the file that matches your device from the Release.

| Device | Firmware update | First-time factory image |
| --- | --- | --- |
| CoreS3 + Stack-chan | `stackchan_cores3_firmware.bin` | `stackchan_cores3_factory.bin` |
| StopWatch | `stackchan_stopwatch_firmware.bin` | `stackchan_stopwatch_factory.bin` |
| AtomS3R Chatbot | `stackchan_atoms3r_firmware.bin` | `stackchan_atoms3r_factory.bin` |

| Type | Use |
| --- | --- |
| firmware bin | Update an existing install. Preserves Wi-Fi settings, LittleFS images, and CoreS3 servo calibration. |
| factory image | First-time install. Combines bootloader, partition table, firmware, and LittleFS runtime image data. |

For release maintainers, this command generates assets with the same file names
into `dist/`:

```sh
bash scripts/build_release_bins.sh all
```

To generate one device, pass `cores3`, `stopwatch`, or `atoms3r`.

## Important Notes

- Do not flash a binary built for a different device.
- Flashing a factory image resets saved Wi-Fi settings, StreetPass settings, and CoreS3 servo home calibration.
- Conversation does not work with this firmware alone.
- Conversation, speech recognition, TTS, and response generation require a compatible phone app, WebSocket client, or USB Serial client.
- StopWatch and AtomS3R Chatbot have no camera or servo. `/capture`, `capture.request`, and servo home calibration are unavailable.
- Images included in the binary must not be extracted and reused as material assets.
- If users post screenshots, capture videos, or demo videos, they must identify this firmware/app in the post.

Example attribution for posts:

```text
Used with Stack-chan Multi-Device Controller
```

## Tsukuyomi-chan Credit

This firmware uses standing illustration material for the free character
"Tsukuyomi-chan" (© Rei Yumesaki).

```text
フリー素材キャラクター「つくよみちゃん」（© Rei Yumesaki）
https://tyc.rei-yumesaki.net/

つくよみちゃん万能立ち絵素材（花兎*様）
https://tyc.rei-yumesaki.net/material/illust/
```

Before publishing, review the official Tsukuyomi-chan terms and credit rules:

- Terms: https://tyc.rei-yumesaki.net/about/terms/
- Credit rules: https://tyc.rei-yumesaki.net/about/terms/credit/

## Wi-Fi Setup

On first boot, or when Wi-Fi is not configured, the device starts a setup access point.

| Device | SSID | Password | URL |
| --- | --- | --- | --- |
| CoreS3 | `StackChan-Direct` | `stackchan123` | `http://192.168.4.1/wifi` |
| StopWatch | `StopWatch` | `stackchan123` | `http://192.168.4.1/wifi` |
| AtomS3R | `AtomS3R` | `stackchan123` | `http://192.168.4.1/wifi` |

On the device Network screen, use `Wi-Fi QR` to connect to the setup Wi-Fi, then
use `Setup QR` to open the setup page. Without QR codes, connect a phone or PC
to the target device SSID, then open `http://192.168.4.1/wifi` in a browser.
The setup page supports SSID scanning, saving SSID/password pairs, editing saved
networks, deleting saved networks, and changing connection priority.

## Updating An Existing Install

To keep Wi-Fi settings, image data, and CoreS3 servo calibration, flash only the
firmware bin for the target device.

### 1. Requirements

- Target device
- USB-C cable that supports data transfer
- PC or Mac
- Firmware bin for the target device downloaded from the GitHub Release

### 2. Prepare esptool.py and the serial port

See "Install esptool.py" and "Connect the device over USB" in the first-time
installation steps below.

### 3. Flash the firmware

Flash the firmware bin at offset `0x10000`:

```sh
esptool.py --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x10000 <FIRMWARE_BIN>
```

macOS example:

```sh
esptool.py --chip esp32s3 --port /dev/cu.usbmodem101 --baud 921600 write_flash 0x10000 stackchan_cores3_firmware.bin
```

Windows example:

```sh
esptool.py --chip esp32s3 --port COM3 --baud 921600 write_flash 0x10000 stackchan_cores3_firmware.bin
```

Replace `stackchan_cores3_firmware.bin` with `stackchan_stopwatch_firmware.bin`
for StopWatch or `stackchan_atoms3r_firmware.bin` for AtomS3R.

## First-Time Installation

First-time users should flash the factory image for the target device.

### 1. Requirements

- Target device
- USB-C cable that supports data transfer
- PC or Mac
- Factory image for the target device downloaded from the GitHub Release

### 2. Install esptool.py

Install `esptool.py` in a Python environment:

```sh
python3 -m pip install esptool
```

Confirm that it is available:

```sh
esptool.py version
```

### 3. Connect The Device Over USB

Connect the target device to your PC/Mac with USB.

On macOS, the port usually looks like this:

```text
/dev/cu.usbmodem101
```

You can list serial ports with:

```sh
ls /dev/cu.*
```

On Windows, the port usually looks like `COM3` or `COM4`.
Check Device Manager if you are not sure.

### 4. Flash The Binary

Flash the factory image at offset `0x0`:

```sh
esptool.py --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x0 <FACTORY_BIN>
```

macOS example:

```sh
esptool.py --chip esp32s3 --port /dev/cu.usbmodem101 --baud 921600 write_flash 0x0 stackchan_cores3_factory.bin
```

Windows example:

```sh
esptool.py --chip esp32s3 --port COM3 --baud 921600 write_flash 0x0 stackchan_cores3_factory.bin
```

Replace `stackchan_cores3_factory.bin` with `stackchan_stopwatch_factory.bin`
for StopWatch or `stackchan_atoms3r_factory.bin` for AtomS3R.

The device restarts after flashing. If flashing fails, check that the USB cable
supports data transfer and that no serial monitor or other app is using the same
port.

### 5. Set Up Wi-Fi

After reboot, if Wi-Fi is not configured, the target device starts its setup
Wi-Fi. On the device Network screen, scan `Wi-Fi QR` to connect, then scan
`Setup QR` to open the setup page. Without QR codes, connect a phone or PC to
the target device setup Wi-Fi, then open:

```text
http://192.168.4.1/wifi
```

Follow the page and save the SSID/password for the Wi-Fi network you normally use.

### 6. Connect A Conversation Client

Conversation does not work with this firmware alone.
A compatible phone app should be connected according to its own release notes.
If you connect your own WebSocket or USB Serial client, see the API
documentation in [README.en.md](../README.en.md#connection-points) and
[the affection API details](device_affection_api.md). For USB Serial framing,
see [USB Serial Protocol](usb_serial_protocol.md).

## Basic Usage

See the [Device Guide](devices.md) for detailed target-specific controls.

Common:

- `Network` shows the current Wi-Fi mode, IP address, WebSocket endpoint, and Wi-Fi setup QR codes.
- If Wi-Fi is not configured, the device starts the target-specific setup Wi-Fi. Connect a phone or PC to it, then open `http://192.168.4.1/wifi`.
- `Display` adjusts brightness and screen on/off. AtomS3R Chatbot has no `Display` screen.
- `Audio` adjusts speaker volume.
- `Pwr` shows battery, temperature, and low-power mode controls.
- Low-power mode caps display brightness and reduces idle face updates.
  Audio playback and lip-sync during speech continue.
- CoreS3 mutes or unmutes mic streaming by tapping the mic overlay. AtomS3R Chatbot mutes or unmutes mic streaming by double-clicking BtnA on the normal face screen. StopWatch has no on-device mic mute control.

CoreS3:

- Flick from an edge of the touch screen to show or hide the settings screen.
- Back touch triggers petting.
- Tapping the camera overlay sends a `camera_button` event.
- `Servo` saves the face reference position or returns to the saved position.

StopWatch:

- Short-press BtnA to show or hide the settings screen.
- Press BtnB or the power button to turn the display on or off.
- Touch or drag near the center of the normal face screen to trigger petting.
- The settings screen order is Network, Display, Audio, Power, and StreetPass. Use the top `<` / `>` buttons or left/right flicks to switch pages.
- On Display and Audio, tap `-` / `+` to adjust brightness or volume in 20-point steps.

AtomS3R Chatbot:

- Short-press BtnA to advance pages. From the normal face screen it opens Network, then cycles Network -> StreetPass -> Audio -> Power -> normal screen.
- Network screen: short-press moves to StreetPass, double-click shows or hides `Setup QR`, and hold switches between STA and SoftAP.
- StreetPass screen: short-press moves to Audio, double-click switches Status / Profile / Latest views, and hold toggles StreetPass.
- Audio screen: short-press moves to Power. Double-click enters volume adjust mode; while in that mode, short-press increases volume and hold decreases volume.
- Power screen: short-press returns to the normal face screen, and hold toggles low-power mode.
- Normal face screen: hold triggers petting, and double-click mutes or unmutes mic streaming.

### Calibrate CoreS3 Servo Home

If the face is not centered, calibrate the home position from the CoreS3 `Servo` screen.

1. Move the face by hand to the forward, slightly upward position.
2. Tap `Save Direction` to store that direction as the face reference position.
3. Tap `Go to Saved` to return to the saved reference position.
