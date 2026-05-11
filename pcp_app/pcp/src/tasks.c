#include <assert.h>
#include <stdio.h>
#include <zephyr/sys/dlist.h>

#include "tasks.h"
#include "display.h"

/**
 * TODO (Part c): Time-demand analysis for periodic tasks, accounting for (I)PCP blocking time
 * (You can largely reuse your solution of the second practical assignment.)
 */
bool acceptance_test(struct task_params *params)
{
    int Ci = params->execution_time_ms;
    int Pi = params->period_ms;
    int Bi = blocking_time(params); 
    int t = Ci + Bi;
    int prev_t;

    while (t <= Pi) {
        prev_t = t;
        int demand = Ci + Bi;
        STRUCT_SECTION_FOREACH(task_params, hp_task) {
            if (hp_task->priority < params->priority) {
                demand += ((t + hp_task->period_ms - 1) / hp_task->period_ms) * hp_task->execution_time_ms;
            }
        }

        if (demand <= t) {
            params->accepted = true;
            return true;
        }
        
        t = demand;
        if (t == prev_t) break;
    }

    params->accepted = false;
    return false;
}

/**
 * @brief Let LED blink for (roughly) delay_ms.
 *
 * @param params    Task parameters
 * @param duration  Duration in ms
 */
void useless_load_periodic_tasks(struct task_params *params, int32_t duration)
{
	assert(params->execution_time_ms % BLINK_PERIOD_MS == 0 && BLINK_PERIOD_MS % 2 == 0);
	ssd1306_print_resource();

	for (int32_t d = 0; d < duration; d += BLINK_PERIOD_MS) {
		printf(" Task %d (prio %d)\n", params->task_id,
		       k_thread_priority_get(k_current_get()));
		gpio_pin_set_dt(params->led, 1);
		k_busy_wait(BLINK_PERIOD_MS * 1000 / 2);
		gpio_pin_set_dt(params->led, 0);
		k_busy_wait(BLINK_PERIOD_MS * 1000 / 2);
	}
}

/*
 * @brief Comparator to sort the double-linked list by the CS access time
 */
int sort_by_access(sys_dnode_t *succ, void *data)
{
	struct critical_section_params *cs = (struct critical_section_params *)data;
	struct task_critical_section *cs_succ = SYS_DLIST_CONTAINER(succ, cs_succ, access_node);
	return cs_succ->cs->access_time > cs->access_time;
}

/*
 * @brief Comparator to sort the double-linked list by the CS release time
 */
int sort_by_release(sys_dnode_t *succ, void *data)
{
	struct critical_section_params *cs = (struct critical_section_params *)data;
	struct task_critical_section *cs_succ = SYS_DLIST_CONTAINER(succ, cs_succ, release_node);
	return cs_succ->cs->release_time > cs->release_time;
}

/*
 * @brief Sort the CS of this task according to their access and release time (respectively)
 * to simplify operation later on, where we have to decide which mutex to lock/unlock.
 */
void initialize_critical_sections(struct task_params *params)
{
	STRUCT_SECTION_FOREACH(critical_section_params, p) {
		if (p->task->task_id == params->task_id) {
			p->task_cs->cs = p;
			sys_dnode_init(&p->task_cs->access_node);
			sys_dlist_insert_at(&params->cs->by_access, &p->task_cs->access_node,
					    sort_by_access, p);
			sys_dnode_init(&p->task_cs->release_node);
			sys_dlist_insert_at(&params->cs->by_release, &p->task_cs->release_node,
					    sort_by_release, p);
		}
	}

	struct task_critical_section *t_cs;
	printf("Critical Sections of Task%d:\n", params->task_id);
	printf(" Sorted by Access: ");
	SYS_DLIST_FOR_EACH_CONTAINER(&params->cs->by_access, t_cs, access_node) {
		struct critical_section_params *c = t_cs->cs;
		printf("R%d[%d, %d] ", c->resource->id, c->access_time, c->release_time);
	}
	printf("\n Sorted by Release: ");
	SYS_DLIST_FOR_EACH_CONTAINER(&params->cs->by_release, t_cs, release_node) {
		struct critical_section_params *c = t_cs->cs;
		printf("R%d[%d, %d] ", c->resource->id, c->access_time, c->release_time);
	}
	printf("\n");
}

/*
 * @brief Initialized the GPIO pin used for the LED
 */
void initialize_gpio(struct task_params *params)
{
	if (!gpio_is_ready_dt(params->led)) {
		return;
	}
	int ret = gpio_pin_configure_dt(params->led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}
}

/*
 * @brief Check if a CS needs to be accessed at this time
 */
struct task_critical_section *check_access_cs(int32_t exec_time, struct task_params *params,
					      struct task_critical_section *next_access)
{
	while (next_access != NULL && exec_time == next_access->cs->access_time) {
		printf(" Task %d requests R%d\n", params->task_id, next_access->cs->resource->id);
		mutex_lock(next_access->cs->resource);
		printf(" Task %d locks R%d\n", params->task_id, next_access->cs->resource->id);
		next_access = SYS_DLIST_PEEK_NEXT_CONTAINER(&params->cs->by_access, next_access,
							    access_node);
	}
	return next_access;
}

/*
 * @brief Check if a CS needs to be released at this time
 */
struct task_critical_section *check_release_cs(int32_t exec_time, struct task_params *params,
					       struct task_critical_section *next_release)
{
	while (next_release != NULL && exec_time == next_release->cs->release_time) {
		printf(" Task %d unlocks R%d\n", params->task_id, next_release->cs->resource->id);
		mutex_unlock(next_release->cs->resource, params);
		next_release = SYS_DLIST_PEEK_NEXT_CONTAINER(&params->cs->by_release, next_release,
							     release_node);
	}
	return next_release;
}

/*
 * @brief Compute the duration for which the thread can continue its execution, before reaching
 * its (i) maximum execution time, (ii) the next CS access time, or (iii) the next CS release time.
 */
int compute_operation_duration(int32_t exec_time, struct task_params *params,
			       struct task_critical_section *next_access,
			       struct task_critical_section *next_release)
{
	int32_t duration = params->execution_time_ms - exec_time;
	if (next_access != NULL) {
		duration = MIN(duration, next_access->cs->access_time - exec_time);
	}
	if (next_release != NULL) {
		duration = MIN(duration, next_release->cs->release_time - exec_time);
	}
	return duration;
}

/**
 * @brief Task initialization, including GPIO and critical section setup
 */
void periodic_task_initialization(struct task_params *params)
{
	initialize_gpio(params);
	initialize_critical_sections(params);
}

/**
 * @brief Dummy task implementation that toggles LED
 */
void periodic_task_implementation(void *p0, void *p1, void *p2)
{
	struct task_params *params = (struct task_params *)p0;

	k_timer_start(params->timer, K_NO_WAIT, K_MSEC(params->period_ms));
	for (int32_t i = 1;; i++) {
		k_timer_status_sync(params->timer);

		struct task_critical_section *next_access = SYS_DLIST_PEEK_HEAD_CONTAINER(
			&params->cs->by_access, next_access, access_node);
		struct task_critical_section *next_release = SYS_DLIST_PEEK_HEAD_CONTAINER(
			&params->cs->by_release, next_release, release_node);

		int32_t duration = 0;
		for (int32_t exec_time = 0; exec_time < params->execution_time_ms;) {
			// check if new CS needs to be requested/locked
			next_access = check_access_cs(exec_time, params, next_access);

			// run until next CS needs to be locked/unlocked
			duration = compute_operation_duration(exec_time, params, next_access,
							      next_release);
			useless_load_periodic_tasks(params, duration);
			exec_time += duration;

			// check if CS needs to be unlocked
			next_release = check_release_cs(exec_time, params, next_release);
		}
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
