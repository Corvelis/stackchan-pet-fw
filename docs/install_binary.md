# Binary Installation Guide

This document explains how to install the M5Stack CoreS3 binary distributed through GitHub Releases.

## Release Assets

Download the file that matches the intended use from the GitHub Release:

```text
stackchan_cores3_firmware.bin
stackchan_cores3_factory.bin
```

| File | Use |
| --- | --- |
| `stackchan_cores3_firmware.bin` | Firmware update for existing installs. Preserves Wi-Fi settings, servo calibration, and LittleFS images. |
| `stackchan_cores3_factory.bin` | Factory image for first-time installs. Combines bootloader, partition table, firmware, and LittleFS runtime image data. |

## Important Notes

- This binary is only for M5Stack CoreS3. Do not flash it to other devices.
- Conversation does not work with this firmware alone.
- Conversation, speech recognition, TTS, and response generation require a compatible phone app, WebSocket client, or USB Serial client.
- Existing users should normally update with `stackchan_cores3_firmware.bin`.
- Flashing the factory image resets saved Wi-Fi settings and servo home calibration on the device.
- Images included in the binary must not be extracted and reused as material assets.
- If users post screenshots, capture videos, or demo videos, they must identify this firmware/app in the post.

Example attribution for posts:

```text
Used with Stack-chan CoreS3 Controller
```

## Tsukuyomi-chan Credit

This firmware uses standing illustration material for the free character "Tsukuyomi-chan" (© Rei Yumesaki).

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

On first boot, or when Wi-Fi is not configured, the device starts a setup access point:

```text
SSID: StackChan-Direct
Password: stackchan123
URL: http://192.168.4.1/wifi
```

On the device Network screen, use `Wi-Fi QR` to connect to `StackChan-Direct`,
then use `Setup QR` to open `http://192.168.4.1/wifi`. Without QR codes, connect
a phone or PC to `StackChan-Direct`, then open `http://192.168.4.1/wifi` in a browser.
The setup page supports SSID scanning, saving SSID/password pairs, editing saved networks,
deleting saved networks, and changing connection priority.

## Updating an Existing Install

To keep Wi-Fi settings, servo calibration, and image data, flash only the firmware bin.

### 1. Requirements

- M5Stack CoreS3
- USB-C cable that supports data transfer
- PC or Mac
- `stackchan_cores3_firmware.bin` downloaded from the GitHub Release

### 2. Prepare esptool.py and the serial port

See "Install esptool.py" and "Connect the CoreS3 over USB" in the first-time installation steps below.

### 3. Flash the firmware

Flash `stackchan_cores3_firmware.bin` at offset `0x10000`:

```sh
esptool.py --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x10000 stackchan_cores3_firmware.bin
```

macOS example:

```sh
esptool.py --chip esp32s3 --port /dev/cu.usbmodem101 --baud 921600 write_flash 0x10000 stackchan_cores3_firmware.bin
```

Windows example:

```sh
esptool.py --chip esp32s3 --port COM3 --baud 921600 write_flash 0x10000 stackchan_cores3_firmware.bin
```

## First-Time Installation

First-time users can flash the binary with the steps below.

### 1. Requirements

- M5Stack CoreS3
- USB-C cable that supports data transfer
- PC or Mac
- `stackchan_cores3_factory.bin` downloaded from the GitHub Release

### 2. Install esptool.py

Install `esptool.py` in a Python environment:

```sh
python3 -m pip install esptool
```

Confirm that it is available:

```sh
esptool.py version
```

### 3. Connect the CoreS3 over USB

Connect the CoreS3 to your PC/Mac with USB.

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

### 4. Flash the binary

Use `esptool.py` to flash the factory image at offset `0x0`.
For first-time installation, use `stackchan_cores3_factory.bin`.

```sh
esptool.py --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x0 stackchan_cores3_factory.bin
```

macOS example:

```sh
esptool.py --chip esp32s3 --port /dev/cu.usbmodem101 --baud 921600 write_flash 0x0 stackchan_cores3_factory.bin
```

Windows example:

```sh
esptool.py --chip esp32s3 --port COM3 --baud 921600 write_flash 0x0 stackchan_cores3_factory.bin
```

The device restarts after flashing. If flashing fails, check that the USB cable supports data transfer
and that no serial monitor or other app is using the same port.

### 5. Set up Wi-Fi

After reboot, if Wi-Fi is not configured, the device starts `StackChan-Direct`.
On the device Network screen, scan `Wi-Fi QR` to connect, then scan `Setup QR`
to open the setup page. Without QR codes, connect a phone or PC to
`StackChan-Direct`, then open:

```text
http://192.168.4.1/wifi
```

Follow the page and save the SSID/password for the Wi-Fi network you normally use.

### 6. Connect a conversation client

Conversation does not work with this firmware alone.
A compatible phone app is planned for a future upload.
If you want to connect your own WebSocket or USB Serial client, see the API documentation in
[README.en.md](../README.en.md#connection-points) and
[the affection API details](device_affection_api.md). For USB Serial framing,
see [USB Serial Protocol](usb_serial_protocol.md).

## Basic Usage

- Flick from an edge of the touch screen to show or hide the settings screen.
- Use the bottom tabs to switch between `Network`, `Display`, `Audio`, `Servo`, and `Pwr`.
- `Network` shows the current Wi-Fi mode, IP address, WebSocket endpoint, and Wi-Fi setup QR codes.
- If Wi-Fi is not configured, the device starts a setup Wi-Fi network named `StackChan-Direct`. Connect a phone or PC to it, then open `http://192.168.4.1/wifi` to configure Wi-Fi.
- When the `Network` screen is in SoftAP mode, use `Wi-Fi QR` to connect to `StackChan-Direct`, then use `Setup QR` to open the setup page.
- Hold the `Network` screen to switch between STA mode for normal Wi-Fi connection and SoftAP mode for setup.
- `Display` adjusts brightness and screen on/off.
- `Audio` adjusts speaker volume.
- `Servo` saves the face reference position or returns to the saved position.
- `Pwr` shows battery, temperature, and low-power mode controls.
- Low-power mode caps display brightness and reduces idle face updates and nodding motion.
  Audio playback and lip-sync during speech continue.
- Short-press the power button to turn the screen on or off.
- While the screen is off, petting and shake interactions are disabled. They resume when the screen is turned on.
- When a WebSocket or USB Serial client is connected, tap the microphone overlay on the right side of the face screen to mute or unmute mic streaming.
- When a WebSocket or USB Serial client is connected, tap the camera overlay above the microphone overlay to send a `camera_button` event.
  Network clients should call HTTP `POST /capture`; USB Serial clients should send `capture.request`.

### 7. Calibrate servo home

If the face is not centered, calibrate the home position from the device `Servo` screen.

1. Move the face by hand to the forward, slightly upward position.
2. Tap `Save Direction` to store that direction as the face reference position.
3. Tap `Go to Saved` to return to the saved reference position.
