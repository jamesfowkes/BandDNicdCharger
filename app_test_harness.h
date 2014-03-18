#ifndef _APP_TEST_HARNESS_H_
#define _APP_TEST_HARNESS_H_

#ifdef TEST_HARNESS

#include <stdio.h>

#include "util_sequence_generator.h"
#include "memorypool.h"
#include "lib_adc_harness_functions.h"

static SEQUENCE * pBatteryReadingSequence;

static void DO_TEST_HARNESS_PRE_INIT(void)
{
	setbuf(stdout, NULL);
}

static void DO_TEST_HARNESS_POST_INIT(void)
{
	uint16_t used = MEMPOOL_GetUsed();
	printf("Memory Pool bytes used = %d of %d = %d%%\n", used, MEMORY_POOL_BYTES, ((used * 100) + (MEMORY_POOL_BYTES/2)) / MEMORY_POOL_BYTES);
	
	pBatteryReadingSequence = SEQGEN_GetNewSequence(200);
	ADC_Harness_SetReadingArray(BATT_ADC_CHANNEL, pBatteryReadingSequence);
	
	SEQGEN_AddConstants(pBatteryReadingSequence, 0, 30);
	SEQGEN_AddConstants(pBatteryReadingSequence, ADC_FROM_MILLIVOLTS(2000), 30);
	SEQGEN_AddRamp_StartStepLength(pBatteryReadingSequence, ADC_FROM_MILLIVOLTS(2000), -(int16_t)ADC_FROM_MILLIVOLTS(5), 100);
	SEQGEN_AddConstants(pBatteryReadingSequence, BATTERY_DISCONNECTED_ADC-1, 30);
}

static void DO_TEST_HARNESS_RUNNING(void)
{
	if (adc.busy)
	{
		ADC_Harness_TriggerISR(&adc);
	}
	
	TMR8_Tick_Kick(50);
}

static void _testEnterState(SM_STATEID old, SM_STATEID new, SM_EVENT e)
{
	printf("Entering state %d from %d with event %d\n", new, old, e);
}

#else
#define _testEnterState NULL
#define DO_TEST_HARNESS_PRE_INIT() {}
#define DO_TEST_HARNESS_POST_INIT() {}
#define DO_TEST_HARNESS_RUNNING() {}
#endif

#endif
