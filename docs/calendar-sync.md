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
- **Desk Calendar sleep screen**: the same dots, plus the next few events.

Repeating events (daily, weekly, monthly, yearly, with days of the week, "second Thursday", end dates, and skipped dates) are expanded, and moved or cancelled single occurrences are respected. About 45 days back and 13 months ahead are kept, up to 400 entries. If a very busy calendar goes over that, the sync tells you some events were left out.

## Limits

- Times ending in `Z` (UTC) are shifted by the time zone set on the reader (Settings > Clock). Times that name another time zone are taken as already being local, which is right for a calendar kept in your own time zone.
- Only one calendar is supported. Read-only; you cannot add events from the reader.
- On the original X4 (no clock), set the date in the Desk Calendar first (Options > Set Date) so the reader knows what "today" is.
- Text is shown in the reader's UI font, so a title in a script that font lacks may show gaps.

## Day View

**Options > Day View** shows the selected day full screen: every event on its own row, the time on the left and the whole title (up to four lines) beside it. Left and Right go to the previous and next day, the side buttons scroll a long day, a touch reader can swipe, and Back returns to the month, which follows the day you ended on.
