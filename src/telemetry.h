/* SPDX-License-Identifier: Apache-2.0 */
#ifndef KG100S_TELEMETRY_H
#define KG100S_TELEMETRY_H

#include <stdbool.h>
#include "telemetry_packet.h"

int telemetry_init(void);
int telemetry_read(struct telemetry_sample *sample);
bool telemetry_pending(void);
void telemetry_clear_pending(void);
void telemetry_request(void);

#endif
