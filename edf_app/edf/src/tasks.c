//Team members:  Gopalapillai Balagopal, Kattalaparambi Binesh Neha, Vaitheeswaran Kirthika and Verma, Riya
#include <assert.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include "display.h"
#include "tasks.h"

/* ---------- Part (b): Global variables ---------- */
static struct k_sem aperiodic_sem;

#define USER_BUTTON_NODE DT_NODELABEL(user_button)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(USER_BUTTON_NODE, gpios);
static struct gpio_callback button_cb;

/**
 * @brief Let LED blink for (roughly) delay_ms.
 *
 * @param params    Task parameters
 */
void useless_load_periodic_tasks(struct task_params *params)
{
	assert(params->execution_time_ms % BLINK_PERIOD_MS == 0 && BLINK_PERIOD_MS % 2 == 0);

	// Lock scheduler to avoid time sharing if the deadline of two or multiple tasks is equal
	k_sched_lock();
	for (int32_t d = 0; d < params->execution_time_ms; d += BLINK_PERIOD_MS) {
		printf(" Task %d with deadline %d\n", params->task_id,
		       params->thread->base.prio_deadline);
		gpio_pin_set_dt(params->led, 1);
		k_busy_wait(BLINK_PERIOD_MS * 1000 / 2);
		gpio_pin_set_dt(params->led, 0);
		k_busy_wait(BLINK_PERIOD_MS * 1000 / 2);
	}
	k_sched_unlock();
}

/**
 * @brief Show animation on SSD1306 for one second
 */
void useless_load_aperiodic_tasks()
{
	k_sched_lock();
	for (int32_t d = 0; d < MSEC_PER_SEC; d += BLINK_PERIOD_MS) {
		printf(" Aperiodic task\n");
		ssd1306_print_aperiodic_task();
		k_busy_wait(BLINK_PERIOD_MS * 1000);
	}
	k_sched_unlock();
}

/**
 * @brief Simplified version of z_impl_k_thread_deadline_set
 *
 * Unfortunately, z_impl_k_thread_deadline_set relies on deadlines
 * relative to the CPU cycle counter, while casting the result in
 * int32_t. For our dummy tasks, with periods and latencies in the
 * range of multiple seconds, this will result in frequent overflows
 * that breaks EDF as a consequence.
 *
 * @param thread    thread handle
 * @param deadline  thread deadline in milliseconds
 */
void thread_deadline_set(struct k_thread *thread, int deadline)
{
	/* TODO (Part a): Write a simplified version of z_impl_k_thread_deadline_set
	 * that specified the deadline in milliseconds rather than
	 * in CPU cycle counts.
	 * Hint: You can use k_yield() after updating the deadline
	 * to dequeue the thread from the ready queue. */
	
	thread->base.prio_deadline = deadline;
	k_yield();
}

/**
 * @brief System density test for periodic tasks
 *
 * @params params task specification
 */
bool acceptance_test(struct task_params *params)
{
	static bool acceptance_test_results[CONFIG_NUM_TASKS];

	/* TODO (Part a): The system density test should account for the deferrable server (DS).
	 * That is, we assume the DS is always accepted and the acceptance is solely performed for
	 * periodic tasks. Moreover the acceptance test is called iteratively (see main.c), meaning
	 * that Task i must only account for the previously accepted tasks.
	 *
	 * Your solution should
	 *  - use STRUCT_SECTION_FOREACH for retrieving the task specification of all other tasks
	 *    (see main.c for reference)
	 *  - store the acceptance test result in *acceptance_test_results*
	 * */
	
	int32_t periodic_density = 0;   /* Σ ek / min(dk, pk) */
	int32_t ds_density = 0;         /* u_s (1 + (ps − es) / di) */
	int32_t us = 0;                 /* u_s = es / ps */

	struct task_params *ds = NULL;

	if (params->type == DEFERRABLE_SERVER) {
		acceptance_test_results[params->task_id - 1] = true;
		return true;
	}
	STRUCT_SECTION_FOREACH(task_params, p) {
		if (p->type == DEFERRABLE_SERVER) {
			ds = p;
			break;
		}
	}

	if (ds == NULL) {
		return false;
	}

	us = (ds->execution_time_ms * 1000) / ds->period_ms;


	STRUCT_SECTION_FOREACH(task_params, p) {

		if (p->type != PERIODIC_TASK) {
			continue;
		}

		if (p->task_id > params->task_id) {
			continue;
		}

		if (p->task_id < params->task_id &&
		    !acceptance_test_results[p->task_id - 1]) {
			continue;
		}

		periodic_density +=
			(p->execution_time_ms * 1000) / p->period_ms;
	}

	ds_density =
		us * (1000 +
		      ((ds->period_ms - ds->execution_time_ms) * 1000)
		      / params->period_ms) / 1000;

	bool accepted =(periodic_density + ds_density) <= 1000;
	acceptance_test_results[params->task_id - 1] = accepted;
	return accepted;
}

/**
 * @brief Dummy task implementation that toggles LED
 */
void periodic_task_implementation(void *p0, void *p1, void *p2)
{
	struct task_params *params = (struct task_params *)p0;

	if (!gpio_is_ready_dt(params->led)) {
		return;
	}
	int ret = gpio_pin_configure_dt(params->led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}

	k_timer_start(params->timer, K_NO_WAIT, K_MSEC(params->period_ms));
	for (int32_t i = 1;; i++) {
		k_timer_status_sync(params->timer);
		thread_deadline_set(params->thread, i * params->period_ms);
		useless_load_periodic_tasks(params);
	}
}

/* TODO (Part b): Configure the button in the DTS overlay and initialize the GPIO driver here.
 * The goal is to release an aperiodic job whenever the button is pressed and to use the deferrable
 * server (as introduced in the lecture) to schedule them. To this end, you have to complete
 * the implementation of
 *  - button_press_callback
 *  - init_button_gpio
 *  - deferrable_server_implementation
 * */

void button_press_callback(const struct device *dev,
			   struct gpio_callback *cb,
			   uint32_t pins)
{
	/* Utilize a suitable synchronization primitive to let the DS know that
	 * the button was pressed. Note that
	 *  - Interrupt service routines (ISRs) are very different from normal threads and many
	 *    kernel APIs behave differently when called from an ISR or from a thread. The
	 *    implementation of this ISR should therefore be as lightweight as possible. For more
	 *    information, see https://docs.zephyrproject.org/latest/kernel/services/interrupts.html
	 *  - Deferrable servers are supposed to yield their execution to the scheduler if there
	 *    are no asynchronous jobs available. Thus, your synchronization primitive must not
	 *    result in busy waiting that may start periodic tasks.
	 * */
	
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	k_sem_give(&aperiodic_sem);
}

int init_button_gpio()
{
	/* TODO: Initialize the GPIO pin to trigger a callback to *button_press_callback* whenever
	 *the button is pressed. For reference, you may want to have a look at:
	 *https://docs.zephyrproject.org/latest/samples/basic/button/README.html */
	
	if (!device_is_ready(button.port)) {
		return -1;
	}

	int ret = gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0) {
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) {
		return ret;
	}

	gpio_init_callback(&button_cb, button_press_callback, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb);

	k_sem_init(&aperiodic_sem, 0, 1);

	return 0;
}

void deferrable_server_implementation(void *p0, void *p1, void *p2)
{
	struct task_params *params = (struct task_params *)p0;

	/* TODO: Implement a deferrable server that replenishes periodically, but
	 * that does not accumulate its budget over multiple cycles. For simplicity,
	 * you may assume the following:
	 *  - Every aperiodic job is identical and wants to run *useless_load_aperiodic_tasks()*
	 *  - The budget of the DS is equal to the budget of one aperiodic job per period
	 * As a starting point, you can likely reuse parts of *periodic_task_implementation*.
	 * */
	
	int32_t budget_ms;

	if (init_button_gpio() < 0) {
		return;
	}

	k_timer_start(params->timer, K_NO_WAIT, K_MSEC(params->period_ms));

	for (int32_t i = 1;; i++) {
		k_timer_status_sync(params->timer);

		thread_deadline_set(params->thread, i * params->period_ms);

		budget_ms = params->execution_time_ms;

		while (budget_ms >= MSEC_PER_SEC) {
			if (k_sem_take(&aperiodic_sem, K_NO_WAIT) == 0) {
				useless_load_aperiodic_tasks();
				budget_ms -= MSEC_PER_SEC;
			} else {
				break;
			}
		}

		k_yield();
	}
}

/**
 * @brief Prints current time in seconds
 */
void print_time()
{
	static int time_sec = 0;
	printf("T = %ds:\n", time_sec);
	time_sec++;
}
K_TIMER_DEFINE(time_keeper, print_time, NULL);

/**
 * @brief Initializes periodic timer interrupt to print time in seconds
 */
void start_time_keeper(void)
{
	k_timer_start(&time_keeper, K_NO_WAIT, K_MSEC(1000));
}