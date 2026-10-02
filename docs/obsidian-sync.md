---
title: Obsidian Clipping Sync
nav_order: 7.6
---

# Obsidian Clipping Sync

Obsidian Clipping Sync pushes the highlights you save while reading (the same
ones written to `/My Clippings.txt`) into an Obsidian vault, without a
computer in between.

Obsidian doesn't offer a public sync protocol, so RetroInk sends highlights
to one of two places:

- **[Local REST API with MCP](https://github.com/coddingtonbear/obsidian-local-rest-api)
  plugin** (recommended), built by Adam Coddington. It runs an HTTPS API on
  your phone or computer, inside your vault. RetroInk posts each new
  highlight straight into a note in your vault over your local Wi-Fi
  network. No cloud account, no internet dependency: whatever syncs your
  vault already (Obsidian Sync, iCloud, Syncthing, Git) carries it from
  there.
- **Generic webhook**. RetroInk POSTs a JSON payload to any URL you give it.
  Point this at n8n, Make, Home Assistant, or a small script of your own if
  you want clippings routed somewhere the Local REST API plugin can't reach
  (e.g. over the internet, or into a different note-taking app).

## How it works

Every highlight you save in the reader is queued locally (a small file on the
SD card) as soon as it's saved to `/My Clippings.txt`. No network activity,
no delay in the reader. Nothing is sent until a sync runs. Sync is:

- **Manual**, any time, from the web UI's Settings page ("Sync Now"), or
- **Automatic when File Transfer / Calibre Wireless mode starts**, if you
  turn that on. Wi-Fi is already up then, so this is the only way to get
  clippings synced without spending extra battery keeping Wi-Fi on while you
  read.

### Older highlights

Only highlights saved while sync is set up and enabled are queued. Anything
saved before that, including highlights carried over from CrossInk, stays in
the reader but isn't sent. To send those too, press **Queue Older
Highlights** in the Obsidian Clipping Sync card, then **Sync Now**. It queues
every stored highlight that hasn't been sent or queued already, so pressing it
again is safe. RetroInk only started recording what it has sent in 0.4.6, so
highlights synced before then may be sent a second time the first time you
use it.

A sync stops at the first clipping it can't deliver, so nothing already sent
is lost and nothing unsent is skipped. Whatever didn't go through stays
queued for the next attempt, unless it's now failed 5 times in a row: at
that point it's almost certainly a permanent rejection rather than a
transient hiccup (a malformed clipping, a rule on the receiving end), so
RetroInk gives up on that one clipping specifically, logs it to
`.crosspoint/obsidian-failed.jsonl` on the SD card, and keeps going instead
of letting it block everything behind it forever.

## Setup: Local REST API plugin

1. In Obsidian, install and enable **Local REST API with MCP** (by Adam
   Coddington), from the community plugins list.
2. Open its settings and copy the **API key** and the **HTTPS URL** it shows
   (e.g. `https://192.168.1.50:27124`). The plugin's certificate is
   self-signed, which is why RetroInk defaults to skipping certificate
   verification for this target. You're relying on your Wi-Fi network being
   trusted, not on the certificate.
3. Make sure the device running Obsidian and your RetroInk reader are on the
   **same Wi-Fi network**.
4. On the reader's web Settings page (**File Transfer > Join Network** or
   **Create Hotspot**, then open the shown URL and go to **Settings**), find
   the **Obsidian Clipping Sync** card:
   - Target: **Obsidian Local REST API plugin**
   - Base URL: the HTTPS URL from the plugin. Prefer the host's `.local`
     hostname (e.g. `https://your-mac.local:27124`) over its raw IP: an IP
     assigned by DHCP can change when the host reboots or reconnects to
     Wi-Fi, silently breaking sync until you notice and update it.
   - Fallback URL (optional): tried only if the Base URL can't be reached at
     all. Pairs well with a raw IP here as a backup for a `.local` hostname
     above, or vice versa.
   - API key: the token from the plugin
   - Note path: where clippings land, e.g. `Clippings/{book}.md`. `{book}`
     is replaced with the book's title. Point this at a folder you don't
     otherwise hand-edit; each sync **appends** to the note, and RetroInk
     falls back to creating it with a fresh write the first time a note
     doesn't exist yet.
   - Enabled: on
5. Save, then highlight something and hit **Sync Now**.

## Setup: generic webhook

1. Set Target to **Generic webhook** and Base URL to your endpoint.
2. Optionally set an API key. It's sent as `Authorization: Bearer <key>` if
   present, otherwise omitted.
3. Each clipping arrives as its own POST:

   ```json
   {
     "source": "RetroInk",
     "book": "Book Title",
     "author": "Author Name",
     "chapter": "Chapter 3",
     "page": 42,
     "text": "The highlighted passage.",
     "timestamp": 1732900000
   }
   ```

   Wire that into whatever writes it into your vault: an n8n/Make flow
   calling the Local REST API plugin itself, a Home Assistant automation, or
   your own script.

## Notes and limits

- **Nothing is sent while you're just reading.** Saving a highlight only
  writes to the SD card (same as `/My Clippings.txt` always has). Network
  activity only happens during a sync.
- **The Local REST API target only works on the same local network** as
  whatever's running Obsidian. For sync over the internet, use the webhook
  target and route it yourself.
- **If syncing behaves unexpectedly, check the plugin's documentation.**
  RetroInk adds to an existing note with `POST /vault/<path>`. If the plugin
  answers `404` because the note doesn't exist yet, RetroInk creates it with
  `PUT`. A different plugin version may behave differently.
- Pending clippings are capped at roughly 512 KB on the SD card (many
  thousands of highlights) so a forgotten sync can't grow without bound;
  syncing regularly avoids ever approaching that.
- This feature does not touch or replace `/My Clippings.txt`. That
  Kindle-format export keeps working exactly as before, independent of
  whether Obsidian sync is enabled.

## Security notes

- **"Skip TLS verification" trusts whatever certificate the server presents.**
  This is necessary for the Local REST API plugin's self-signed certificate,
  but it means the reader can't tell your real Obsidian instance apart from
  another device on the same Wi-Fi network impersonating it. This is bounded
  to "someone is already on your trusted network," the same assumption this
  firmware's whole File Transfer web server already makes (it has no
  authentication either). Avoid running File Transfer with Obsidian sync
  configured on untrusted or public Wi-Fi.
- **The reader refuses to sync if a target URL is `http://` (not `https://`)
  while an API key is set**, rather than sending that key in cleartext. If
  you hit this, switch the URL to `https://` or remove the key.
- **The API key is obfuscated on the SD card, not encrypted**, XORed against
  this device's hardware MAC address and base64-encoded, the same scheme
  every other saved password in this firmware uses (Wi-Fi, OPDS, KOReader
  sync). It stops casual reading of the SD card, not a targeted attacker who
  also has the device.

### Who can reach the plugin

The Local REST API plugin can't know which device on your network is your reader, so it accepts connections from every device on the same Wi-Fi. The API key is all that stands between those devices and your vault, and it isn't limited to adding clippings: it gives the same access Obsidian itself has. The plugin has no way to limit a key to one folder or to read-only access, so anyone with a valid key could read, write, and delete anywhere in the vault.

To use it, someone would need to be on your Wi-Fi network and have your API key. The plugin isn't reachable from the internet unless your router forwards its port, so don't set up port forwarding for it.

This is the same trust boundary the File Transfer web server already relies on (it has no login either), extended to the machine running Obsidian.

To reduce the risk, roughly easiest first:

1. **Treat the API key like a password.** Don't paste it anywhere else, don't commit it to a repository, and regenerate it from the plugin's settings if you think it leaked. Do this whatever else you choose.
2. **Only turn on "Auto-sync on File Transfer" if you'll use it.** The plugin only needs to be reachable while a sync is running.
3. **Limit the plugin's port to your reader's IP with a firewall rule on the machine running Obsidian.** This closes the gap without blocking the reader. On macOS, the built-in Application Firewall works by app and can't filter by port and source address, but `pf` (packet filter) can. This example allows one IP and blocks everyone else:

   ```
   # /etc/pf.anchors/obsidian-rest-api, replace 192.168.1.50 with your reader's IP
   block in proto tcp from any to any port 27124
   pass in proto tcp from 192.168.1.50 to any port 27124
   ```

   Reference that anchor from `/etc/pf.conf` and load it with `sudo pfctl -f /etc/pf.conf -e`. `pf` rules don't survive a reboot by themselves, so add a LaunchDaemon to reload them, and confirm the rules are active with `pfctl -s rules`.
4. **Or do the same at your router**, if it supports custom firewall rules (UniFi, pfSense, OPNsense, and ASUS routers running Merlin firmware do). Look for a rule that blocks the port from the local network except from one allowed IP. Avoid your router's blanket "client isolation" or "AP isolation" switch: it isolates every device from every other, including the reader, which would break the feature.
5. **Or accept the risk.** It comes down to someone with bad intent already being on your home Wi-Fi, which is the baseline every other login-free feature on this firmware assumes.

## Troubleshooting

**"Sync target rejected the API key"**

Recheck the API key in Settings. It's stored obfuscated and never round-trips
back to the browser, so a typo requires re-entering it, not just re-saving.

**"Could not reach the sync target"**

Confirm the reader and the Local REST API plugin's device are on the same
Wi-Fi network, and that the Base URL (including port) matches what the plugin
shows. For a webhook target, confirm the endpoint is reachable from your
Wi-Fi network (or the internet, if that's where it lives).

**Clippings pile up in "pending" and never seem to sync**

Auto-sync only fires when File Transfer or Calibre Wireless mode starts, not
continuously in the background. Use **Sync Now** in the web UI at any time to
drain the queue immediately.

**Only some of my highlights synced**

Highlights are queued when you save them, and only while sync is enabled.
Anything from before that, including highlights from CrossInk, needs
**Queue Older Highlights** once, then **Sync Now**. See [Older
highlights](#older-highlights).

**"Sync Now" says some clippings were skipped**

One or more clippings failed to deliver 5 times in a row and were given up
on rather than left blocking the rest of the queue forever. Check
`.crosspoint/obsidian-failed.jsonl` on the SD card (plain JSON, one clipping
per line, with a `reason` field) to see what didn't make it and why.
