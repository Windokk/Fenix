#pragma once

#include "common.h"

#define TASK_STACK_SIZE  16384
#define MAX_TASKS        16

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD,
} task_state_t;

typedef struct task_struct {
    uint64_t     rsp;        /* saved stack pointer — must stay first (asm depends on offset 0) */
    task_state_t state;
    uint32_t     id;
    char         name[32];
    uint8_t     *stack;      /* heap-allocated stack base */
} task_t;

extern task_t *current_task;

void    task_init(void);
task_t *task_create(void (*fn)(void), const char *name);
task_t *task_locate(const char *name);
void    task_yield(void);
void    task_block(void);
void    task_unblock(task_t *task);
void    task_exit(void);
void    scheduler_start(void);