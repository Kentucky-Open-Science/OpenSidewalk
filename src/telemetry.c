/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>
#include "telemetry.h"

LOG_MODULE_REGISTER(kg100s_telemetry, LOG_LEVEL_INF);

static const struct device *const temperature = DEVICE_DT_GET(DT_ALIAS(die_temp0));
static const struct gpio_dt_spec buttons[] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios),
};
static atomic_t presses[2];
static atomic_t pending;
K_MUTEX_DEFINE(temperature_lock);

static void button_event(struct input_event *evt, void *user_data)
{
	unsigned int index;

	ARG_UNUSED(user_data);
	if (evt->type != INPUT_EV_KEY || (evt->code != INPUT_KEY_0 && evt->code != INPUT_KEY_1)) {
		return;
	}
	index = evt->code == INPUT_KEY_0 ? 0 : 1;
	if (evt->value) {
		atomic_inc(&presses[index]);
	}
	atomic_set(&pending, 1);
	LOG_INF("BTN%u %s, presses=%u", index, evt->value ? "pressed" : "released",
		(unsigned int)(uint16_t)atomic_get(&presses[index]));
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_PATH(buttons)), button_event, NULL);

int telemetry_init(void)
{
	if (!device_is_ready(temperature)) {
		return -ENODEV;
	}
	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
		if (!gpio_is_ready_dt(&buttons[i])) {
			return -ENODEV;
		}
	}
	return 0;
}

int telemetry_read(struct telemetry_sample *sample)
{
	struct sensor_value value;
	int ret;

	sample->temperature_centi_c = TELEMETRY_TEMPERATURE_UNAVAILABLE;
	k_mutex_lock(&temperature_lock, K_FOREVER);
	ret = sensor_sample_fetch_chan(temperature, SENSOR_CHAN_DIE_TEMP);
	if (!ret) {
		ret = sensor_channel_get(temperature, SENSOR_CHAN_DIE_TEMP, &value);
	}
	k_mutex_unlock(&temperature_lock);
	if (!ret) {
		int64_t centi = (int64_t)value.val1 * 100 + value.val2 / 10000;

		if (centi > INT16_MIN && centi <= INT16_MAX) {
			sample->temperature_centi_c = centi;
		}
	} else {
		LOG_WRN("Die temperature read failed: %d", ret);
	}
	sample->buttons = 0;
	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
		ret = gpio_pin_get_dt(&buttons[i]);
		if (ret < 0) {
			return ret;
		}
		if (ret) {
			sample->buttons |= BIT(i);
		}
		sample->button_presses[i] = (uint16_t)atomic_get(&presses[i]);
	}
	return 0;
}

bool telemetry_pending(void)
{
	return atomic_get(&pending) != 0;
}

void telemetry_clear_pending(void)
{
	atomic_clear(&pending);
}

void telemetry_request(void)
{
	atomic_set(&pending, 1);
}
