# Release validation — v0.3.1

Validated November 13, 2026 on a KG100S BRD4332A / BRD4002A mainboard.
Device identities, keys, gateway details and raw logs are deliberately omitted.

- Pinned Zephyr v4.5.0 / SDK 1.0.1 / GCC 14.3.0 build: PASS.
- Memory: 124,076 bytes FLASH, 26,624 bytes RAM. Application image ends at
  `0x0001e4ab`, below NVS at `0x000f8000`. HEX records/checksums and binary
  content agree; no NVS or USERDATA addresses are present.
- Host tests: PASS for C/JavaScript telemetry compatibility, signed Celsius and
  Fahrenheit, button fields, missing temperature, invalid frames, automatic
  join timing/backoff, shell/autojoin interleavings, RF-switch register fields,
  unrelated-bit preservation, and restoration after simulated sleep.
- Public RF helper: implemented against Semtech datasheet rev 2.2 section 8.6;
  DIO3 boot readback is `0x0e`, the intended 3 V/high configuration.
- Hardware flash of this release: PASS without mass erase. Saved profile was
  retained; a fresh nonce was reserved, OTAA accepted automatically, and the
  first temperature/button sample was transmitted. MAC downlink was received.
  No manual join command was issued.
- Automatic join began at about 3 seconds, was accepted at about 9 seconds,
  and telemetry completed at about 14 seconds after boot.
- Prior v0.3.0 verification established software-reset and real cold-power
  automatic joining, TTN decoded telemetry, physical button press counts and
  held/released states. Those application behaviors are unchanged in v0.3.1.
- Factory USERDATA is retained; no bootloader, Secure Element, debug-lock or
  adapter-firmware update is part of this release.
- Public privacy checks: no credentials, factory backups, NVS/USERDATA dumps,
  user filesystem paths, private TTN screenshots/logs or debugger ELF artifacts.
  The supplied photograph has identifier areas redacted and no PNG metadata.
- Independent read-only review: host tests, image boundaries, BIN/HEX agreement,
  datasheet register fields, notices and photograph inspected. A truncated Zlib
  notice was corrected before release. No unresolved important findings.

A real gateway-outage retry cycle was not exercised; retry timing/gates are
covered by host tests. The longer idle periodic interval and packet-loss retry
are not a production qualification. The firmware is an experimental Class A
prototype, has not completed LoRaWAN certification, and measures MCU die
rather than calibrated ambient temperature. Firmware hashes are in
`firmware/manifest.json` and `firmware/SHA256SUMS`.
