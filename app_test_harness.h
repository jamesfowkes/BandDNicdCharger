#ifndef _APP_TEST_HARNESS_H_
#define _APP_TEST_HARNESS_H_

#ifdef TEST_HARNESS

#include <stdio.h>

#include "memorypool.h"

static uint16_t readingCount = 0;

static void DO_TEST_HARNESS_PRE_INIT(void)
{
	setbuf(stdout, NULL);
}

static void DO_TEST_HARNESS_POST_INIT(void)
{
	uint16_t used = MEMPOOL_GetUsed();
	printf("Memory Pool bytes used = %d of %d = %d%%\n", used, MEMORY_POOL_BYTES, ((used * 100) + (MEMORY_POOL_BYTES/2)) / MEMORY_POOL_BYTES);
	adc.reading = ADC_FROM_MILLIVOLTS(2000);
}

static void DO_TEST_HARNESS_RUNNING(void)
{
	if (adc.busy)
	{
		adc.busy = false;
		adc.conversionComplete = true;
		if (readingCount++ < 200)
		{
			adc.reading += 10;
		}
		else
		{
			adc.reading -= 10;
		}
		
		if (adc.reading < ADC_FROM_MILLIVOLTS(50))
		{
			readingCount = 0;
			adc.reading = ADC_FROM_MILLIVOLTS(2000);
		}
		
		printf("ADC Reading = %d\n", adc.reading);
	}
	
	TMR8_Tick_Kick(50);
}

static void _testEnterState(SM_STATEID old, SM_STATEID new, SM_EVENT e)
{
	printf("Entering state %d from %d with event %d\n", new, old, e);
}

#else
#define _testEnterState NULL
#define DO_TEST_HARNESS_SETUP() {};
#define DO_TEST_HARNESS_RUNNING() {};
#endif

#endif
