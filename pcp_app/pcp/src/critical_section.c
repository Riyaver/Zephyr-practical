#include "critical_section.h"
#include "tasks.h"
#include <assert.h>

/* Global array to store pre-calculated resource ceilings */
static int resource_ceilings[3];

/*
 * TODO (Part b): Initialize resource priority ceilings and system ceiling
 */
void initialize_priority_ceilings()
{
    resource_ceilings[0] = 0; 
    resource_ceilings[1] = 0; 
    resource_ceilings[2] = 1;
}

/*
 * TODO (Part b): Locks the resource and elevates priority immediately (IPCP)
 */
int mutex_lock(struct resource_params *r)
{
    k_tid_t current_thread = k_current_get();
    int ceiling = resource_ceilings[r->id - 1];
    int err = k_mutex_lock(r->mutex, K_FOREVER);
    if (err != 0) {
        return err;
    }
    if (k_thread_priority_get(current_thread) > ceiling) {
        k_thread_priority_set(current_thread, ceiling);
    }

    return 0;
}

/*
 * TODO (Part b): Unlock the resource and restore priority
 */
int mutex_unlock(struct resource_params *r, const struct task_params *params)
{
    int err = k_mutex_unlock(r->mutex);
    if (err != 0) {
        return err;
    }
    k_thread_priority_set(k_current_get(), params->priority);

    return 0;
}

/*
 * TODO (Part c): Compute maximum blocking time Bi induced by (I)PCP [cite: 77]
 */
int blocking_time(const struct task_params *params)
{
    int max_b = 0;
    int my_prio = params->priority;

    STRUCT_SECTION_FOREACH(critical_section_params, cs) 
	{
        if (cs->task->priority > my_prio) {
            int ceiling = resource_ceilings[cs->resource->id - 1];
            if (ceiling <= my_prio) {
                int duration = cs->release_time - cs->access_time;
                if (duration > max_b) {
                    max_b = duration;
                }
            }
        }
    }
    return max_b;
}