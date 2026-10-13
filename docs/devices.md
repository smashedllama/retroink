---
title: X3, X4 and X4 Pro
nav_order: 3
---

# X3, X4 and X4 Pro

RetroInk runs on three Xteink readers. They share the same reading engine,
Library, dictionary, transfers and System 6 look, but the hardware is not the
same, so a few things differ. This page puts them side by side.

> **New in 0.6.0:** the **Xteink X4 Pro** is supported. Rows and
> cells marked **New** are things that arrived with it.

## At a glance

| | **Xteink X3** | **Xteink X4** | **Xteink X4 Pro** **New** |
|---|---|---|---|
| Firmware file | `firmware-x3-x4-v*.bin` | `firmware-x3-x4-v*.bin` | `firmware-x4-pro-v*.bin` **New** |
| Processor | ESP32-C3 | ESP32-C3 | ESP32-S3 |
| Touch screen | No | No | **Yes**, multi-touch **New** |
| Controls | Front buttons, side rocker, Power | Front buttons, side rocker, Power | Touch, Home key, two edge keys, Power **New** |
| Frontlight | No | No | **Warm and cool**, quick panel **New** |
| Clock chip (keeps date and time asleep) | Yes | **No** | Yes |
| Motion sensor | Yes | No | No |
| USB Drive (SD card as a drive over USB-C) | No | No | **Yes** **New** |
| Update over Wi-Fi | Yes | Yes | Yes, from the same release |

## Features by device

| Feature | X3 | X4 | X4 Pro |
|---|---|---|---|
| Reading, Library, dictionary, transfers, Calendar Sync | Yes | Yes | Yes |
| Moon Phase, Earth, Desk Calendar | Yes | Pick a date | Yes |
| Clock accessory | Yes | No | Yes |
| Today, Book Status, Book + Week, Reading Year sleep screens | Yes | No | Yes |
| Daily reading goal and countdown badge | Yes | No | Yes |
| Marble Maze | Yes | No | No (needs a motion sensor) |
| Tilt page turn | Yes | No | No |
| **Minesweeper** **New** | No | No | **Yes** |
| **Quick brightness and warmth panel** (swipe down) **New** | No | No | **Yes** |
| **Hold a word to highlight, drag to extend** **New** | No | No | **Yes** |
| **Tap and swipe through every screen** **New** | No | No | **Yes** |
| **Pinch to resize text, twist to rotate** | No | No | Yes |
| Rocker on/off switches | Yes | Yes | Yes |

## Which firmware file do I need?

- **X3 or X4:** `firmware-x3-x4-v*.bin`. The same file runs on both.
- **X4 Pro:** `firmware-x4-pro-v*.bin`. It will not run on an X3 or X4, and the
  X3/X4 file will not run on the Pro. The updater checks this and refuses a
  wrong image rather than flashing it.

See [Installation](./installation.html) for how to flash each one.

## X3 vs X4

The X3 has a clock chip and the original X4 does not. That is the main
difference between them, and it decides which features appear. The details
are in [Installation: X3 vs X4](./installation.html#x3-vs-x4).

## The X4 Pro

The Pro has a touch screen instead of front buttons, plus a frontlight and a
clock chip. How to use it, and what it does differently, is on the
[X4 Pro page](./x4-pro.html).

## Not supported

The **X4 Classic** is not supported yet. It uses the same chip family as the
Pro but has buttons only, and it has not been built or tested with RetroInk.
