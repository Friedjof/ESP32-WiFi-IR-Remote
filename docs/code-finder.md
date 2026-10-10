# IR Code Finder and source notes

The Toshiba 43UL2163DAY is identified on the owner's label as a Vestel-built Toshiba television. No infrared profile for the exact model was found in the reviewed public catalogs. A replacement-remote seller [lists CT-8543 as compatible](https://www.freetv.ie/toshiba-ct-8543-tv-remote-control/), but that listing does not publish IR timings and is not a test on this particular television. The [official product page](https://toshiba-tv.com/de-en/43ul2163day) lists a remote and Smart Center, but does not identify its IR protocol. The owner's physical original remote is the best way to settle the question.

The bundled catalog is a **small, attributed set of hypotheses**, not a firmware database of confirmed Toshiba codes. All entries have `unverified` or `unverified_for_target` status. Their source URL, model, protocol, carrier, command count and notes are visible in the Finder. Codes are in [candidates.json](../data/candidates.json), and the build embeds that file in the firmware. A normal firmware update does not replace saved test results or remotes.

The profile picker also contains three coded, ready-to-try examples sourced from individual Flipper-IRDB files: [Samsung UE32F4000 / AA59-00741A](https://github.com/Lucaslhm/Flipper-IRDB/blob/main/TVs/Samsung/Samsung_UE32F4000.ir), [LG C1](https://github.com/Lucaslhm/Flipper-IRDB/blob/main/TVs/LG/LG_C1.ir), and [Sony RMT-TX100D](https://github.com/Lucaslhm/Flipper-IRDB/blob/main/TVs/Sony/Sony_RMT_TX100D.ir). Each saved command retains the source's protocol, address and command beside its converted transmission code. The names identify the source remote, not a compatibility guarantee for every TV from that manufacturer. None has been tested on the owner's Toshiba. The Toshiba and media-player profiles remain blank templates; selecting one of them displays a direct link to Finder.

| Candidate | Public source | Evidence and limit |
|---|---|---|
| User-supplied NEC values | Values supplied with the development request | Four 32-bit values, no independently verified origin; deliberately separate from the catalog-derived NEC sets. |
| Toshiba CT-8563 | [Flipper-IRDB `.ir`](https://github.com/Lucaslhm/Flipper-IRDB/blob/main/TVs/Toshiba/Toshiba_Ct-8563.ir) | RC5 address `0x01`; the six RC5 values suggested in the request match this file. No CT-8543/43UL2163DAY compatibility proof. The file lists digit 7 and 8 with the same command; those digits were not bundled. |
| Toshiba CT-90325 | [Flipper-IRDB `.ir`](https://github.com/Lucaslhm/Flipper-IRDB/blob/main/TVs/Toshiba/Toshiba_CT-90325.ir) | NEC address `0x40`, volume commands `0x1A`/`0x1E`, input `0x0F`; its volume values differ from the user-supplied `0xA0`/`0xF0`. |
| Toshiba CT-8560 | [Flipper-IRDB `.ir`](https://github.com/Lucaslhm/Flipper-IRDB/blob/main/TVs/Toshiba/Toshiba%20CT%208560.ir) | NEC address `0x40`, with some navigation and input values different from CT-90325. |
| Generic Vestel TV | [Flipper-IRDB converted `.ir`](https://github.com/Lucaslhm/Flipper-IRDB/blob/main/_Converted_/CSV/V/Vestel/Unknown_TV/0,-1.ir) | Generic `NECext` record. The source's volume labels are ambiguous, so neither volume direction is assigned. This is not evidence of compatibility with the target. |

The [Flipper-IRDB tree](https://github.com/Lucaslhm/Flipper-IRDB) and [IRDB tree](https://github.com/probonopd/irdb) did not contain files named CT-8543 or 43UL2163DAY when reviewed. [RemoteCentral's Toshiba index](https://www.remotecentral.com/cgi-bin/codes/toshiba/) also did not identify the target. These are search results, not proof that a profile does not exist elsewhere. The bundled Flipper source files were committed before that repository's [CC0 cutoff for newer submissions](https://github.com/Lucaslhm/Flipper-IRDB#license); review source-data permissions before redistributing larger copies. This project bundles only a small attributed set of command facts, not those source files.

## Why the NEC hex values differ

Flipper stores parsed `address` and `command` as four **little-endian bytes** and its NEC encoder sends the bits least-significant-first. `IRremoteESP8266::sendNEC` takes a packed value that it sends most-significant-first. The importer reverses each field's bits and constructs the complete 32-bit signal. For example, Flipper NEC address `40 00 00 00`, command `12 00 00 00` becomes `0x02FD48B7` for `sendNEC`. Copying `0x40BF12ED` into that slot would send a different waveform. `NECext` is also converted from its 16-bit address and 16-bit command, without inventing inverse bytes. These rules follow the [Flipper NEC encoder](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/lib/infrared/encoder_decoder/nec/infrared_encoder_nec.c) and [IRremoteESP8266's NEC implementation](https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_NEC.cpp).

RC5 candidates use 12 data bits (toggle, five address, six command). The transmitter flips the toggle for each new key press and keeps it for held repeats, following [IRremoteESP8266's RC5 implementation](https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_RC5_RC6.cpp). RC5 and RC6 use 36 kHz. NEC and RAW use the frequency in the command/profile; Flipper RAW duty cycle is preserved and used when transmitting. A protocol's repeat behavior can be more involved than a repeated HTTP request, so test held keys on the real device.

## Testing safely

1. Open **Finder**, choose Toshiba or Vestel and a candidate. Start with **Volume Up** (or Mute/navigation if that profile has no volume key).
2. Press **Send one test** once while pointing the LED toward the TV. The API queues exactly one command. No automatic scan runs.
3. Answer **Yes** or **No** based on the TV. Feedback is stored in LittleFS. A Yes result confirms only that key on the current TV setup.
4. Confirm at least two different keys before **Save as remote** becomes available. This copies the candidate codes into a separate editable Toshiba-layout profile and activates it. Untested keys remain unverified. The original blank Toshiba profile remains available.
   Use **Reset results** to clear this candidate's test history and start again.
5. Use **Settings → Download profile** before full erase or a LittleFS upload. The active remote ID is stored in NVS; profiles and Finder feedback are stored in LittleFS.

The Finder only allows one manually selected volume, mute, navigation, home/back, input or channel/menu/exit command per request. Power, service, factory-reset and other potentially disruptive keys are excluded from Finder testing. The normal remote can still send Power after a profile is activated. A shared 120 ms acceptance limit and a four-item queue protect both endpoints.

## Importing and learning

The browser can import a Flipper `.ir` file. The ESP32 parses `parsed` NEC, NECext, Samsung32, Sony SIRC/SIRC15, RC5 and RC6 mode 0 and `raw` signals, preserving Flipper address, command, original name, frequency and RAW duty cycle metadata. Unsupported protocols, duplicate normalized key names, malformed lines and signals exceeding 300 timings or 300 ms fail with an explicit error. A file creates a new saved remote; it never silently overwrites the active one. The [official Flipper format](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/documentation/file_formats/InfraredFileFormats.md) is the format reference. LIRC `.conf`, IRDB CSV, Pronto and Broadlink are **not yet imported**; a converted Flipper `.ir` or validated profile JSON can be used instead. The repository has no online catalog or runtime Internet download: import a file from your phone, then use it fully offline.

### Button names and layout

The official `.ir` format stores a free-form `name` for each signal; it contains no screen position or button category. The [Flipper Universal Remotes guide](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/documentation/UniversalRemotes.md) defines the TV library names `Power`, `Mute`, `Vol_up`, `Vol_dn`, `Ch_next`, and `Ch_prev`. [Flipper-IRDB's naming guide](https://github.com/Lucaslhm/Flipper-IRDB#naming-scheme) recommends additional names for navigation and media controls. The importer uses these conventions as layout hints:

| Source names | Display and placement |
|---|---|
| `Vol_up`, `Vol_dn` and common volume variants | `Vol +`, `Vol -` in Volume; hold to repeat |
| `Ch_next`, `Ch_prev` and common channel variants | `CH +`, `CH -` in Channel; hold to repeat |
| `Up`, `Down`, `Left`, `Right`, `Ok` / `Enter` | Direction pad with OK in the centre |
| `Power`, `Input` / `Source`, `Mute`, `Home`, `Back` / `Return` | Main, Volume and System groups |
| `Play`, `Pause`, `Stop`, `Next`, `Prev` | Mute, Play, Pause and Stop sit below the TV direction pad when available; extra media controls go to More. Audio-only profiles keep media on Remote. `Next` and `Prev` mean tracks, not channels. |
| `0`–`9`, `Red`, `Green`, `Yellow`, `Blue` | Number pad and colour keys on More |

TV-like files with direction, volume and channel keys use the familiar Power/Source, system, side rocker and direction pad arrangement. Other names remain visible in an Other Keys group with their original `source_name` stored alongside the IR data. The mapping does not assert that an imported signal works on a particular device. For an already imported Flipper profile, select it and use **Settings → Arrange imported buttons**. That rebuilds only its button layout from saved names; its command identifiers, protocols and IR codes stay intact. Download a backup first if you manually customized the layout.

An optional 3.3 V IR receiver can capture a real remote. The default XIAO build disables RX; consult [IR learning](ir-learning.md) before enabling it. An unknown decoded protocol falls back to RAW timing capture. A demodulating receiver does not measure the original carrier precisely, so check the stored frequency when using RAW.

## Possible TV control over home WiFi

The [official model sheet](https://toshiba-tv.com/de-en/pdf/43ul2163day) lists Toshiba TV Smart Center. An independent [open-source Vestel/Toshiba SmartCenter client](https://github.com/gokutou/vestel-smartcenter-remote-ios) documents local HTTP XML commands at `http://TV_IP:56789/apps/SmartCenter` when the TV's Virtual Remote option is enabled. Another [open-source client](https://github.com/SmolSoftBoi/vestel-tv-api) uses local discovery and SmartCenter commands. This protocol has **not been tested on the 43UL2163DAY**, and it is not part of this IR firmware. It may be useful later if that TV exposes the service on the local network.
