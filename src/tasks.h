#ifndef TASKS_H
#define TASKS_H

/*
 * tasks.h - Simple parallel task runner using pthreads.
 *
 * Usage:
 *   TaskPool *pool = task_pool_create(njobs);
 *   for each work item:
 *       task_pool_submit(pool, my_func, arg);
 *   task_pool_wait(pool);
 *   task_pool_destroy(pool);
 */

typedef struct TaskPool TaskPool;

/* Work function signature: returns 0 on success, non-zero on error. */
typedef int (*task_fn_t)(void *arg);

/* Create a pool with at most 'jobs' parallel workers. */
TaskPool *task_pool_create(int jobs);

/*
 * Submit a task.  May block if the pool is full.
 * Returns 0 on success.
 */
int task_pool_submit(TaskPool *pool, task_fn_t fn, void *arg);

/*
 * Wait for all submitted tasks to complete.
 * Returns the number of tasks that returned non-zero.
 */
int task_pool_wait(TaskPool *pool);

/* Destroy the pool (must call task_pool_wait first). */
void task_pool_destroy(TaskPool *pool);

/* Returns the number of logical CPUs available. */
int task_cpu_count(void);

#endif /* TASKS_H */
