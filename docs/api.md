# HTTP API

The local API is available at `http://192.168.4.1` on the always-on open `TV-Remote-XXXX` AP, and at `http://tv-remote.local` or the station IP when home WiFi is configured. The ESP32 makes no cloud requests or automatic router port mappings. All API success and error bodies are JSON, except the downloadable JSON export. Requests must originate in the AP subnet or the ESP32's home-WiFi subnet.

State-changing routes are open by default to match the open AP setup. **Settings** or `PUT /api/security` can set an optional 8–64 character administrator password. Once set, all non-GET requests use HTTP Basic authentication with user `admin`. ArduinoOTA uses a separate random password printed on serial. Neither password nor WiFi credentials appear in `GET` responses.

## Endpoints

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/status` | Firmware, active profile, AP/STA connection, IR queue, RX, uptime, heap |
| GET | `/api/logs` | Last 48 RAM-only WiFi, IR and OTA events, oldest first; no credentials or OTA password |
| GET / PUT | `/api/network` | Read AP/STA state; set `{ "ssid": "...", "password": "..." }` for 2.4 GHz home WiFi; empty SSID and password forget it |
| GET / PUT | `/api/security` | Read `admin_password_set`; set `{ "password": "..." }`, empty password removes authentication |
| GET | `/api/candidates` | Curated Finder catalog plus saved per-command Yes/No feedback |
| POST | `/api/candidates/{id}/test` | Queue exactly one safe command using `{ "command": "volume_up" }` |
| POST | `/api/candidates/{id}/feedback` | Save `{ "command": "volume_up", "worked": true }` |
| DELETE | `/api/candidates/{id}/feedback` | Clear all saved test feedback for one candidate |
| POST | `/api/candidates/{id}/activate` | Create/select an editable remote from a candidate after two different positive results |
| POST | `/api/import/flipper` | Import `{ "id": "my_remote", "device": "My Remote", "content": "Filetype: IR signals file\\n..." }` |
| POST | `/api/config/auto-layout` | Rebuild button pages for the active Flipper import from saved `source_name` values; preserves its IR commands |
| GET / POST | `/api/remotes` | List saved profiles, including `configured_commands` for each / create and activate a complete JSON profile |
| POST | `/api/remotes/active` | Select with `{ "id": "toshiba_tv" }` |
| DELETE | `/api/remotes/{id}` | Delete a profile; at least one must remain |
| GET | `/api/commands` | Active profile commands, protocol, configured state and hold interval |
| POST | `/api/commands/{name}/send` | Queue one active-profile IR command. Optional `X-IR-Held: 1` preserves RC5/RC6 toggle for held repeats. |
| GET / PUT | `/api/config` | Read / validate and replace the active profile; its ID cannot change |
| GET | `/api/config/export` | Download the active profile JSON |
| POST | `/api/config/import` | Replace the active profile with JSON having the same ID |
| POST | `/api/learn/start` | Start a 20-second capture window with `{ "command": "power" }` |
| GET | `/api/learn/status` | Capture state and candidate signal |
| POST | `/api/learn/commit` | Persist the captured signal in the active profile |
| POST | `/api/restart` | Schedule a restart |

Example manual Finder test:

```sh
curl http://192.168.4.1/api/candidates
curl -H 'Content-Type: application/json' \
  -d '{"command":"volume_up"}' \
  http://192.168.4.1/api/candidates/toshiba_ct8563/test
```

The test response is `202` and reports queue acceptance; it cannot confirm that the TV reacted. Record the result only after observing the TV:

```sh
curl -H 'Content-Type: application/json' \
  -d '{"command":"volume_up","worked":true}' \
  http://192.168.4.1/api/candidates/toshiba_ct8563/feedback
```

After two different successful keys, `POST /api/candidates/toshiba_ct8563/activate` creates and activates a saved Toshiba-layout remote. Its `origin` records the source and confirmed command names. Full compatibility remains unverified. The normal remote then uses this active profile:

```sh
curl -X POST http://192.168.4.1/api/commands/volume_up/send
```

Accepted command response:

```json
{"success":true,"command":"volume_up","queued":true}
```

Unknown commands return `404`; known but unconfigured commands return `409`; invalid import/test data returns `400`; more than one accepted IR command in 120 ms returns `429`; a full four-item queue returns `503`. Error bodies have `{"success":false,"error":"..."}`. IR transmission is serial, never overlapping. Finder tests allow only manually selected volume, mute, navigation, home/back, input, channel, menu and exit commands. No automatic scan exists.

`GET /api/logs` returns `{ "uptime_ms": ..., "capacity": 48, "events": [...] }`. Each event has an incrementing `id`, `uptime_ms`, `category` and `message`. IR events include the command name, protocol, encoded value, bit count, carrier and elapsed transmission time. The history is held in RAM and clears on reboot. It records what the ESP32 sent, not what the TV received. `/api/status` also reports the home-WiFi IP and the password-protected OTA port (3232). See [Remote diagnosis and OTA](remote-diagnostics.md).

## Profiles and persistence

Each saved remote is `/remotes/{id}.json` in LittleFS. `id` uses lowercase letters, digits and underscores, up to 32 characters. Required fields are `id`, `device`, `default_frequency` (30–60 kHz), `repeat_interval_ms` (120–1000), `receiver_enabled`, `pages`, and `commands`. A command is a protocol plus encoded hex `code` and `bits`, or `RAW` with alternating mark/space `timings` in microseconds. `frequency` may be set on each command; RAW can also have a fractional `duty_cycle` from a Flipper file. Encoded protocols are `NEC`, `SONY`, `RC5`, and `RC6`; RC5/RC6 use 36 kHz and have per-press toggle handling. An encoded `code: null` or empty RAW timings means unconfigured.

Pages contain groups, and groups contain buttons that reference command names. A profile may have 1–6 pages, 1–12 groups per page, up to 96 commands and visible buttons, and at most 300 RAW timings / 300 ms per command. The device holds up to eight profiles. Profile updates use a temporary file and backup. The active ID is stored in NVS; Finder feedback uses `/candidate_feedback.json`. A regular firmware upload preserves both. A full flash erase, changed partition table or explicit `uploadfs` can remove LittleFS contents. Download each profile with `/api/config/export` before such operations.

The Flipper importer accepts version-1 `.ir` parsed NEC, NECext, Samsung32, Sony SIRC/SIRC15, RC5 and RC6 mode 0, plus RAW. It preserves original signal names and parsed address/command metadata. Known signal names are mapped to readable labels and remote-style groups; unknown names stay visible under Other Keys. `POST /api/config/auto-layout` returns `409` for non-Flipper profiles. Unsupported protocols fail rather than being sent with a guessed encoder. See [Finder and source notes](code-finder.md) for conversion and layout details. LIRC, IRDB CSV, Pronto and Broadlink are not currently parsed.

The optional receive endpoints require a compatible 3.3 V IR receiver and a build with `IR_ENABLE_RX=1`. The default XIAO build ships with RX disabled. A captured unknown protocol is stored as RAW at the configured default frequency, which may need correction after testing.
