/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "join_policy.h"

int main(void)
{
	assert(JOIN_BOOT_DELAY_MS == 3000);
	assert(!join_due(2999, JOIN_BOOT_DELAY_MS, true, false, false));
	assert(join_due(3000, JOIN_BOOT_DELAY_MS, true, false, false));
	assert(!join_due(10000, 3000, false, false, false));
	assert(!join_due(10000, 3000, true, true, false));
	assert(!join_due(10000, 3000, true, false, true));
	assert(!join_due(10000, -1, true, false, false));
	assert(join_retry_delay_ms(1) == 60000);
	assert(join_retry_delay_ms(2) == 120000);
	assert(join_retry_delay_ms(3) == 240000);
	assert(join_retry_delay_ms(4) == 480000);
	assert(join_retry_delay_ms(5) == 900000);
	assert(join_retry_delay_ms(6) == 900000);
	assert(join_retry_delay_ms(UINT_MAX) == 900000);
	int64_t retry_due = 7000 + join_retry_delay_ms(1);
	assert(!join_due(66999, retry_due, true, false, false));
	assert(join_due(67000, retry_due, true, false, false));
	puts("PASS: automatic join gates, boot delay, retry deadlines and capped backoff.");
}
