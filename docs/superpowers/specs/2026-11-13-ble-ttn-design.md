# BLE connection events to TTN

The user authorized enabling BLE and accepts a device identifier plus a
"connected" event as the initial bridge. An iPhone is the first client.

## Approach

Make KG100S a connectable BLE peripheral named `OpenSidewalk`. A successful
connection queues a compact event; the existing main loop forwards it through
the configured LoRaWAN device. This allows testing from a BLE scanner app
without a custom iPhone app. Reading arbitrary existing sensor profiles would
require sensor-specific client code and is outside this first version.

As requested in v0.4.1, four clients are supported simultaneously. Advertising
continues while capacity remains and resumes when a connection slot is recycled.
The peer's BLE address is the identifier; a private address is not a persistent
device identity and the phone's friendly name is not automatically available.

## Wire format and scheduling

Use FPort 3 and an 11-byte payload: schema 1, event 1 (`connected`), a
big-endian 16-bit sequence, address type, and the six address bytes in display
order. Sequences start at 1 each boot and wrap. The TTN decoder returns
`event`, `sequence`, `ble_address`, and `ble_address_type`.

Queue eight events in RAM, drop the newest when full and expose that count.
Send one event at a time after OTAA join, retry failures without removing the
head, and space attempts by 30 seconds. Unconfirmed uplinks are best effort,
not proof of cloud delivery. A reset discards pending events. Continue the
existing FPort 2 telemetry. BLE callbacks never wait for LoRaWAN operations.

## Constraints and verification

Keep the provisioned DevEUI, keys, channel plan, nonce persistence, NVS layout,
and factory USERDATA. Flash application addresses only, without mass erase.
Run host tests for event framing, queue overflow/FIFO/retry semantics, decoder
validation and existing behavior. Build for the pinned Zephyr board, check
memory/image boundaries, and verify advertising, a real BLE connection and a
LoRaWAN event on the hardware. Confirm TTN receipt when its console is available.

Public docs and artifacts contain no credentials or hardware-specific IDs.
