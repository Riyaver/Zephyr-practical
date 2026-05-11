#include <zephyr/kernel.h>

#include "tasks.h"
#include "critical_section.h"

K_THREAD_STACK_ARRAY_DEFINE(task_stacks, CONFIG_NUM_TASKS, STACKSIZE);

// Setup Tasks T1, T2, T3
PERIODIC_TASK_SETUP(1);
PERIODIC_TASK_SETUP(2);
PERIODIC_TASK_SETUP(3);

// Setup Resources R1, R2
RESOURCE_SETUP(1);
RESOURCE_SETUP(2);
RESOURCE_SETUP(3);

// Specific Critical Sections
CRITICAL_SECTION_SETUP(T1, R1, 2000, 3000);
CRITICAL_SECTION_SETUP(T1, R2, 1000, 4000);

CRITICAL_SECTION_SETUP(T2, R3, 1000, 2000);
CRITICAL_SECTION_SETUP(T2, R1, 2000, 3000);

CRITICAL_SECTION_SETUP(T3, R1, 1000, 5000);
CRITICAL_SECTION_SETUP(T3, R2, 4000, 5000);

int main()
{
	// Check if tasks are accepted and initialize them (GPIO pin and CS). As Zephyr's dlist can
	// have some overhead, this is kept separate to the thread initialization.
	STRUCT_SECTION_FOREACH(task_params, params) {
		printf("Task %d: blocking time %d\n", params->task_id, blocking_time(params));
		if (acceptance_test(params)) {
			printf("Task %d: accepted\n", params->task_id);
			periodic_task_initialization(params);
		} else {
			printf("Task %d: rejected\n", params->task_id);
		}
	}

	start_time_keeper();
	initialize_priority_ceilings();

	STRUCT_SECTION_FOREACH(task_params, params) {
		if (params->accepted) {
			k_thread_create(params->thread, task_stacks[params->task_id], STACKSIZE,
					params->func, params, NULL, NULL, params->priority, 0,
					K_MSEC(params->release_ms));
		}
	}

	return 0;
}
