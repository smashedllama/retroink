---
title: FAQ
nav_order: 6
---

# FAQ

## Which firmware do I install?

The X3 and X4 use `firmware-x3-x4-v*.bin`. The X4 Pro uses
`firmware-x4-pro-v*.bin`. The two are not interchangeable. See
[X3, X4 and X4 Pro](./devices.html).

## Why is there no Clock (or the reading-stats sleep screens) on my X4?

The original X4 has no clock chip, so it never knows the date or time while it
sleeps. Features that need the real time as it passes are left out, and
Moon Phase, Earth and Desk Calendar ask you to pick a date instead. The X3 and
X4 Pro have a clock chip and do not have this limit.
[More detail](./installation.html#x3-vs-x4).

## Is the X4 Pro supported?

Yes, as a pre-release since 0.6.0. It works on a real device but has had less
use than the X3 and X4, so please report anything odd. **New**

## Where is Marble Maze on the X4 Pro?

It needs a motion sensor. The X3 has one; the X4 and X4 Pro do not.

## Will Check for Updates find the Pro build?

Not while it is a pre-release. The updater only looks at the latest full
release. Install pre-releases by hand, then updates over Wi-Fi resume once a
full release includes the Pro firmware.

## Can I go back to the stock firmware?

Yes. Back up the stock firmware before the first install, and flash that
image again if you want to return. Do not skip the backup on the Pro, whose
stock layout differs from the X3 and X4.

## I tapped a row on the Pro and nothing looked selected

That is intended. On the Pro, nothing stays highlighted after a tap, because
the tap already did the job. Press an edge key and the highlight comes back.

## What are the O and I marks on the switches?

**I** is on and **O** is off, as on a power switch. The half that is pressed
down is the current state. **New**

## How do I get to the reader menu on the Pro?

Swipe up from the page, or hold the Home key. **New**

## How do I highlight text on the Pro?

Hold a word for half a second, drag, and lift. Let go without dragging to look
the word up instead. **New**

## My clippings did not appear in Obsidian

See [Obsidian Clipping Sync](./obsidian-sync.html) and make sure the reader and
the vault are on the same network, then run the sync from **File Transfer**.

## Something looks wrong or the reader will not start

Check [Troubleshooting](./troubleshooting.html). If it is still wrong, open an
issue on [GitHub](https://github.com/smashedllama/retroink/issues) with the
firmware version from **Settings > System**.
