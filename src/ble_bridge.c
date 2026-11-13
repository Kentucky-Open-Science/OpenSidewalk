/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include "ble_bridge.h"

LOG_MODULE_REGISTER(ble_bridge, LOG_LEVEL_INF);

static struct ble_event_queue events;
static struct k_spinlock queue_lock;
static atomic_t enabled;
static atomic_t connected_now;

static const struct bt_data advertising[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_DIS_VAL)),
};
static const struct bt_data scan_response[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void advertise(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(advertise_work, advertise);

static void advertise(struct k_work *work)
{
	ARG_UNUSED(work);
	if (!atomic_get(&enabled) || atomic_get(&connected_now)) {
		return;
	}
	int ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, advertising, ARRAY_SIZE(advertising),
				 scan_response, ARRAY_SIZE(scan_response));

	if (ret && ret != -EALREADY) {
		LOG_WRN("BLE advertising failed: %d; retry in one second", ret);
		k_work_reschedule(&advertise_work, K_SECONDS(1));
	} else {
		LOG_INF("BLE advertising as %s", CONFIG_BT_DEVICE_NAME);
	}
}

static void connected(struct bt_conn *conn, uint8_t error)
{
	if (error) {
		LOG_WRN("BLE connection failed: 0x%02x", error);
		return;
	}
	const bt_addr_le_t *peer = bt_conn_get_dst(conn);
	char address[BT_ADDR_LE_STR_LEN];
	k_spinlock_key_t key = k_spin_lock(&queue_lock);
	bool queued = ble_event_push(&events, peer->type, peer->a.val);

	k_spin_unlock(&queue_lock, key);
	atomic_set(&connected_now, 1);
	bt_addr_le_to_str(peer, address, sizeof(address));
	if (queued) {
		LOG_INF("BLE connected %s; TTN event queued", address);
	} else {
		LOG_WRN("BLE connected %s; event dropped (queue full/address unsupported)", address);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	atomic_clear(&connected_now);
	LOG_INF("BLE disconnected: 0x%02x", reason);
}

static void recycled(void)
{
	/* The connection slot is available again; no RF work runs in callbacks. */
	k_work_reschedule(&advertise_work, K_NO_WAIT);
}

BT_CONN_CB_DEFINE(bridge_connections) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled,
};

bool ble_bridge_pending(uint8_t payload[BLE_EVENT_SIZE])
{
	k_spinlock_key_t key = k_spin_lock(&queue_lock);
	bool pending = ble_event_peek(&events, payload);

	k_spin_unlock(&queue_lock, key);
	return pending;
}

void ble_bridge_complete(bool success)
{
	k_spinlock_key_t key = k_spin_lock(&queue_lock);

	ble_event_complete(&events, success);
	k_spin_unlock(&queue_lock, key);
}

static int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	k_spinlock_key_t key = k_spin_lock(&queue_lock);
	unsigned int count = events.count;
	uint32_t dropped = events.dropped;
	uint32_t sent = events.sent;

	k_spin_unlock(&queue_lock, key);
	shell_print(sh, "BLE %s; name: %s; connected: %s; queued: %u/8; sent: %u; dropped: %u",
		    atomic_get(&enabled) ? "enabled" : "disabled", CONFIG_BT_DEVICE_NAME,
		    atomic_get(&connected_now) ? "yes" : "no", count, sent, dropped);
	shell_print(sh, "Connections generate FPort 3 events; 30 seconds between attempts; RAM queue.");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(ble_commands,
	SHELL_CMD_ARG(status, NULL, "Show BLE connection and TTN event queue.", cmd_status, 1, 0),
	SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(ble, &ble_commands, "BLE connection bridge", NULL);

int ble_bridge_init(void)
{
	int ret = bt_enable(NULL);

	if (ret) {
		return ret;
	}
	atomic_set(&enabled, 1);
	k_work_reschedule(&advertise_work, K_NO_WAIT);
	return 0;
}
