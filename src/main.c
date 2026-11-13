/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/lorawan/lorawan.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>
#include "telemetry.h"
#include "join_policy.h"

LOG_MODULE_REGISTER(kg100s_lorawan, LOG_LEVEL_INF);

struct node_config {
	uint8_t version;
	uint8_t provisioned;
	uint8_t subband;
	uint8_t reserved;
	uint8_t dev_eui[8];
	uint8_t join_eui[8];
	uint8_t app_key[16];
	uint32_t last_nonce;
};

static struct node_config config = {.version = 1, .subband = 2};
static atomic_t ready;
static atomic_t joined;
static atomic_t busy;
static int config_load_error;
static uint16_t telemetry_sequence;
static int64_t next_periodic;
static int64_t telemetry_not_before;
static int64_t next_join;
static unsigned int join_failures;
static atomic_t join_delay_seconds = ATOMIC_INIT(JOIN_BOOT_DELAY_MS / 1000);
K_MUTEX_DEFINE(config_lock);

enum job_kind { JOB_JOIN, JOB_SEND };
struct job {
	enum job_kind kind;
};
K_MSGQ_DEFINE(jobs, sizeof(struct job), 1, 4);

static int config_load(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	struct node_config saved;
	int ret;

	if (strcmp(name, "config") != 0) {
		return -ENOENT;
	}
	if (len != sizeof(saved)) {
		return config_load_error = -EINVAL;
	}
	ret = read_cb(cb_arg, &saved, sizeof(saved));
	if (ret != sizeof(saved)) {
		return config_load_error = ret < 0 ? ret : -EIO;
	}
	if (saved.version != 1 || saved.subband < 1 || saved.subband > 8 ||
	    saved.provisioned > 1 || saved.last_nonce > UINT16_MAX) {
		return config_load_error = -EINVAL;
	}
	config = saved;
	return 0;
}
SETTINGS_STATIC_HANDLER_DEFINE(kg100s_node, "kg100s", NULL, config_load, NULL, NULL);

static struct node_config config_snapshot(void)
{
	struct node_config copy;

	k_mutex_lock(&config_lock, K_FOREVER);
	copy = config;
	k_mutex_unlock(&config_lock);
	return copy;
}

static int save_config(const struct node_config *updated)
{
	int ret = settings_save_one("kg100s/config", updated, sizeof(*updated));

	if (ret == 0) {
		k_mutex_lock(&config_lock, K_FOREVER);
		config = *updated;
		k_mutex_unlock(&config_lock);
	}
	return ret;
}

static int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
	struct node_config copy = config_snapshot();
	char dev_eui[17], join_eui[17];

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	bin2hex(copy.dev_eui, sizeof(copy.dev_eui), dev_eui, sizeof(dev_eui));
	bin2hex(copy.join_eui, sizeof(copy.join_eui), join_eui, sizeof(join_eui));
	shell_print(sh, "KG100S LoRaWAN telemetry v0.3.1; Zephyr 4.5.0");
	shell_print(sh, "Radio/stack: %s; joined: %s; busy: %s",
		    atomic_get(&ready) ? "ready" : "not ready",
		    atomic_get(&joined) ? "yes" : "no", atomic_get(&busy) ? "yes" : "no");
	shell_print(sh, "US915 sub-band %u: uplink channels %u-%u, 500kHz channel %u",
		    copy.subband, (copy.subband - 1) * 8, copy.subband * 8 - 1,
		    63 + copy.subband);
	shell_print(sh, "OTAA credentials: %s; DevEUI: %s; JoinEUI: %s",
		    copy.provisioned ? "configured" : "not configured", dev_eui, join_eui);
	shell_print(sh, "Last reserved DevNonce: %u; fresh OTAA joins automatically after reboot.",
		    (unsigned int)copy.last_nonce);
	if (atomic_get(&join_delay_seconds) < 0) {
		shell_print(sh, "Auto-join halted by configuration/storage error; check logs.");
	} else {
		shell_print(sh, "Auto-join enabled; last scheduled delay: %u seconds.",
			    (unsigned int)atomic_get(&join_delay_seconds));
	}
	return 0;
}

static int begin_config(const struct shell *sh)
{
	if (!atomic_cas(&busy, 0, 1)) {
		shell_error(sh, "Operation in progress.");
		return -EBUSY;
	}
	if (!atomic_get(&ready) || atomic_get(&joined)) {
		atomic_clear(&busy);
		shell_error(sh, "Radio not ready, operation in progress, or already joined. Reboot before changing a joined profile.");
		return -EBUSY;
	}
	return 0;
}

static int cmd_provision(const struct shell *sh, size_t argc, char **argv)
{
	struct node_config updated;
	uint8_t dev_eui[8], join_eui[8], app_key[16];
	int ret;

	ARG_UNUSED(argc);
	if (strlen(argv[1]) != 16 || strlen(argv[2]) != 16 || strlen(argv[3]) != 32 ||
	    hex2bin(argv[1], 16, dev_eui, 8) != 8 ||
	    hex2bin(argv[2], 16, join_eui, 8) != 8 ||
	    hex2bin(argv[3], 32, app_key, 16) != 16) {
		shell_error(sh, "Expected hexadecimal DevEUI (16), JoinEUI (16), AppKey (32). No colons.");
		return -EINVAL;
	}
	ret = begin_config(sh);
	if (ret) {
		return ret;
	}
	updated = config_snapshot();
	memcpy(updated.dev_eui, dev_eui, sizeof(dev_eui));
	memcpy(updated.join_eui, join_eui, sizeof(join_eui));
	memcpy(updated.app_key, app_key, sizeof(app_key));
	updated.provisioned = 1;
	ret = save_config(&updated);
	atomic_clear(&busy);
	if (ret) {
		shell_error(sh, "Saving credentials failed: %d", ret);
	} else {
		shell_print(sh, "OTAA credentials saved; AppKey is hidden. Register the same values on the network server, then run lw join.");
	}
	return ret;
}

static int cmd_subband(const struct shell *sh, size_t argc, char **argv)
{
	struct node_config updated;
	int ret;

	ARG_UNUSED(argc);
	if (strlen(argv[1]) != 1 || argv[1][0] < '1' || argv[1][0] > '8') {
		shell_error(sh, "Sub-band must be 1 through 8.");
		return -EINVAL;
	}
	ret = begin_config(sh);
	if (ret) {
		return ret;
	}
	updated = config_snapshot();
	updated.subband = argv[1][0] - '0';
	ret = save_config(&updated);
	atomic_clear(&busy);
	if (ret) {
		shell_error(sh, "Saving sub-band failed: %d", ret);
	} else {
		shell_print(sh, "Sub-band %u saved; applied on the next join.", updated.subband);
	}
	return ret;
}

static int cmd_queue(const struct shell *sh, size_t argc, char **argv)
{
	struct job job;
	int ret;

	ARG_UNUSED(argc);
	if (!atomic_cas(&busy, 0, 1)) {
		shell_error(sh, "Operation in progress.");
		return -EBUSY;
	}
	if (!atomic_get(&ready)) {
		shell_error(sh, "Radio/stack is not ready.");
		ret = -ENODEV;
		goto rejected;
	}
	job.kind = strcmp(argv[0], "join") == 0 ? JOB_JOIN : JOB_SEND;
	if (!config_snapshot().provisioned) {
		shell_error(sh, "Configure credentials with lw provision first.");
		ret = -EINVAL;
		goto rejected;
	}
	if (job.kind == JOB_SEND && !atomic_get(&joined)) {
		shell_error(sh, "Join the network first.");
		ret = -ENOTCONN;
		goto rejected;
	}
	if (job.kind == JOB_JOIN && atomic_get(&joined)) {
		shell_error(sh, "Already joined.");
		ret = -EALREADY;
		goto rejected;
	}
	ret = k_msgq_put(&jobs, &job, K_NO_WAIT);
	if (!ret) {
		return 0;
	}
rejected:
	atomic_clear(&busy);
	return ret;
}

static int cmd_sensors(const struct shell *sh, size_t argc, char **argv)
{
	struct telemetry_sample sample;
	int ret;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	ret = telemetry_read(&sample);
	if (ret) {
		return ret;
	}
	shell_print(sh, "MCU die temperature: %d centi-degrees C; BTN0=%u (%u presses), BTN1=%u (%u presses)",
		    sample.temperature_centi_c, !!(sample.buttons & 1), sample.button_presses[0],
		    !!(sample.buttons & 2), sample.button_presses[1]);
	return 0;
}

static int cmd_reboot(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	if (atomic_get(&busy)) {
		shell_error(sh, "Wait for the current radio/settings operation to finish.");
		return -EBUSY;
	}
	shell_print(sh, "Rebooting; saved credentials and LoRaWAN nonces are retained.");
	k_msleep(100);
	sys_reboot(SYS_REBOOT_COLD);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(lw_commands,
	SHELL_CMD_ARG(status, NULL, "Show radio, region, and profile status.", cmd_status, 1, 0),
	SHELL_CMD_ARG(provision, NULL, "<DevEUI> <JoinEUI> <AppKey>, hexadecimal MSB first.", cmd_provision, 4, 0),
	SHELL_CMD_ARG(subband, NULL, "<1-8> Select US915 uplink sub-band.", cmd_subband, 2, 0),
	SHELL_CMD_ARG(join, NULL, "Request one OTAA join; send telemetry if accepted.", cmd_queue, 1, 0),
	SHELL_CMD_ARG(send, NULL, "Send current temperature/button telemetry.", cmd_queue, 1, 0),
	SHELL_CMD_ARG(sensors, NULL, "Read die temperature, button states, and press counts locally.", cmd_sensors, 1, 0),
	SHELL_CMD_ARG(reboot, NULL, "Restart the application.", cmd_reboot, 1, 0),
	SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(lw, &lw_commands, "KG100S LoRaWAN controls", NULL);

static void downlink(uint8_t port, uint8_t flags, int16_t rssi, int8_t snr,
		     uint8_t len, const uint8_t *data)
{
	ARG_UNUSED(flags);
	LOG_INF("Downlink port %u, RSSI %d dBm, SNR %d dB", port, rssi, snr);
	if (len && data) {
		LOG_HEXDUMP_INF(data, len, "Downlink payload");
	}
}

static int send_telemetry(void)
{
	struct telemetry_sample sample;
	uint8_t payload[TELEMETRY_PAYLOAD_SIZE];
	uint16_t sequence = telemetry_sequence + 1;
	int ret;

	/* Clear before sampling so events during TX/RX schedule another update. */
	telemetry_clear_pending();
	ret = telemetry_read(&sample);
	if (!ret) {
		telemetry_pack(payload, sequence, &sample);
		ret = lorawan_send(2, payload, sizeof(payload), LORAWAN_MSG_UNCONFIRMED);
	}
	telemetry_not_before = k_uptime_get() + 10000;
	if (ret) {
		telemetry_request();
		LOG_ERR("Telemetry uplink failed: %d; retry scheduled", ret);
	} else {
		telemetry_sequence = sequence;
		next_periodic = k_uptime_get() + 300000;
		LOG_INF("Telemetry sent seq=%u temp=%d centi-C BTN0=%u/count%u BTN1=%u/count%u",
			sequence, sample.temperature_centi_c, !!(sample.buttons & 1),
			sample.button_presses[0], !!(sample.buttons & 2), sample.button_presses[1]);
	}
	return ret;
}

static int reserve_join_nonce(uint16_t *previous)
{
	struct node_config updated = config_snapshot();
	uint32_t next = updated.last_nonce + 1;
	int ret;

	if (next > UINT16_MAX) {
		return -EOVERFLOW;
	}
	/* Commit before RF TX. A reset during join must never reuse this nonce. */
	*previous = (uint16_t)updated.last_nonce;
	updated.last_nonce = next;
	ret = save_config(&updated);
	if (ret) {
		return ret;
	}
	/* This pinned LoRaMac-node version increments the supplied value for TX. */
	return 0;
}

static void attempt_join(void)
{
	struct node_config profile = config_snapshot();
	uint16_t mask[6] = {0};
	unsigned int first_channel = (profile.subband - 1) * 8;
	struct lorawan_join_config join_cfg = {
		.mode = LORAWAN_ACT_OTAA,
		.dev_eui = profile.dev_eui,
		.otaa = {
			.join_eui = profile.join_eui,
			.app_key = profile.app_key,
			.nwk_key = profile.app_key,
		},
	};
	int ret;

	mask[first_channel / 16] = 0xffu << (first_channel % 16);
	mask[4] = BIT(profile.subband - 1);
	ret = lorawan_set_channels_mask(mask, ARRAY_SIZE(mask));
	if (!ret) {
		ret = reserve_join_nonce(&join_cfg.otaa.dev_nonce);
	}
	if (ret) {
		next_join = -1;
		atomic_set(&join_delay_seconds, -1);
		LOG_ERR("OTAA setup/storage failed: %d; automatic joins halted", ret);
		return;
	}
	LOG_INF("OTAA join: US915 sub-band %u, channels %u-%u, reserved DevNonce %u",
		profile.subband, first_channel, first_channel + 7,
		(unsigned int)join_cfg.otaa.dev_nonce + 1);
	ret = lorawan_join(&join_cfg);
	if (ret) {
		uint32_t delay = join_retry_delay_ms(++join_failures);

		next_join = k_uptime_get() + delay;
		atomic_set(&join_delay_seconds, delay / 1000);
		LOG_WRN("Join failed: %d; retry in %u seconds", ret, delay / 1000);
		return;
	}
	join_failures = 0;
	next_join = -1;
	atomic_set(&join_delay_seconds, 0);
	atomic_set(&joined, 1);
	LOG_INF("LoRaWAN OTAA JOIN ACCEPTED");
	send_telemetry();
}

int main(void)
{
	const struct device *radio = DEVICE_DT_GET(DT_ALIAS(lora0));
	static struct lorawan_downlink_cb callback = {.port = LW_RECV_PORT_ANY, .cb = downlink};
	struct job job;
	bool banner_shown = false;
	int ret;


	LOG_INF("KG100S LoRaWAN telemetry v0.3.1; US915; automatic OTAA");
	if (!device_is_ready(radio)) {
		LOG_ERR("Integrated SX1262 is not ready");
		return -ENODEV;
	}
	ret = telemetry_init();
	if (ret) {
		LOG_ERR("Temperature/button initialization failed: %d", ret);
		return ret;
	}
	ret = settings_subsys_init();
	if (!ret) {
		ret = settings_load_subtree("kg100s");
	}
	/* Settings ignores h_set errors, so explicitly fail closed on our latch. */
	if (!ret) {
		ret = config_load_error;
	}
	if (ret) {
		LOG_ERR("Persistent settings failed: %d; radio transmission disabled", ret);
		return ret;
	}
	ret = lorawan_start();
	if (ret) {
		LOG_ERR("LoRaWAN initialization failed: %d", ret);
		return ret;
	}
	lorawan_register_downlink_callback(&callback);
	lorawan_enable_adr(true);
	atomic_set(&ready, 1);
	next_join = k_uptime_get() + JOIN_BOOT_DELAY_MS;
	LOG_INF("SX1262 and LoRaWAN ready; automatic join in 3 seconds when provisioned.");

	while (true) {
		if (k_msgq_get(&jobs, &job, K_MSEC(100)) == 0) {
			if (job.kind == JOB_JOIN) {
				attempt_join();
			} else {
				send_telemetry();
			}
			atomic_clear(&busy);
		}
		if (!banner_shown && k_uptime_get() >= JOIN_BOOT_DELAY_MS) {
			printk("\n"
			       "      .  |  .\n"
			       "   ((   /|\\   ))\n"
			       "       /_|_\\\n"
			       "  +-- KG100S --+\n"
			       "  | LoRaWAN TTN |\n"
			       "  +-------------+\n\n");
			banner_shown = true;
		}
		if (join_due(k_uptime_get(), next_join, config_snapshot().provisioned,
			     atomic_get(&joined), atomic_get(&busy)) && atomic_cas(&busy, 0, 1)) {
			attempt_join();
			atomic_clear(&busy);
		}
		if (atomic_get(&joined) && k_uptime_get() >= telemetry_not_before &&
		    (telemetry_pending() || k_uptime_get() >= next_periodic) &&
		    atomic_cas(&busy, 0, 1)) {
			send_telemetry();
			atomic_clear(&busy);
		}
	}
	return 0;
}
