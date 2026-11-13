/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ble_event.h"

int main(void)
{
	struct ble_event_queue queue = {.sequence = 0x1233};
	const uint8_t address[6] = {0x33, 0x22, 0x11, 0, 0xde, 0xc0};
	const uint8_t expected[11] = {1, 1, 0x12, 0x34, 1, 0xc0, 0xde, 0, 0x11, 0x22, 0x33};
	uint8_t payload[BLE_EVENT_SIZE], retry[BLE_EVENT_SIZE];

	assert(!ble_event_peek(&queue, payload));
	assert(ble_event_push(&queue, 3, address)); /* Resolved random identity. */
	assert(ble_event_peek(&queue, payload));
	assert(memcmp(payload, expected, sizeof(expected)) == 0);
	for (size_t i = 0; i < sizeof(payload); i++) {
		printf("%02X", payload[i]);
	}
	puts("");
	/* A failed radio attempt must retain exactly the same event. */
	ble_event_complete(&queue, false);
	assert(queue.count == 1 && queue.sent == 0);
	assert(ble_event_peek(&queue, retry));
	assert(memcmp(payload, retry, sizeof(payload)) == 0);
	ble_event_complete(&queue, true);
	assert(queue.count == 0 && queue.sent == 1);

	memset(&queue, 0, sizeof(queue));
	for (unsigned int i = 0; i < BLE_EVENT_CAPACITY; i++) {
		assert(ble_event_push(&queue, 0, address));
	}
	assert(!ble_event_push(&queue, 0, address));
	assert(queue.count == 8 && queue.dropped == 1 && queue.sequence == 8);
	for (unsigned int i = 0; i < BLE_EVENT_CAPACITY; i++) {
		assert(ble_event_peek(&queue, payload));
		assert(payload[2] == 0 && payload[3] == i + 1 && payload[4] == 0);
		ble_event_complete(&queue, true);
	}
	assert(queue.sent == 8 && !ble_event_peek(&queue, payload));
	/* Exercise ring reuse after its tail wraps and sequence rollover. */
	queue.sequence = 0xffff;
	assert(ble_event_push(&queue, 2, address));
	assert(ble_event_peek(&queue, payload));
	assert(payload[2] == 0 && payload[3] == 0 && payload[4] == 0);
	assert(!ble_event_push(&queue, 9, address));
	assert(queue.count == 1 && queue.dropped == 2);
	puts("PASS: BLE event framing, FIFO overflow, retry retention and rollover.");
	return 0;
}
