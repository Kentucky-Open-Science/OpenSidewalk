/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BLE_BRIDGE_H
#define BLE_BRIDGE_H

#include "ble_event.h"

int ble_bridge_init(void);
bool ble_bridge_pending(uint8_t payload[BLE_EVENT_SIZE]);
void ble_bridge_complete(bool success);

#endif
