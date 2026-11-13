/* SPDX-License-Identifier: Apache-2.0 */
#include "telemetry_packet.h"

void telemetry_pack(uint8_t out[TELEMETRY_PAYLOAD_SIZE], uint16_t sequence,
		    const struct telemetry_sample *sample)
{
	uint16_t temp = (uint16_t)sample->temperature_centi_c;

	out[0] = 1;
	out[1] = sequence >> 8;
	out[2] = sequence;
	out[3] = temp >> 8;
	out[4] = temp;
	out[5] = sample->buttons & 3;
	out[6] = sample->button_presses[0] >> 8;
	out[7] = sample->button_presses[0];
	out[8] = sample->button_presses[1] >> 8;
	out[9] = sample->button_presses[1];
}
