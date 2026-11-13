/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "kg100s_rf_switch.h"
static uint8_t registers[0x1000];
static unsigned int writes;
static uint8_t read_register(uint16_t address) { return registers[address]; }
static void write_register(uint16_t address, uint8_t value) { registers[address]=value; writes++; }
int main(void)
{
 registers[0x0580]=0x55; registers[0x0583]=0xff;
 registers[0x0584]=0xaa; registers[0x0585]=0x08; registers[0x0920]=0xa1;
 kg100s_rf_switch_restore(read_register,write_register);
 assert(registers[0x0580]==0x5d && registers[0x0583]==0xf7);
 assert(registers[0x0584]==0xa2 && registers[0x0585]==0x00);
 assert(registers[0x0920]==0xae); /* retain unrelated high bits */
 assert(writes==5);
 registers[0x0580]&=~8; registers[0x0583]|=8; registers[0x0920]=0x01;
 kg100s_rf_switch_restore(read_register,write_register);
 assert((registers[0x0580]&8)==8 && (registers[0x0583]&8)==0);
 assert(registers[0x0920]==0x0e && writes==10);
 puts("PASS: datasheet RF switch setup, unrelated bits preserved, restoration after sleep.");
}
