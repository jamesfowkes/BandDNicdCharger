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
#include <avr/wdt.h>

/*
 * Generic Library Includes
 */
 
#include "ringbuf.h"
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
#include "lib_tmr8_tick.h"

/*
 * Defines and Typedefs
 */

/* Pins and ports */
#define HEARTBEAT_PORT IO_PORTB 
#define CHARGE_ON_PORT IO_PORTB 
#define BATT_ADC_PORT IO_PORTB

#define HEARTBEAT_PIN 0
#define CHARGE_ON_PIN 1 
#define BATT_ADC_PIN 2

#define BATT_ADC_CHANNEL LIB_ADC_CH_0

#define BUFFER_SIZE 32

#define ADC_FROM_MILLIVOLTS(mv) (((mv * 1023UL) + 2500UL)/ 5000UL)
#define NEGATIVE_DELTAV_ADC ADC_FROM_MILLIVOLTS(300UL)
#define BATTERY_DISCONNECTED_ADC ADC_FROM_MILLIVOLTS(1000UL)

#define APPLICATION_TICK_MS 1000
#define BATTERY_CAPACITY_MAH 1500
#define CHARGE_RATE_MA 1000

#define CHARGING_TIMEOUT_COUNTS (MS_PER_HOUR * BATTERY_CAPACITY_MAH) / (CHARGE_RATE_MA * APPLICATION_TICK_MS)

enum state
{
	INIT,
	WAIT_FOR_BATT,
	WAIT_FOR_UNPLUG,
	CHARGING,
	MAX_STATE
};
typedef enum state STATE;

enum event
{
	NEW_BATTERY,
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

static void applicationTick(void);
static void adcHandler(void);
static bool batteryIsCharged(void);

static void startCharging(void);
static void stopCharging(void);

/*
 * Private Variables
 */

/* Library control structures */
static TMR8_TICK_CONFIG appTick;

static ADC_CONTROL_ENUM adc;

static uint8_t sm_index = 0;

static SM_ENTRY sm[] = {
	{INIT,				UNPLUGGED,			NULL,			WAIT_FOR_BATT	},
	{INIT,				NEW_BATTERY,		startCharging,	CHARGING		},
	
	{WAIT_FOR_BATT,		NEW_BATTERY,		startCharging,	CHARGING		},
		
	{WAIT_FOR_UNPLUG,	UNPLUGGED,			NULL,			WAIT_FOR_BATT	},
	
	{CHARGING,			CHARGED,			stopCharging,	WAIT_FOR_UNPLUG	},
	{CHARGING,			UNPLUGGED,			stopCharging,	WAIT_FOR_BATT	},
	{CHARGING,			TIMER_EXPIRED,		stopCharging,	WAIT_FOR_UNPLUG	},
};

static RING_BUFFER s_batteryVoltageBuffer;
static uint16_t s_batteryVoltageData[BUFFER_SIZE];

static uint16_t s_highestAverage = 0;
static uint16_t s_timerCounts = 0;

int main(void)
{
	
	CLK_Init(0);

	setupIO();
	setupADC();
	setupTimer();
	
	SMM_Config(1, 1);
	sm_index = SM_Init((SM_STATE)INIT, (SM_EVENT)(MAX_STATE-1), (SM_STATE)(MAX_EVENT-1), sm);
	
	Ringbuf_Init(&s_batteryVoltageBuffer, (uint8_t*)s_batteryVoltageData, sizeof(uint16_t), BUFFER_SIZE, true);
	
	sei();
	
	wdt_disable();
	
	while (true)
	{
		if (ADC_TestAndClear(&adc))
		{
			adcHandler();
		}
		
		if (TMR8_Tick_TestAndClear(&appTick))
		{
			applicationTick();
			IO_Control(HEARTBEAT_PORT, HEARTBEAT_PIN, IO_TOGGLE);
		}
	}

	return 0;
}


/*
 * Private Functions
 */

static void setupIO(void)
{
	IO_SetMode(HEARTBEAT_PORT, HEARTBEAT_PIN, IO_MODE_OUTPUT);
	IO_SetMode(CHARGE_ON_PORT, CHARGE_ON_PIN, IO_MODE_OUTPUT);
	IO_SetMode(BATT_ADC_PORT, BATT_ADC_PIN, IO_MODE_INPUT);
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

static void startCharging(void)
{
	s_highestAverage = 0;
	s_timerCounts = 0;
	IO_Control(CHARGE_ON_PORT, CHARGE_ON_PIN, IO_OFF);
}

static void stopCharging(void)
{
	IO_Control(CHARGE_ON_PORT, CHARGE_ON_PIN, IO_OFF);
}

static void adcHandler(void)
{
	if (adc.reading > BATTERY_DISCONNECTED_ADC)
	{
		Ringbuf_Put(&s_batteryVoltageBuffer, (uint8_t*)&adc.reading);
		
		if ( batteryIsCharged() )
		{
			SM_Event(sm_index, CHARGED);
		}
	}
	else
	{
		SM_Event(sm_index, UNPLUGGED);
	}
}

static bool batteryIsCharged(void)
{
	uint32_t average;

	for (uint8_t i = 0; i < BUFFER_SIZE; ++i)
	{
		average += s_batteryVoltageData[i];
	}
	
	average /= BUFFER_SIZE;
	
	s_highestAverage = max(s_highestAverage, average);
	
	return (s_highestAverage - average) > NEGATIVE_DELTAV_ADC;
}

static void applicationTick(void)
{	
	if (SM_GetState(sm_index) == CHARGING)
	{
		if (++s_timerCounts == CHARGING_TIMEOUT_COUNTS)
		{
			SM_Event(sm_index, TIMER_EXPIRED);
		}
	}
}
