# Calendar Sync

Shows events from your Google, Apple (iCloud) or Outlook calendar in the Desk Calendar, and lists what is coming up on the Desk Calendar sleep screen. You give the reader your calendar's iCal link once, then update it whenever you like. The reader only ever reads the calendar; it never changes it.

The events live on the SD card (`/.crosspoint/calendar.bin`), so nothing is lost when the reader restarts, and the reader works offline between syncs.

## Set it up

1. Get your calendar's iCal link (below).
2. On the reader, open **File Transfer** and start a web server (Join a Network). Open the address it shows, go to **Settings**, and find the **Desk Calendar** card.
3. Paste the link and press **Save**, then **Sync Now**.

After that, you can update from the reader alone: open the Desk Calendar, press the middle button (**Options**), and choose **Sync Calendar**. It connects to Wi-Fi, downloads the calendar, and returns to Home.

## Getting the link

- **Google Calendar**: on a computer, Settings, choose the calendar, Integrate calendar, copy **Secret address in iCal format**.
- **Apple Calendar (iCloud)**: in the Calendar app, share the calendar and turn on **Public Calendar**, then copy the link. It starts with `webcal://`, which works as is.
- **Outlook / Microsoft 365**: Outlook on the web, Settings, Calendar, Shared calendars, **Publish a calendar**, pick the calendar with "Can view all details", and copy the **ICS** link.

Anyone with the link can read the calendar, so keep it private. The reader stores it obfuscated on the SD card and never shows it again after you save it.

## What you see

- **Desk Calendar**: a small square under every day that has an event. The front Left and Right buttons move one day at a time (rolling into the next or previous month), the side page-turn buttons change the month, and a tap picks a day. The selected day is the filled (black) one, and today has a double border around its number; when they are the same day you get a filled day with a ring inside. The events for the selected day are listed under the grid. If a day has more events than fit, the list ends with "+N more" and a reminder that **Options > Day View** shows them all. The Options button holds Sync Calendar (and Set Date on the original X4).
- **Desk Calendar sleep screen**: the same dots, plus what is still to come: today's remaining events with their times, then later days by date. On a reader with a clock, today's finished events are left out. On the original X4 it shows the day you set in Options > Set Date.

Repeating events (daily, weekly, monthly, yearly, with days of the week, "second Thursday", end dates, and skipped dates) are expanded, and moved or cancelled single occurrences are respected. About 45 days back and 13 months ahead are kept, up to 400 entries. If a very busy calendar goes over that, the sync tells you some events were left out.

## Limits

- Times ending in `Z` (UTC) are shifted by the time zone set on the reader (Settings > Clock). Times that name another time zone are taken as already being local, which is right for a calendar kept in your own time zone.
- Only one calendar is supported. Read-only; you cannot add events from the reader.
- On the original X4 (no clock), set the date in the Desk Calendar first (Options > Set Date) so the reader knows what "today" is.
- Text is shown in the reader's UI font, so a title in a script that font lacks may show gaps.

## Day View

**Options > Day View** shows the selected day full screen: every event on its own row, the start time on the left with the length under it ("1h 30m", when the event has an end time on the same day) and the whole title (up to four lines) beside it. Left and Right go to the previous and next day, the side buttons scroll a long day, a touch reader can swipe, and Back returns to the month, which follows the day you ended on.

## Calendar Day sleep screen

Settings > Sleep Screen > **Calendar Day** shows the whole of today in the Desk Calendar's window: each event with its start time, length and title. On a reader with a clock, events that have finished are struck through, the one under way has a bar beside it and a bold title, and the bottom line says when the screen was drawn ("Updated 2:14 PM"). An event with no end time counts as an hour long, and all-day events are never struck through. If the day has more events than fit, it starts at the first unfinished one and ends with "+N more".

On the original X4, which has no clock, it shows the day you set in the Desk Calendar (Options > Set Date) with nothing struck through and no update time. If no calendar has been synced yet it says so.
