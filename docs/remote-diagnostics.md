# Remote diagnosis and OTA

The XIAO ESP32C3 can stay near the TV on USB power while the phone and laptop use home WiFi. Its open `TV-Remote-XXXX` access point remains active as a fallback. The default IR output is D10/GPIO10; moving the board does not change its pin or saved remote profile.

## Prepare before unplugging USB

1. Flash the current firmware once over USB with `make flash 0`. Open `http://192.168.4.1/` while connected to the ESP32 access point and confirm the firmware version in **Status**.
2. In **Settings → Home WiFi and access**, enter the 2.4 GHz home-network SSID and password. These stay in the ESP32's NVS and are not part of the repository. **Status** must show `station_connected: true` and a `station_ip`. Open `http://tv-remote.local/` or `http://STATION_IP/` from a device on the same home network before moving the XIAO. Use the IP if mDNS does not resolve.
3. Connect USB and run `make monitor 0` at 115200 baud if you need serial diagnostics. OTA currently uses no password. A normal USB firmware upload does not erase saved remote profiles or home-WiFi settings.
4. Download a backup of the active remote from **Settings → Download profile**. A normal firmware upload preserves profiles, but a full flash erase or `uploadfs` can remove them.
5. Supply the board and IR circuit with suitable power near the TV. Keep the ESP32 and LED supply grounds connected and the external WiFi antenna attached.

## Watch IR tests remotely

Open the remote at the home-WiFi address and select **Status → Live device log**. It refreshes while Status is visible. Each IR event lists the sent command, protocol, encoded code, bit count, carrier frequency, tap/held state and time spent transmitting. The log also shows home-WiFi connection and OTA events. `GET /api/logs` provides the same data to scripts. It retains only the latest 48 events in RAM and resets when the ESP32 restarts.

For a controlled TV test, tap one key briefly, wait about two seconds, and compare the TV's response with the new `Sent ...` event. Holding a repeatable key for more than 440 ms starts repeated HTTP requests. A `Sent` event confirms that the firmware completed its send call; an IR receiver or oscilloscope is needed to inspect the emitted signal, and the TV response must still be observed directly.

On the XIAO ESP32C3, 32-bit NEC commands use the chip's RMT peripheral for the carrier and pulse timings. The log marks these events `via RMT`; other protocols use the IR library's software transmitter and are marked `via software`. RMT generates its waveform in hardware while WiFi runs, removing software scheduling from the NEC timing path. It does not measure LED current or prove that the TV receives the signal. The transmitter also calibrates its software carrier once at boot for the other protocols. The 65 ms calibration burst contains no remote command.

After updating, confirm **Status** reports firmware `1.5.5` and one short NEC tap produces a `via RMT` event. Try Power or a harmless navigation key with separate taps, about two seconds apart, while keeping LED position unchanged. If reliability does not improve, compare the original remote and ESP output with a 38 kHz IR receiver or oscilloscope; the remaining candidates are optical power, receiver saturation from an LED placed too close, and profile compatibility. A phone camera only shows that the LED emits light, not its 38 kHz modulation or decoded command.

## Update firmware over home WiFi

The laptop and ESP32 must be on the same reachable local network. ArduinoOTA listens on port 3232 without a password. Update with:

```sh
make ota OTA_HOST=192.168.1.123
```

Replace the example address with the `station_ip` shown in Status. `OTA_HOST=tv-remote.local` also works when mDNS resolves. The Makefile builds the XIAO ESP32C3 firmware and uses PlatformIO's `espota` uploader. Wait for the upload and restart, then reload **Status** and confirm the new firmware version. If the home router isolates wireless clients or a laptop firewall blocks the OTA callback, put the laptop on the ESP32 access point and run the same command with `OTA_HOST=192.168.4.1`.

OTA changes the firmware partition only. It does not upload the LittleFS image or replace saved remote profiles. Keep USB access available for recovery if a firmware build stops joining WiFi or serving OTA.

Anyone who can reach port 3232 on the open AP or home network can upload firmware while password-free OTA is enabled. The optional web-admin password does not protect OTA. Connect the XIAO to the laptop with a USB data cable and run `make flash 0` if network updating fails. Do not run a full flash erase or `uploadfs` when preserving saved profiles.
