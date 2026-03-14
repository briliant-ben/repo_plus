#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tasks.h"

/* ------------------------------------------------------------------ */
/* Task queue entry                                                     */
/* ------------------------------------------------------------------ */

typedef struct TaskEntry {
  task_fn_t fn;
  void *arg;
  struct TaskEntry *next;
} TaskEntry;

/* ------------------------------------------------------------------ */
/* Pool structure                                                       */
/* ------------------------------------------------------------------ */

struct TaskPool {
  int jobs;     /* max parallel workers */
  int active;   /* currently executing workers */
  int errors;   /* total tasks that returned != 0 */
  int shutdown; /* set to 1 to stop workers */

  TaskEntry *head; /* front of pending queue */
  TaskEntry *tail; /* back of pending queue */
  int queue_len;

  pthread_mutex_t lock;
  pthread_cond_t work_ready; /* signalled when a task is enqueued */
  pthread_cond_t work_done;  /* signalled when a task finishes */

  pthread_t *threads;
};

/* ------------------------------------------------------------------ */
/* Worker thread                                                        */
/* ------------------------------------------------------------------ */

static void *worker(void *arg) {
  TaskPool *pool = (TaskPool *)arg;

  pthread_mutex_lock(&pool->lock);
  while (1) {
    /* Wait for work or shutdown */
    while (!pool->shutdown && pool->head == NULL)
      pthread_cond_wait(&pool->work_ready, &pool->lock);

    if (pool->shutdown && pool->head == NULL)
      break;

    /* Dequeue one task */
    TaskEntry *entry = pool->head;
    pool->head = entry->next;
    if (!pool->head)
      pool->tail = NULL;
    pool->queue_len--;
    pool->active++;

    pthread_mutex_unlock(&pool->lock);

    /* Execute */
    int rc = entry->fn(entry->arg);
    free(entry);

    pthread_mutex_lock(&pool->lock);
    pool->active--;
    if (rc != 0)
      pool->errors++;
    pthread_cond_broadcast(&pool->work_done);
  }
  pthread_mutex_unlock(&pool->lock);
  return NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int task_cpu_count(void) {
  long n = sysconf(_SC_NPROCESSORS_ONLN);
  return (n > 0) ? (int)n : 1;
}

TaskPool *task_pool_create(int jobs) {
  if (jobs <= 0)
    jobs = task_cpu_count();

  TaskPool *pool = calloc(1, sizeof(*pool));
  if (!pool)
    return NULL;

  pool->jobs = jobs;
  pthread_mutex_init(&pool->lock, NULL);
  pthread_cond_init(&pool->work_ready, NULL);
  pthread_cond_init(&pool->work_done, NULL);

  pool->threads = calloc(jobs, sizeof(pthread_t));
  if (!pool->threads) {
    free(pool);
    return NULL;
  }

  for (int i = 0; i < jobs; i++) {
    if (pthread_create(&pool->threads[i], NULL, worker, pool) != 0) {
      /* Shutdown threads we already created */
      pthread_mutex_lock(&pool->lock);
      pool->shutdown = 1;
      pthread_cond_broadcast(&pool->work_ready);
      pthread_mutex_unlock(&pool->lock);
      for (int j = 0; j < i; j++)
        pthread_join(pool->threads[j], NULL);
      free(pool->threads);
      free(pool);
      return NULL;
    }
  }

  return pool;
}

int task_pool_submit(TaskPool *pool, task_fn_t fn, void *arg) {
  TaskEntry *entry = malloc(sizeof(*entry));
  if (!entry)
    return -1;
  entry->fn = fn;
  entry->arg = arg;
  entry->next = NULL;

  pthread_mutex_lock(&pool->lock);

  /* Append to tail */
  if (pool->tail)
    pool->tail->next = entry;
  else
    pool->head = entry;
  pool->tail = entry;
  pool->queue_len++;

  pthread_cond_signal(&pool->work_ready);
  pthread_mutex_unlock(&pool->lock);
  return 0;
}

int task_pool_wait(TaskPool *pool) {
  pthread_mutex_lock(&pool->lock);
  while (pool->head != NULL || pool->active > 0)
    pthread_cond_wait(&pool->work_done, &pool->lock);
  int errors = pool->errors;
  pthread_mutex_unlock(&pool->lock);
  return errors;
}

void task_pool_destroy(TaskPool *pool) {
  if (!pool)
    return;

  /* Signal all workers to exit */
  pthread_mutex_lock(&pool->lock);
  pool->shutdown = 1;
  pthread_cond_broadcast(&pool->work_ready);
  pthread_mutex_unlock(&pool->lock);

  for (int i = 0; i < pool->jobs; i++)
    pthread_join(pool->threads[i], NULL);

  free(pool->threads);
  pthread_mutex_destroy(&pool->lock);
  pthread_cond_destroy(&pool->work_ready);
  pthread_cond_destroy(&pool->work_done);
  free(pool);
}
