/* SPDX-License-Identifier: Apache-2.0 */
#include <string.h>
#include "ble_event.h"

bool ble_event_push(struct ble_event_queue *queue, uint8_t type, const uint8_t address_le[6])
{
	if (queue->count == BLE_EVENT_CAPACITY || type > 3) {
		queue->dropped++;
		return false;
	}
	uint8_t *out = queue->payloads[(queue->head + queue->count) % BLE_EVENT_CAPACITY];
	uint16_t sequence = ++queue->sequence;

	out[0] = 1;
	out[1] = 1; /* Connected. */
	out[2] = sequence >> 8;
	out[3] = sequence;
	out[4] = type & 1; /* Normalize public/random identity address types. */
	for (unsigned int i = 0; i < 6; i++) {
		out[5 + i] = address_le[5 - i];
	}
	queue->count++;
	return true;
}

bool ble_event_peek(const struct ble_event_queue *queue, uint8_t out[BLE_EVENT_SIZE])
{
	if (queue->count == 0) {
		return false;
	}
	memcpy(out, queue->payloads[queue->head], BLE_EVENT_SIZE);
	return true;
}

void ble_event_complete(struct ble_event_queue *queue, bool success)
{
	if (success && queue->count) {
		queue->head = (queue->head + 1) % BLE_EVENT_CAPACITY;
		queue->count--;
		queue->sent++;
	}
}
