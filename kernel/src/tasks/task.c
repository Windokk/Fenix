#include "tasks/task.h"
#include "memory/heap.h"
#include "string.h"
#include "stdio.h"

/* Defined in task_switch.asm */
extern void task_switch(uint64_t *old_rsp, uint64_t new_rsp);

task_t *current_task = NULL;

static task_t   task_table[MAX_TASKS];
static uint32_t task_count = 0;
static uint32_t next_id    = 0;

void task_init(void) {
    memset(task_table, 0, sizeof(task_table));
    task_count = 0;
    next_id    = 0;
    current_task = NULL;
}

task_t *task_create(void (*fn)(void), const char *name) {
    if (task_count >= MAX_TASKS) {
        printf("TASK: max tasks reached\n");
        return NULL;
    }

    task_t *t = &task_table[task_count++];
    t->id    = next_id++;
    t->state = TASK_READY;

    strncpy(t->name, name, sizeof(t->name) - 1);
    t->name[sizeof(t->name) - 1] = '\0';

    t->stack = (uint8_t *)malloc(TASK_STACK_SIZE);
    if (!t->stack) {
        printf("TASK: failed to allocate stack for '%s'\n", name);
        task_count--;
        return NULL;
    }

    /* Align stack top to 16 bytes (x86-64 ABI requirement) */
    uint64_t *sp = (uint64_t *)((uint64_t)(t->stack + TASK_STACK_SIZE) & ~0xFULL);

    /* Initial stack frame consumed by task_switch on first resume:
     *
     *   [ task_exit ]  <-- fn returns here
     *   [ fn        ]  <-- ret target inside task_switch
     *   [ r15 = 0   ]
     *   [ r14 = 0   ]
     *   [ r13 = 0   ]
     *   [ r12 = 0   ]
     *   [ rbp = 0   ]
     *   [ rbx = 0   ]  <-- t->rsp points here
     */
    *--sp = (uint64_t)task_exit;
    *--sp = (uint64_t)fn;
    *--sp = 0; /* r15 */
    *--sp = 0; /* r14 */
    *--sp = 0; /* r13 */
    *--sp = 0; /* r12 */
    *--sp = 0; /* rbp */
    *--sp = 0; /* rbx */

    t->rsp = (uint64_t)sp;
    return t;
}

task_t *task_locate(const char *name) {
    for (uint32_t i = 0; i < task_count; i++) {
        if (strncmp(task_table[i].name, name, sizeof(task_table[i].name)) == 0)
            return &task_table[i];
    }
    return NULL;
}

/* Pick the next READY task after current, wrapping round-robin. */
static task_t *scheduler_next(void) {
    uint32_t start = (uint32_t)(current_task - task_table + 1) % task_count;
    for (uint32_t i = 0; i < task_count; i++) {
        uint32_t idx = (start + i) % task_count;
        if (task_table[idx].state == TASK_READY ||
            task_table[idx].state == TASK_RUNNING) {
            return &task_table[idx];
        }
    }
    return NULL;
}

void task_yield(void) {
    task_t *prev = current_task;
    task_t *next = scheduler_next();

    if (!next || next == prev) return;

    if (prev->state == TASK_RUNNING)
        prev->state = TASK_READY;

    next->state  = TASK_RUNNING;
    current_task = next;

    task_switch(&prev->rsp, next->rsp);
}

void task_block(void) {
    current_task->state = TASK_BLOCKED;
    task_yield();
}

void task_unblock(task_t *task) {
    if (task && task->state == TASK_BLOCKED)
        task->state = TASK_READY;
}

void task_exit(void) {
    printf("TASK: '%s' exited\n", current_task->name);
    current_task->state = TASK_DEAD;
    task_yield();
    while (1) {} /* unreachable */
}

void scheduler_start(void) {
    if (task_count == 0) return;
    current_task = &task_table[0];
    current_task->state = TASK_RUNNING;
    uint64_t dummy;
    task_switch(&dummy, current_task->rsp);
    /* never returns */
}
