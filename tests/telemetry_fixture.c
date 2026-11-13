/* SPDX-License-Identifier: Apache-2.0 */
#include <stdio.h>
#include "telemetry_packet.h"

int main(void)
{
    struct telemetry_sample sample = {
        .temperature_centi_c = -1234,
        .buttons = 3,
        .button_presses = {0x0102, 0xabcd},
    };
    uint8_t payload[TELEMETRY_PAYLOAD_SIZE];

    telemetry_pack(payload, 0x1234, &sample);
    for (size_t i = 0; i < sizeof(payload); i++) {
        printf("%02X", payload[i]);
    }
    puts("");
    return 0;
}
