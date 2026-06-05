#pragma once

#include "common.h"

#include "memory/pool.h"

#define TASK_STACK_SIZE     16384
#define MAX_TASKS           16
#define STACK_CANARY_MAGIC  0xDEADC0DEDEADC0DEULL

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD,
} task_state_t;

typedef struct task_struct {
    uint64_t     rsp;          /* saved stack pointer — must stay first (asm depends on offset 0) */
    task_state_t state;
    uint32_t     id;
    char         name[32];
    uint8_t     *stack;        /* heap-allocated stack base */
    uint64_t     wakeup_tick;  /* non-zero: wake when rtc_get_ticks() >= this value */
} task_t;

extern task_t *current_task;

extern pool_t* stack_pool;

void    task_init(void);
task_t *task_create(void (*fn)(void), const char *name);
task_t *task_locate(const char *name);
void    task_yield(void);
void    task_block(void);
void    task_unblock(task_t *task);
void    task_exit(void);
void    task_sleep(uint64_t ticks);      /* block for N RTC ticks then auto-wake */
void    task_tick_update(void);          /* called from timer ISR to wake sleeping tasks */
void    scheduler_start(void);
void    task_canary_check_all(void);     /* check every live task's stack canary */

// ─── Mutex ────────────────────────────────────────────────────────────────────

#define MUTEX_MAX_WAITERS 8

typedef struct {
    volatile int  locked;
    task_t       *owner;
    task_t       *waiters[MUTEX_MAX_WAITERS];
    uint32_t      waiter_count;
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);    /* spin-yields until acquired */
void mutex_unlock(mutex_t *m);

// ─── Semaphore ────────────────────────────────────────────────────────────────

typedef struct {
    volatile int32_t count;
} semaphore_t;

void sem_init(semaphore_t *s, int32_t initial);
void sem_wait(semaphore_t *s);    /* P: decrement; spin-yields if count <= 0 */
void sem_signal(semaphore_t *s);  /* V: increment */