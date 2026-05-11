#pragma once

#include <zephyr/kernel.h>
#include <zephyr/sys/dlist.h>

struct resource_params {
	uint32_t id;
	int priority_ceiling;
	struct k_mutex *mutex;
};

struct task_params;
struct task_critical_section;
struct critical_section_params {
	struct task_params *task;
	struct resource_params *resource;
	struct task_critical_section *task_cs;
	int access_time;
	int release_time;
};

struct task_critical_section {
	struct critical_section_params *cs;
	sys_dnode_t access_node;
	sys_dnode_t release_node;
};

/*
 * @brief Initialize resource priority ceilings and system ceiling
 */
void initialize_priority_ceilings();

/*
 * @brief Blocks the current thread until it is safe to lock the resource
 *
 * The semantic of this method depends on PIP (default), IPCP, or PCP:
 *  - PIP: Utilizes the normal mutex provided by Zephyr (see
 *    https://docs.zephyrproject.org/latest/kernel/services/synchronization/mutexes.html)
 *  - IPCP: Locks the resource if and only if the thread's priority exceeds the system ceiling.
 *    If so, the thread's priority is immediately elevated to the resource ceiling.
 *  - PCP: Locks the resource if and only if the thread's priority exceeds the system ceiling.
 *    If it is unable to do so (i.e., due to priority ceiling blocking), elevate the priority
 *    of the thread that holds the resource associated with the system ceiling.
 */
int mutex_lock(struct resource_params *resource);

/*
 * @brief Unlocks the resource
 *
 * The semantic of this method depends on PIP (default), IPCP, or PCP:
 *  - PIP: Utilizes the normal mutex provided by Zephyr (see
 *    https://docs.zephyrproject.org/latest/kernel/services/synchronization/mutexes.html)
 *  - IPCP & PCP: Unlocks the mutex and resets the thread's priority accordingly.
 */
int mutex_unlock(struct resource_params *resource, const struct task_params *params);

/*
 * @brief Compute the maximum blocking time of the given task under the assumption that
 * all lower-priority tasks are scheduled.
 */
int blocking_time(const struct task_params *params);

#define RESOURCE_SETUP(resource_id)                                                                \
	K_MUTEX_DEFINE(R##resource_id##_mutex);                                                    \
	static STRUCT_SECTION_ITERABLE(resource_params, resource_R##resource_id##_) = {            \
		.id = resource_id, .priority_ceiling = 0, .mutex = &R##resource_id##_mutex};

#define CRITICAL_SECTION_SETUP(task_id, resource_id, t1, t2)                                       \
	__maybe_unused static struct task_critical_section                                         \
		task_##task_id##_cs_##resource_id##_##t1##_##t2##_cs;                              \
	static STRUCT_SECTION_ITERABLE(critical_section_params,                                    \
				       task_##task_id##_cs_##resource_id##_##t1##_##t2##_) = {     \
		.task = &task_##task_id##_params,                                                  \
		.resource = &resource_##resource_id##_,                                            \
		.task_cs = &task_##task_id##_cs_##resource_id##_##t1##_##t2##_cs,                  \
		.access_time = t1,                                                                 \
		.release_time = t2,                                                                \
	};
