/* SPDX-License-Identifier: Apache-2.0 */
#include "join_policy.h"

uint32_t join_retry_delay_ms(unsigned int failures)
{
	if (failures >= 5) {
		return 900000;
	}
	return 60000u << (failures > 0 ? failures - 1 : 0);
}

bool join_due(int64_t now, int64_t due, bool provisioned, bool joined, bool busy)
{
	return provisioned && !joined && !busy && due >= 0 && now >= due;
}
