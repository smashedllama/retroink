---
title: Installation
nav_order: 2
---

# Installation

Already on RetroInk 0.3.0 or newer? [Update over Wi-Fi](#update-over-wi-fi)
from the reader itself. Otherwise, grab a prebuilt `.bin` from the [Releases
page](https://github.com/smashedllama/retroink/releases) and use one of the
install methods below, easiest first. Building from source is only needed if
you want to modify the code yourself.

## Supported Devices

- Xteink X3, X4
- **Xteink X4 Pro (pre-release). New** Its own firmware file,
  `firmware-x4-pro-v*.bin`, built for its ESP32-S3 chip. See
  [X4 Pro](#x4-pro) below.
- Seeed Studio Sticky (build from source with `pio run -e sticky`; releases
  only include the X3/X4 and X4 Pro firmware)

**Not supported (yet):** the Xteink X4 Classic. It has not been built or
tested with RetroInk.

Not sure which device does what? [X3, X4 and X4 Pro](./devices.html) puts them
side by side in a table.

## X3 vs X4

The X3 and the original X4 run the same `firmware-x3-x4-*.bin`, but they
aren't the same hardware. The X3 has a small clock chip that keeps counting
while the reader sleeps. The original X4 has no clock chip at all, and it
powers off completely when it sleeps, so it has no idea what day or time it
is.

A number of RetroInk features are built on knowing today's date. On an X4
they work in one of two ways.

**Pick a date instead.** Moon Phase, Earth, and Desk Calendar ask for a date
the first time you open them. Press **Set Date** to change it any time: the
side buttons move between fields and the front buttons change the value. Earth
also takes a time and a time zone. The date you pick is saved, so each
accessory reopens on it, the picker starts from it next time, and the Moon
Phase, Earth, Desk Calendar, and Calendar Day sleep screens draw it. In the
Desk Calendar, **Set Date** is in the Options menu. With
[Calendar Sync](./calendar-sync.html), the events shown are for the date you
set, so set it again now and then.

**Left out.** These need the real time as it passes, so they don't appear on
an X4:

- **Clock** in Desk Accessories.
- **Marble Maze** in Desk Accessories, which is a tilt game and needs the X3's
  motion sensor.
- **Sleep screens:** Today, Book Status, Book + Week, and Reading Year. If one
  of these was already chosen, the default sleep screen is used instead.
- **Daily reading goal:** the goal and the reader countdown badge, since the
  goal resets each calendar day.
- **Clocks:** the header and reader clocks and the date and time settings.

**Works, with small differences:**

- **Focus Session** times correctly while the reader is awake, but pauses if
  the reader sleeps mid-session.
- **Per-book stats** show a single screen without date-based figures, and
  started/finished dates can't be edited.
- **Highlights and stats backups** aren't dated: highlights have no
  timestamp, "Backup Now" backups are numbered, and automatic stats backup is
  hidden.

Everything else works the same on both: reading, the Library, Puzzle,
all-time totals, transfers, and syncing.

## X4 Pro

**New in 0.6.0, as a pre-release.** The X4 Pro runs `firmware-x4-pro-v*.bin`,
not the X3/X4 file, and it cannot run the X3/X4 file. See [RetroInk on the X4
Pro](./x4-pro.html) for how it is used.

1. **Back up the stock firmware first.** The Pro's stock layout is not the
   same as RetroInk's, so this is the way back if you want it.
2. Download `firmware-x4-pro-v*.bin` from the [Releases
   page](https://github.com/smashedllama/retroink/releases). It is marked as a
   pre-release.
3. Connect the Pro with USB-C and flash the file with the [web flash
   tool](#install-via-the-web-flash-tool-easiest) in Chrome or Edge. This is
   the route the Pro has been tested with.

**Other ways.** If the Pro is already running RetroInk, an [SD card
update](#install-via-sd-card) works the same as on an X3 or X4. `esptool` is
possible with `--chip esp32s3`, but it has not been tested on a Pro.

**Updates.** **Check for Updates** only looks at the latest full release, so
it will not offer a pre-release. Install pre-releases by hand until a full
release includes the Pro file.

## Update over Wi-Fi

If you're already running RetroInk 0.3.0 or newer, you don't need a computer:

1. On the reader, go to **Settings > System > Check for Updates**.
2. Connect to Wi-Fi if it asks. It checks for the latest release and, from
   0.4.1 on, shows what's new before installing.
3. Confirm, and leave the reader alone until it restarts.

Coming from CrossInk, CrossPoint, the stock firmware, or an early RetroInk
test build? Install once with one of the methods below, and future updates
can come over Wi-Fi.

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

Flash the firmware (use `--chip esp32s3` and the `firmware-x4-pro` file for an
X4 Pro):

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
Sync](./obsidian-sync.html) or [Calendar Sync](./calendar-sync.html) setup if
that's what brought you here.
