# Learning IR codes

1. Connect a 3.3 V compatible 38 kHz demodulating receiver. On the classic ESP32 DevKit, use GPIO27. On the ESP32-C3, select a free valid pin (GPIO3 is configured as an example), set `IR_ENABLE_RX=1` in `platformio.ini`, and rebuild.
2. Select the remote profile, open **Settings**, change `receiver_enabled` to `true` in its JSON and save.
3. Choose a command and press **Learn IR code**.
4. Point the original remote at the receiver and press the corresponding key once within 20 seconds.
5. Review the decoded protocol, hexadecimal value and bit count, or RAW timing array for unknown protocols.
6. Press **Save captured code**. Then export a backup.

The capture uses [IRremoteESP8266's receive and raw conversion functions](https://github.com/crankyoldgit/IRremoteESP8266/tree/master/examples/IRrecvDumpV3). If a protocol is not among the implemented senders, the full RAW timing array is saved instead. The receiver cannot measure carrier frequency from a demodulated signal, so RAW captures assume 38 kHz; adjust the frequency in JSON if the original remote uses another carrier. Signals longer than 300 timings are rejected. Some remotes vary codes for held keys or use multi-frame protocols; validate learned codes against the actual TV.

The captured value is a preview until committed. A restart before **Save captured code** discards it. Saved codes are stored in the active profile's LittleFS file and survive a normal firmware OTA update. Export each profile before `uploadfs`, which replaces the file-system image.
