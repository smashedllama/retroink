---
title: X3, X4 and X4 Pro
nav_order: 3
---

# X3, X4 and X4 Pro

RetroInk runs on three Xteink readers. They share the reading engine, the
Library, the dictionary, transfers, syncing and the System 6 look, but the
hardware is different, so some features only exist on some of them. This page
is the full list: what each device has, what works where, and **why** when it
doesn't.

**Legend.** ✅ works · 🟨 works, with a difference · ➖ not available.
**New** marks things that arrived with the X4 Pro in 0.6.0.

## Which file is for which device

| | X3 | X4 | X4 Pro **New** |
|---|---|---|---|
| Firmware file | `firmware-x3-x4-v*.bin` | `firmware-x3-x4-v*.bin` | `firmware-x4-pro-v*.bin` |
| Same file as | the X4 | the X3 | none, its own build |
| Runs on the other devices | ➖ | ➖ | ➖ |
| Updates over Wi-Fi | ✅ | ✅ | ✅ |

The X3 and X4 share one file because they share a processor family; the
firmware tells them apart when it starts. The Pro uses a different processor,
so it has its own file, and the reader refuses a file built for another
device.

## The hardware

This is the reason behind almost every difference further down.

| | **X3** | **X4** | **X4 Pro** **New** |
|---|---|---|---|
| Screen | 792 × 528 e-ink | 800 × 480 e-ink | 800 × 480 e-ink |
| Processor | ESP32-C3 | ESP32-C3 | ESP32-S3 with 8 MB extra RAM |
| Touch screen | ➖ | ➖ | ✅ multi-touch |
| Front buttons (Back, Select, Left, Right) | ✅ | ✅ | ➖ |
| Side rocker (Up, Down) | ✅ | ✅ | 🟨 two edge keys |
| Home key | ➖ | ➖ | ✅ capacitive |
| Power button | ✅ | ✅ | ✅ |
| Frontlight | ➖ | ➖ | ✅ warm and cool |
| Clock chip (keeps time while asleep) | ✅ | ➖ | ✅ |
| Motion sensor | ✅ | ➖ | ➖ |
| Battery gauge | chip | voltage reading | chip |

## Features, device by device

### Reading

| Feature | X3 | X4 | X4 Pro | Why |
|---|---|---|---|---|
| EPUB, TXT and XTC reading | ✅ | ✅ | ✅ | Shared code |
| Fonts, SD card fonts, margins, layout options | ✅ | ✅ | ✅ | Shared code |
| Dictionary lookup | ✅ | ✅ | ✅ | Shared code |
| Bookmarks, footnotes, chapters, go to % | ✅ | ✅ | ✅ | Shared code |
| Clippings and highlights | ✅ | ✅ | ✅ | Shared code |
| Highlight by holding a word and dragging **New** | ➖ | ➖ | ✅ | Needs a touch screen |
| Turn pages with buttons | ✅ | ✅ | 🟨 edge keys | The Pro has no front or side-rocker buttons |
| Turn pages by tap or swipe **New** | ➖ | ➖ | ✅ | Needs a touch screen |
| Tilt to turn pages | ✅ | ➖ | ➖ | Needs a motion sensor, which only the X3 has |
| Pinch to change font size, twist to rotate **New** | ➖ | ➖ | ✅ | Needs a multi-touch screen |
| Auto page turn | ✅ | ✅ | ✅ | Shared code |
| Page refresh on screen change | fast | half refresh | half refresh | The X3 has a different panel controller from the X4 and the Pro |

### Library, files and sync

| Feature | X3 | X4 | X4 Pro | Why |
|---|---|---|---|---|
| Library with shelves, custom shelves, sorts, A–Z | ✅ | ✅ | ✅ | Shared code |
| Browse and move files in the web file manager | ✅ | ✅ | ✅ | Wi-Fi |
| **Move Selected** (many files at once) | ✅ | ✅ | ✅ | Wi-Fi |
| OPDS, Calibre, WebDAV, Nearby transfer | ✅ | ✅ | ✅ | Wi-Fi |
| KOReader progress sync | ✅ | ✅ | ✅ | Wi-Fi |
| Obsidian clipping sync | ✅ | ✅ | ✅ | Wi-Fi |
| **USB Drive** (SD card as a drive over USB-C) **New** | ➖ | ➖ | ✅ | Needs USB mass-storage support that the Pro's S3 processor has and the X3/X4's C3 does not |
| Update over Wi-Fi | ✅ | ✅ | ✅ | Each release carries both files |

### Look and feel

| Feature | X3 | X4 | X4 Pro | Why |
|---|---|---|---|---|
| System 6 theme, 16 desktop patterns | ✅ | ✅ | ✅ | Shared code |
| **O \| I rocker** on/off switches | ✅ | ✅ | ✅ | Shared code |
| System 6 alert dialogs | ✅ | ✅ | ✅ | Shared code |
| Other themes (Lyra, Carousel, Minimal, Dashboard) | ✅ | ✅ | ✅ | Shared code |
| Classic and Rounded themes | ✅ | ✅ | ➖ | Not offered on touch devices |
| Bottom key bar | ✅ four keys | ✅ four keys | 🟨 one or none | The Pro uses touch and the title bar's Back box instead |
| Tappable Back box in every window **New** | ➖ | ➖ | ✅ | Needs a touch screen |
| Remap the front buttons | ✅ | ✅ | ➖ | The Pro has none |
| Sunlight Fading Fix | ✅ | ✅ | ➖ | Button devices only |
| **Quick brightness and warmth panel** (swipe down) **New** | ➖ | ➖ | ✅ | Needs a frontlight and a touch screen |
| Frontlight settings and wake behavior **New** | ➖ | ➖ | ✅ | Needs a frontlight |

### Time and date

The clock chip is what separates the X4 from the other two. Without it the
reader has no idea what day it is after it sleeps.

| Feature | X3 | X4 | X4 Pro | Why |
|---|---|---|---|---|
| Date and time kept while asleep | ✅ | ➖ | ✅ | Needs the clock chip |
| Clock in the header and reader | ✅ | ➖ | ✅ | Needs the clock chip |
| Date, time and time zone settings, sync from Wi-Fi | ✅ | ➖ | ✅ | Needs the clock chip |
| Clock desk accessory | ✅ | ➖ | ✅ | Needs the real time |
| Moon Phase, Earth, Desk Calendar | ✅ | 🟨 pick a date | ✅ | The X4 asks you for a date and saves it |
| Calendar Sync | ✅ | 🟨 for the date you set | ✅ | The X4 shows events for the date it was given |
| Today sleep screen | ✅ | ➖ | ✅ | Needs the real date |
| Book Status sleep screen | ✅ | ➖ | ✅ | Needs the real date |
| Book + Week and Reading Year sleep screens | ✅ | ➖ | ✅ | Needs the real date |
| Moon, Earth, Desk Calendar, Calendar Day sleep screens | ✅ | 🟨 draw your saved date | ✅ | The X4 uses the date you picked |
| Daily reading goal and countdown badge | ✅ | ➖ | ✅ | The goal resets each calendar day |
| Focus Session | ✅ | 🟨 pauses while asleep | ✅ | The X4 loses time when it powers off |
| Per-book reading stats | ✅ with dates | 🟨 single screen, no dates | ✅ with dates | Dated figures need a clock |
| Automatic stats backup | ✅ | ➖ | ✅ | Backups are dated; **Backup Now** works everywhere, numbered on the X4 |
| Timestamps on highlights | ✅ | ➖ | ✅ | Needs the clock |
| Frontlight schedule **New** | ➖ | ➖ | ✅ | Needs a frontlight and a clock |

### Desk accessories and games

| Feature | X3 | X4 | X4 Pro | Why |
|---|---|---|---|---|
| Focus Session | ✅ | ✅ | ✅ | Shared code |
| Puzzle | ✅ | ✅ | ✅ | Shared code |
| Moon Phase, Earth, Desk Calendar, System Info | ✅ | ✅ | ✅ | The X4 asks for a date |
| Clock | ✅ | ➖ | ✅ | Needs the real time |
| Marble Maze | ✅ | ➖ | ➖ | Needs a motion sensor |
| **Minesweeper** **New** | ➖ | ➖ | ✅ | Plays by touch |

## What each device gets

**X3.** Everything except touch-only features. It is the most complete
button device: it has the clock chip and the motion sensor, so it gets the
Clock, all the date-driven sleep screens and goals, Marble Maze and tilt page
turn.

**X4.** The same reading, Library, syncing and look as the X3, but with no
clock chip and no motion sensor. Date-driven features are either asked for
(Moon Phase, Earth, Desk Calendar) or left out (Clock, the stats sleep
screens, daily goals, header clocks). Marble Maze and tilt page turn are not
available. [More on the X4](./installation.html#x3-vs-x4).

**X4 Pro. New** A touch-first RetroInk on a touch screen, with a clock chip
and a frontlight. It gets the date features of the X3, plus the quick
brightness panel, touch reading gestures, hold-and-drag highlighting, USB
Drive and Minesweeper. It has no motion sensor, so no Marble Maze or tilt page
turn, and no front buttons, so no button remapping. See [RetroInk on the X4
Pro](./x4-pro.html).

## Firmware size

Both files fit the 6.55 MB app partition with room to spare.

| File | Size (0.6.0) |
|---|---|
| `firmware-x3-x4-v0.6.0.bin` | about 6.44 MB |
| `firmware-x4-pro-v0.6.0.bin` | about 6.39 MB |

## Not supported

The **X4 Classic** is not supported yet. It uses the same family of processor
as the Pro but has buttons only, and it has not been built or tested with
RetroInk.
