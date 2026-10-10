# HTTP API

Connect to the open `TV-Remote-XXXX` WiFi network first. Base URL: `http://192.168.4.1` (or `http://tv-remote.local` where mDNS resolves). The captive sign-in page opens the remote automatically on supported phones. All API responses are JSON. Requests from outside the AP subnet receive `403`.

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/status` | Firmware, active remote ID, WiFi, IR, receiver, uptime and heap |
| GET | `/api/remotes` | List stored profiles and the active ID |
| POST | `/api/remotes` | Validate, create and activate a new profile from a complete JSON object |
| POST | `/api/remotes/active` | Select a profile with body `{"id":"media_player"}` |
| DELETE | `/api/remotes/{id}` | Delete a profile; at least one must remain |
| GET | `/api/commands` | Commands in the active profile, protocol, configured state and repeat interval |
| POST | `/api/commands/{name}/send` | Queue one IR transmission from the active profile |
| GET | `/api/config` | Read the active profile's complete JSON |
| PUT | `/api/config` | Validate and replace the active profile; its ID cannot change |
| GET | `/api/config/export` | Download the active profile as JSON |
| POST | `/api/config/import` | Replace the active profile from JSON with the same ID |
| POST | `/api/learn/start` | Start a 20-second capture window for an active-profile command |
| GET | `/api/learn/status` | Read capture state and candidate code |
| POST | `/api/learn/commit` | Save the captured code in the active profile |
| POST | `/api/restart` | Schedule a restart |

List profiles and switch:

```sh
curl http://192.168.4.1/api/remotes
curl -H 'Content-Type: application/json' -d '{"id":"media_player"}' \
  http://192.168.4.1/api/remotes/active
```

Send a configured command:

```sh
curl -X POST http://192.168.4.1/api/commands/volume_up/send
```

Accepted response, HTTP `202`:

```json
{"success":true,"command":"volume_up","queued":true}
```

This acknowledges queueing, not confirmation that the device received the signal. Unknown commands return `404`; known but unconfigured commands return `409`; requests faster than 120 ms return `429`; a full queue returns `503`. Error responses have `{"success":false,"error":"..."}`. The transmitter processes one queued item at a time with a four-command queue.

Each profile is one JSON file in `/remotes/{id}.json` on LittleFS. `id` uses lowercase letters, numbers and underscores (up to 32 characters). Required fields: `id`, `device`, `default_frequency`, `repeat_interval_ms`, `receiver_enabled`, `pages`, and `commands`. Every page has `id`, `label` and `groups`; every group has `label`, `columns` (1–4), and `buttons`. Every button has a valid `command` and `label`. Optional button fields: `icon`, `tone`, `repeat`, `span`, `column`, `row`. The command must exist in `commands`. A profile may have 1–6 pages, up to 12 groups per page, up to 96 commands and 96 visible buttons. The device stores up to eight profiles.

Supported IR protocols: `NEC`, `SONY`, `RC5`, `RC6`, `RAW`. For encoded protocols, `code` is a hexadecimal string (`0x` prefix optional) or `null`, and `bits` is 1–64. RAW uses `timings` as alternating microsecond durations and `frequency` in hertz. Empty timings mean unconfigured. Invalid profile data returns `400` and leaves the existing profile active. The active profile ID is persisted in NVS. A regular firmware upload does not replace saved profiles.

To create a profile, take a copy of an existing profile, change `id` and `device`, clear its codes if appropriate, and POST the entire JSON to `/api/remotes`. A new profile becomes active. To import over the active profile, use `/api/config/import` with the same ID. Download each profile with `/api/config/export` before a full flash erase or LittleFS upload.

Learn a code:

```sh
curl -H 'Content-Type: application/json' -d '{"command":"power"}' -X POST http://192.168.4.1/api/learn/start
curl http://192.168.4.1/api/learn/status
curl -X POST http://192.168.4.1/api/learn/commit
```

`GET /api/learn/status` reports `idle`, `waiting`, `captured`, `timeout`, `signal_too_long`, `capture_failed`, or `saved`. A captured response includes a `candidate` object. If the receiver is disabled, start returns `409`.

The AP is open, so anyone within radio range can use this API. ArduinoOTA has a separate password printed on serial.
