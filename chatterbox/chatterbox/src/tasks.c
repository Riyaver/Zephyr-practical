#include "tasks.h"
#include <assert.h>
#include <stdio.h>

/**
 * @brief TODO: Let LED blink for (roughly) delay_ms.
 *
 * @param led	    LED device.
 * @param delay_ms  Delay in milliseconds. Must be a multiple of
 *		    BLINK_PERIOD_MS.
 */
void useless_load(int task_id, const struct gpio_dt_spec *led, int32_t delay_ms)
{
	assert(delay_ms % BLINK_PERIOD_MS == 0 && BLINK_PERIOD_MS % 2 == 0);

	// Let LED blink for (roughly) delay_ms
	for (int32_t d = 0; d < delay_ms; d += BLINK_PERIOD_MS) {
		printf(" Task %d\n", task_id);

		/** Modify this method so that *led* is blinking while this loop is
		 * executed. Afterwards, the LED should be in the OFF state. */
		gpio_pin_set_dt(led, 1);
		k_busy_wait(BLINK_PERIOD_MS * 500);
		gpio_pin_set_dt(led, 0);
		k_busy_wait(BLINK_PERIOD_MS * 500);
	}
	gpio_pin_set_dt(led, 0);
}

/**
 * @brief TODO: Implementation of the chatterbox task
 */
void chatterbox_task(struct task_params *params)
{	
	const struct gpio_dt_spec *led = params->led;
	gpio_pin_configure_dt(led, GPIO_OUTPUT_ACTIVE);
	// Initialize LED to OFF state
	gpio_pin_set_dt(led, 0);
	k_timer_start(params->timer, K_MSEC(params->period_ms), K_MSEC(params->period_ms));
	
	while (1) {
		// Execute the task
		useless_load(params->task_id, params->led, params->execution_time_ms);
		k_timer_status_sync(params->timer);
	}
}


/** Task Initialization macros — will hook these tasks properly */
INITIALIZE_TASK(1);
INITIALIZE_TASK(2);
INITIALIZE_TASK(3);
