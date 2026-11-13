/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 Kentucky Open Science contributors
 * Hardware reference: Semtech SX1261/2 datasheet rev 2.2, section 8.6.
 */
#ifndef OPENSIDEWALK_KG100S_RF_SWITCH_H
#define OPENSIDEWALK_KG100S_RF_SWITCH_H
#include <stdint.h>
#include <stddef.h>

/* Reapply after sleep; preserve every bit outside each documented field. */
static inline void kg100s_rf_switch_restore(uint8_t (*read_reg)(uint16_t),
					  void (*write_reg)(uint16_t, uint8_t))
{
	static const struct {
		uint16_t address;
		uint8_t mask;
		uint8_t value;
	} fields[] = {
		{0x0580, 0x08, 0x08},
		{0x0583, 0x08, 0x00},
		{0x0584, 0x08, 0x00},
		{0x0585, 0x08, 0x00},
		{0x0920, 0x0f, 0x0e}, /* voltage selection 6, output level high */
	};

	for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
		uint8_t old = read_reg(fields[i].address);

		write_reg(fields[i].address, (old & (uint8_t)~fields[i].mask) | fields[i].value);
	}
}
#endif
