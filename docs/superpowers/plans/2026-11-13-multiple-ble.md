# Multiple BLE connections

The user requested simultaneous BLE connections and a separate TTN connection
event for each peer. Implement four connections as the initial capacity.

Retain the existing 11-byte FPort 3 format, eight-event FIFO, 30-second attempt
spacing, FPort 2 telemetry, OTAA credentials and monotonic nonce storage.

Implementation:

- [x] Test real bridge callbacks with four distinct peers, continued advertising
  below capacity, stopping at capacity, freeing/reusing a slot, failed connection
  setup and independently encoded queued events.
- [x] Track successfully connected slots atomically and derive the count; schedule advertising
  after successful connections and through recycled after disconnection.
- [x] Set four controller/host slots and adequate controller buffer; build and
  verify memory/image boundaries.
- [x] Back up v0.4.0 privately and flash application-only v0.4.1. Verify retained
  TTN identity, advertising while a real Mac connection is held, and event TX.
- [x] Update instructions/manifest, run full tests and release checks, and review
  the final source change independently.

One Mac central can exercise one real peer identity. A second physical central
(such as the user's iPhone) is needed for independent simultaneous-peer hardware
validation. Host tests exercise all four callback slots and event identities.

Independent review identified a rapid failed-connection/disconnect sequence
that could decrement an uncounted peer. The regression failed against the
first counter implementation, then passed with per-slot tracking. Full suite
passes. Final build: 284,852 FLASH bytes and 46,448 RAM bytes; HEX/BIN agreement
and NVS/USERDATA exclusion pass. Controller configuration has four slots and
12,288 bytes of controller buffer.

Hardware result: Mac GATT/connection succeeded while 61 fresh advertisements
were observed. User supplied an iPhone during validation; repeated live status
showed 2/4 simultaneous peers. Their distinct connection events (sequences 3
and 4) were decoded in TTN. Queue drained, no drops. Mac disconnected afterward,
leaving the iPhone connected at 1/4. Four physical centrals were not available;
four-slot lifecycle is covered by the host regression tests.
