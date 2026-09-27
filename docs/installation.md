---
title: Installation
nav_order: 2
---

# Installation

Grab a prebuilt `.bin` from the [Releases
page](https://github.com/smashedllama/retroink/releases) and use one of the
install methods below, easiest first. Building from source is only needed if
you want to modify the code yourself.

## Supported Devices

- Xteink X3, X4
- Seeed Studio Sticky

The X4 Pro and X4 Classic are built on a different chip (ESP32-S3) and can't
run this firmware.

## X3 vs X4

The X3 and the original X4 run the same `firmware-x3-x4-*.bin`, but they
aren't the same hardware. The X3 has a small battery-backed clock chip that
keeps the date and time even while the reader is asleep or switched off. The
original X4 has no clock chip at all, so it has no idea what day or time it
is, and there's nothing for the firmware to read.

A number of RetroInk features are built on knowing today's date. X4 testers
confirmed they don't work without a clock, so on an X4 they're left out
rather than shown broken:

- **Desk Accessories:** Clock, Moon Phase, Earth, and Desk Calendar. Focus
  Session, Puzzle, and System Info are still there.
- **Sleep screens:** Today, Book + Week, Reading Year, Moon Phase, Earth, and
  Desk Calendar. If one of these was already chosen, the default sleep screen
  is used instead.
- **Daily reading goal:** the goal and the reader countdown badge, since the
  goal resets each calendar day.
- **Clocks:** the header and reader clocks and the date and time settings.
  These were already hidden on the X4.

Everything else works the same on both: reading, the Library, Focus
Session, per-book stats, all-time totals, transfers, and syncing.

## Install via the web flash tool (easiest)

No software to install. Connect your device with USB-C, then go to
[crosspointreader.com/#flash-tools](https://crosspointreader.com/#flash-tools)
in Chrome or Edge, pick the `.bin` you downloaded, and follow the on-page
steps.

## Install via SD card

Works even on USB-locked devices, and doesn't need a computer connected to
the reader.

1. Copy the `.bin` onto your SD card, anywhere on it.
2. On the device, go to **Settings > System > SD Card Firmware Update**.
3. Navigate to the `.bin` file and confirm the update.

## Install via USB (esptool)

These instructions are for macOS and Linux.

Install `esptool`:

```sh
pip3 install esptool
```

Connect your device with USB-C, then find the device port:

```sh
# Linux
dmesg | grep tty

# macOS
ls /dev/cu.*
```

Flash the firmware:

```sh
# Linux
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware-x3-x4.bin

# macOS
esptool.py --chip esp32c3 --port /dev/cu.usbmodem2101 --baud 921600 write_flash 0x10000 /path/to/firmware-x3-x4.bin
```

Replace the port and firmware path with your actual values.

## Build from source

You'll need [PlatformIO](https://platformio.org/) (the `pio` CLI).

```sh
git clone https://github.com/smashedllama/retroink.git
cd retroink
pio run -e default
```

The built firmware lands at `.pio/build/default/firmware-x3-x4.bin`. Use it
with any of the install methods above.

## After installing

See [What's Different in RetroInk](./whats-different.html) for what's new
over stock CrossInk, or jump straight to [Obsidian Clipping
Sync](./obsidian-sync.html) setup if that's what brought you here.
