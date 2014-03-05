/*
 * Standard Library Includes
 */
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * AVR Library Includes
 */

#include "lib_tmr8_tick.h"


/*
 * Generic Library Includes
 */
 
#include "memorypool.h"

/*
 * Local Application Includes
 */

#include "app_test_harness.h"

void DO_TEST_HARNESS_PRE_INIT(void)
{
	setbuf(stdout, NULL);
}

void DO_TEST_HARNESS_POST_INIT(void)
{
	uint16_t used = MEMPOOL_GetUsed();
	
	printf("Memory Pool bytes used = %d of %d = %d%%", used, MEMORY_POOL_BYTES, ((used * 100) + (MEMORY_POOL_BYTES/2)) / MEMORY_POOL_BYTES);
}

void DO_TEST_HARNESS_RUNNING(void)
{
	
}
