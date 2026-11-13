/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BLE_EVENT_H
#define BLE_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#define BLE_EVENT_SIZE 11
#define BLE_EVENT_CAPACITY 8

/* Caller serializes access. Events stay queued until a successful send. */
struct ble_event_queue {
	uint8_t payloads[BLE_EVENT_CAPACITY][BLE_EVENT_SIZE];
	uint8_t head;
	uint8_t count;
	uint16_t sequence;
	uint32_t dropped;
	uint32_t sent;
};

bool ble_event_push(struct ble_event_queue *queue, uint8_t type, const uint8_t address_le[6]);
bool ble_event_peek(const struct ble_event_queue *queue, uint8_t out[BLE_EVENT_SIZE]);
void ble_event_complete(struct ble_event_queue *queue, bool success);

#endif
