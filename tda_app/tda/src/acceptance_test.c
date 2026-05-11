#include <math.h>
#include <stdio.h>

#include "acceptance_test.h"

void utilization_bound_test(struct task_params **params, unsigned int task_id,
			    AcceptanceTestResult results[])
{
	double utilization = 0;
	bool accepted = false;
	/*--------------------------------------------------------------------
	 * TODO 1: Implement the Utilization Bound Test from the lecture
	 ---------------------------------------------------------------------*/
	unsigned int num_tasks = 0;
	for (unsigned int i = 0; i <= task_id; i++) 
	{
		if (i == task_id || results[i].accepted) 
		{
			utilization +=(double)params[i]->execution_time_ms  / (double)params[i]->period_ms;
			num_tasks++;
		}
	}
	//	unsigned int num_tasks = task_id+1;
	double bound = num_tasks * (pow(2.0, 1.0 / num_tasks) - 1.0);

	if (utilization <= bound)
	accepted = true;
	results[task_id].accepted = accepted;
	results[task_id].info.util = utilization;

	printf("Utiliization Test (Task %d): %f (%s)\n", task_id + 1, utilization,
	       accepted ? "accepted" : "rejected");
}

void worst_case_simulation(struct task_params **params, unsigned int task_id,
			   AcceptanceTestResult results[])
{
	int32_t completion_time = 0;
	bool accepted = false;
	
	/*--------------------------------------------------------------------
	 * TODO 2: Implement the Worst Case Simulation from the lecture
	 ---------------------------------------------------------------------*/
	int32_t T_execution_time = params[task_id]->execution_time_ms;
	int32_t T_deadline = params[task_id]->period_ms;
	int32_t T_pre = T_execution_time;
	int32_t T_suc = 0;

	while (1) 
	{
		T_suc = T_execution_time;
		for (unsigned int i = 0; i < task_id; i++) 
		{
			if (results[i].accepted) 
			{
				T_suc += (int32_t)ceil((double)T_pre /params[i]->period_ms) * params[i]->execution_time_ms;
			}
		}
		if (T_suc == T_pre)
			break;
		if (T_suc > T_deadline)
			break;
		T_pre = T_suc;
	}
	completion_time = T_suc;
	accepted = (completion_time <= T_deadline);
	// ___________________________ END _____________________________________
	results[task_id].accepted = accepted;
	results[task_id].info.wcs_result = completion_time;

	printf("Worst Case Simulation (Task %d): %d ms (%s)\n", task_id + 1, completion_time,
	       accepted ? "accepted" : "rejected");
}

void time_demand_analysis(struct task_params **params, unsigned int task_id, AcceptanceTestResult results[])
{
	int32_t t_next = 0;
	bool accepted = false;

	/*--------------------------------------------------------------------
	 * TODO 3: Implement the Time Demand Analysis from the lecture
	 ---------------------------------------------------------------------*/
	int32_t execution_T1 = params[task_id]->execution_time_ms;
	int32_t Deadline_T1 = params[task_id]->period_ms;
	for (int32_t i = execution_T1; i <= Deadline_T1; i++) 
	{
		int32_t t1 = execution_T1;
		for (unsigned int j = 0; j < task_id; j++) {
			if (results[j].accepted) 
			{
				t1 += (int32_t)ceil((double)i /params[j]->period_ms) * params[j]->execution_time_ms;
			}
		}

		if (t1 <= i) 
		{
			accepted = true;
			t_next = i;
			break;
		}
	}
	if (!accepted)
		t_next = -1;

	results[task_id].accepted = accepted;
	results[task_id].info.tda_result = t_next;

	printf("Time Demand Analysis (Task %d): %d ms (%s)\n", task_id + 1, t_next,
	       accepted ? "accepted" : "rejected");
}

/* Determine if params[task_id] can be scheduled.
 * - params: array of all task parameters (e.g., needed to perform TDA)
 * - task_id: index such that params[task_id] is the task under consideration
 * - results: output parameter yielding the acceptance test result */
void acceptance_test(struct task_params **params, unsigned int task_id,
		     AcceptanceTestResult results[])
{
	/*--------------------------------------------------------------------
	 * TODO 4: Call the above acceptance tests in a suitable order.
	 *  In particular, recall which of these tests are necessary,
	 *  sufficient, or both.
	 *  Ensure that the final value of result->accepted is true if and
	 *  only if the task encoded by params[task_id] can be scheduled.
	 ---------------------------------------------------------------------*/
	utilization_bound_test(params, task_id, results);
	if (results[task_id].accepted)
		return;
	worst_case_simulation(params, task_id, results);
	if (results[task_id].accepted)
		return;
	time_demand_analysis(params, task_id, results);

}
