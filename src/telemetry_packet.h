/* SPDX-License-Identifier: Apache-2.0 */
#ifndef KG100S_TELEMETRY_PACKET_H
#define KG100S_TELEMETRY_PACKET_H

#include <stdint.h>

#define TELEMETRY_PAYLOAD_SIZE 10
#define TELEMETRY_TEMPERATURE_UNAVAILABLE INT16_MIN

struct telemetry_sample {
	int16_t temperature_centi_c;
	uint8_t buttons;
	uint16_t button_presses[2];
};

void telemetry_pack(uint8_t out[TELEMETRY_PAYLOAD_SIZE], uint16_t sequence,
		    const struct telemetry_sample *sample);

#endif
