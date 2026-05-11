#pragma once

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include "critical_section.h"

#define STACKSIZE       1024
#define BLINK_PERIOD_MS 250

typedef void task_func(void *p0, void *p1, void *p2);

struct sorted_critical_sections {
	sys_dlist_t by_access;
	sys_dlist_t by_release;
};

struct task_params {
	// task specification
	int task_id;
	int priority;
	int32_t release_ms;
	int32_t execution_time_ms;
	int32_t period_ms;
	// critical sections
	struct sorted_critical_sections *cs;
	// optional LED
	const struct gpio_dt_spec *led;
	// helper field
	task_func *func;
	struct k_timer *timer;
	struct k_thread *thread;
	bool accepted;
};

/**
 * @brief Time-demand analysis for periodic tasks, accounting for (I)PCP blocking time
 */
bool acceptance_test(struct task_params *params);

/**
 * @brief Task initialization, including GPIO and critical section setup
 */
void periodic_task_initialization(struct task_params *params);

/**
 * @brief Dummy task implementation that toggles LED
 */
void periodic_task_implementation(void *params, void *p1, void *p2);

/**
 * @brief Initializes periodic timer interrupt to print time in seconds
 */
void start_time_keeper(void);

/**
 * @brief Macro for initializing tasks.
 */
#define PERIODIC_TASK_SETUP(n)                                                                     \
	struct k_thread task##n##_thread;                                                          \
	static const struct gpio_dt_spec task##n##_led =                                           \
		GPIO_DT_SPEC_GET_OR(DT_CHOSEN(task##n##_led), gpios, {0});                         \
	K_TIMER_DEFINE(task##n##_timer, NULL, NULL);                                               \
	static struct sorted_critical_sections task_T##n##_cs = {                                  \
		.by_access = SYS_DLIST_STATIC_INIT(&task_T##n##_cs.by_access),                     \
		.by_release = SYS_DLIST_STATIC_INIT(&task_T##n##_cs.by_release),                   \
	};                                                                                         \
	static STRUCT_SECTION_ITERABLE(task_params, task_T##n##_params) = {                        \
		.task_id = n,                                                                      \
		.priority = CONFIG_TASK##n##_PRIORITY,                                             \
		.release_ms = CONFIG_TASK##n##_RELEASE_MS,                                         \
		.execution_time_ms = CONFIG_TASK##n##_EXECUTION_TIME_MS,                           \
		.period_ms = CONFIG_TASK##n##_PERIOD_MS,                                           \
		.cs = &task_T##n##_cs,                                                             \
		.led = &task##n##_led,                                                             \
		.func = periodic_task_implementation,                                              \
		.timer = &task##n##_timer,                                                         \
		.thread = &task##n##_thread,                                                       \
		.accepted = false,                                                                 \
	};
