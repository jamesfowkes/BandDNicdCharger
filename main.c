/*
 * main.c
 *
 *  Application file for simple battery charger
 */

/*
 * Standard Library Includes
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/*
 * AVR Includes (Defines and Primitives)
 */
 
#include <avr/io.h>
#include <avr/interrupt.h>

/*
 * Generic Library Includes
 */

#include "averager.h" 
#include "statemachine.h"
#include "statemachinemanager.h"
#include "util_macros.h"
#include "util_time.h"

/*
 * AVR Library Includes
 */

#include "lib_clk.h"
#include "lib_io.h"
#include "lib_adc.h"
#include "lib_wdt.h"
#include "lib_tmr8_tick.h"
#include "lib_swserial.h"

/*
 * Defines and Typedefs
 */

/* Pins and ports */
#define eCHARGE_ON_PORT IO_PORTB 
#define eBATT_ADC_PORT IO_PORTB
#define eCHARGE_STATE_PORT IO_PORTB 

#define CHARGE_ON_PIN 3
#define BATT_ADC_PIN 4
#define CHARGE_STATE_PIN 2

#define BATT_ADC_CHANNEL LIB_ADC_CH_2

#define BUFFER_SIZE 32

#define ADC_FROM_MILLIVOLTS(mv) (((mv * 1023UL) + 2500UL)/ 5000UL)
#define NEGATIVE_DELTAV_ADC ADC_FROM_MILLIVOLTS(300UL)

#define BATTERY_DISCONNECTED_HIGH_ADC  ADC_FROM_MILLIVOLTS(4000UL)
#define BATTERY_DISCONNECTED_LOW_ADC ADC_FROM_MILLIVOLTS(500UL)

#define APPLICATION_TICK_MS 500UL
#define BATTERY_CAPACITY_MAH 1500UL
#define CHARGE_RATE_MA 1000UL

#define CHARGING_TIMEOUT_COUNTS (MS_PER_HOUR * BATTERY_CAPACITY_MAH) / (CHARGE_RATE_MA * APPLICATION_TICK_MS)

enum state
{
	WAIT_FOR_BATT,
	WAIT_FOR_UNPLUG,
	CHARGING,
	MAX_STATE
};
typedef enum state STATE;

enum event
{
	BATTERY_PRESENT,
	UNPLUGGED,
	CHARGED,
	TIMER_EXPIRED,
	MAX_EVENT
};
typedef enum event EVENT;

/*
 * Private Function Prototypes
 */

static void setupIO(void);
static void setupADC(void);
static void setupTimer(void);
static void setupStateMachine(void);

static void applicationTick(void);
static void adcHandler(void);
static bool batteryIsCharged(void);
static void updateChargeLED(void);

static void testChargeState(SM_STATEID old, SM_STATEID new, SM_EVENT e);
static void startCharging(SM_STATEID old, SM_STATEID new, SM_EVENT e);
static void stopCharging(SM_STATEID old, SM_STATEID new, SM_EVENT e);

/*
 * Private Variables
 */

/* Library control structures */
static TMR8_TICK_CONFIG appTick;

static ADC_CONTROL_ENUM adc;

static uint8_t sm_index = 0;

#include "app_test_harness.h"

static const SM_STATE stateWaitForBatt		= { WAIT_FOR_BATT,		NULL, _testEnterState};
static const SM_STATE stateWaitForUnplug	= { WAIT_FOR_UNPLUG,	NULL, _testEnterState};
static const SM_STATE stateCharging			= { CHARGING,			NULL, _testEnterState};

static const SM_ENTRY sm[] = {
	{&stateWaitForBatt,		BATTERY_PRESENT,	startCharging,		&stateCharging		},
	{&stateWaitForBatt,		UNPLUGGED,			NULL,				&stateWaitForBatt	},
		
	{&stateWaitForUnplug,	UNPLUGGED,			NULL,				&stateWaitForBatt	},
	{&stateWaitForUnplug,	BATTERY_PRESENT,	NULL,				&stateWaitForUnplug	},
		
	{&stateCharging,		BATTERY_PRESENT,	testChargeState,	&stateCharging		},
	{&stateCharging,		CHARGED,			stopCharging,		&stateWaitForUnplug	},
	{&stateCharging,		UNPLUGGED,			stopCharging,		&stateWaitForBatt	},
	{&stateCharging,		TIMER_EXPIRED,		stopCharging,		&stateWaitForUnplug	},
};

static AVERAGER * pAverager = NULL;
static uint16_t s_highestAverage = 0;
static uint16_t s_timerCounts = 0;

int main(void)
{
	
	DO_TEST_HARNESS_PRE_INIT();

	setupIO();
	setupADC();
	setupTimer();
	setupStateMachine();
	
	pAverager = AVERAGER_GetAverager(U16, BUFFER_SIZE);
	
	sei();
	
	WD_DISABLE();
	
	DO_TEST_HARNESS_POST_INIT();
	
	while (true)
	{
		DO_TEST_HARNESS_RUNNING();
		
		if (ADC_TestAndClear(&adc))
		{
			adcHandler();
		}
		
		if (TMR8_Tick_TestAndClear(&appTick))
		{
			applicationTick();
		}		
	}

	return 0;
}

/*
 * Private Functions
 */

static void setupIO(void)
{
	IO_SetMode(eCHARGE_ON_PORT, CHARGE_ON_PIN, IO_MODE_OUTPUT);
	IO_SetMode(eBATT_ADC_PORT, BATT_ADC_PIN, IO_MODE_INPUT);
	IO_SetMode(eCHARGE_STATE_PORT, CHARGE_STATE_PIN, IO_MODE_OUTPUT);
	
	IO_Control(eCHARGE_ON_PORT, CHARGE_ON_PIN, IO_OFF);
	IO_Control(eCHARGE_STATE_PORT, CHARGE_STATE_PIN, IO_OFF);
	
	SWS_TxInit(IO_PORTB, 1);
	SWS_SetBaudRate(LIB_SWS_BAUD_4800);
}

static void setupADC(void)
{
	ADC_SelectPrescaler(LIB_ADC_PRESCALER_DIV64);
	ADC_SelectReference(LIB_ADC_REF_VCC);
	ADC_Enable(true);
	ADC_EnableInterrupts(true);

	adc.busy = false;
	adc.channel = BATT_ADC_CHANNEL;
	adc.conversionComplete = false;
}

static void setupTimer(void)
{
	CLK_Init(0);
	TMR8_Tick_Init(1, 0);

	appTick.reload = APPLICATION_TICK_MS;
	appTick.active = true;
	TMR8_Tick_AddTimerConfig(&appTick);
}

static void setupStateMachine(void)
{
	SMM_Config(1, 3);
	sm_index = SM_Init(&stateWaitForBatt, MAX_EVENT, MAX_STATE, sm);
	SM_SetActive(sm_index, true);
}

static void startCharging(SM_STATEID old, SM_STATEID new, SM_EVENT e)
{
	(void)old; (void)new; (void)e;
	s_highestAverage = 0;
	s_timerCounts = 0;
	AVERAGER_Reset(pAverager, 0);
	IO_Control(eCHARGE_ON_PORT, CHARGE_ON_PIN, IO_ON);
}

static void stopCharging(SM_STATEID old, SM_STATEID new, SM_EVENT e)
{
	(void)old; (void)new; (void)e;
	IO_Control(eCHARGE_ON_PORT, CHARGE_ON_PIN, IO_OFF);
}

static bool batteryIsConnected(void)
{
	sws_var_dump(adc.reading, "%U");
	bool connected = false;
	connected |= adc.reading > BATTERY_DISCONNECTED_HIGH_ADC;
	connected |= adc.reading < BATTERY_DISCONNECTED_LOW_ADC;
	return connected;
}

static void adcHandler(void)
{
	if (batteryIsConnected())
	{
		SM_Event(sm_index, BATTERY_PRESENT);
	}
	else
	{
		SM_Event(sm_index, UNPLUGGED);
	}
}

static void testChargeState(SM_STATEID old, SM_STATEID new, SM_EVENT e)
{
	(void)old; (void)new; (void)e;
	AVERAGER_NewData(pAverager, (void*)&adc.reading);
	if ( batteryIsCharged() )
	{
		SM_Event(sm_index, CHARGED);
	}	
}

static bool batteryIsCharged(void)
{
	uint16_t average;
	AVERAGER_GetAverage(pAverager, &average);
	
	s_highestAverage = max(s_highestAverage, average);
	
	return (s_highestAverage - average) > (int16_t)NEGATIVE_DELTAV_ADC;
}

static void applicationTick(void)
{	
	
	ADC_GetReading(&adc);
	
	if (SM_GetState(sm_index) == CHARGING)
	{
		if (++s_timerCounts == CHARGING_TIMEOUT_COUNTS)
		{
			SM_Event(sm_index, TIMER_EXPIRED);
		}
	}
	
	updateChargeLED();
}

static void updateChargeLED(void)
{
	switch(SM_GetState(sm_index))
	{
	case WAIT_FOR_BATT:
		IO_Control(eCHARGE_STATE_PORT, CHARGE_STATE_PIN, IO_OFF);
		break;
	case WAIT_FOR_UNPLUG:
		IO_Control(eCHARGE_STATE_PORT, CHARGE_STATE_PIN, IO_TOGGLE);
		break;
	case CHARGING:
		IO_Control(eCHARGE_STATE_PORT, CHARGE_STATE_PIN, IO_ON);
		break;
	}
}
