/* SPDX-License-Identifier: Apache-2.0 */
#ifndef KG100S_JOIN_POLICY_H
#define KG100S_JOIN_POLICY_H
#include <stdbool.h>
#include <stdint.h>

#define JOIN_BOOT_DELAY_MS 3000
uint32_t join_retry_delay_ms(unsigned int failures);
bool join_due(int64_t now, int64_t due, bool provisioned, bool joined, bool busy);
#endif
