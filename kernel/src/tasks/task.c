#include "tasks/task.h"
#include "drivers/rtc.h"
#include "memory/heap.h"
#include "string.h"
#include "stdio.h"
#include "cpu/smp.h"
#include "interrupts/idt.h"
#include "interrupts/apic.h"
#include "kernel.h"

/* Defined in task_switch.asm */
extern void task_switch(uint64_t *old_rsp, uint64_t new_rsp);
extern void task_trampoline();

void idle_task(void) {
    while (1) {
        x86_hlt();
        task_yield_local();
    }
}

void task_init_local(void) {
    uint32_t cpu_id = cpu_self_id();
    memset(cpu_local[cpu_id].run_queue, 0, sizeof(cpu_local[cpu_id].run_queue));
    cpu_local[cpu_id].run_count = 0;
    cpu_local[cpu_id].next_id    = 0;
    cpu_local[cpu_id].current_task = NULL;

    void* backing = kmalloc_aligned(MAX_TASKS * TASK_STACK_SIZE, 16);
    cpu_local[cpu_id].stack_pool = (pool_t*)kmalloc(sizeof(pool_t));
    pool_init(cpu_local[cpu_id].stack_pool, backing, MAX_TASKS * TASK_STACK_SIZE, TASK_STACK_SIZE);
}

task_t *task_create_local(void (*fn)(void), const char *name) {
    uint32_t cpu_id = cpu_least_loaded();

    uint32_t run_count = cpu_local[cpu_id].run_count;

    // Prefer reusing a dead slot so the table doesn't fill up permanently.
    task_t *t = NULL;
    for (uint32_t i = 0; i < cpu_local[cpu_id].run_count; i++) {
        if (cpu_local[cpu_id].run_queue[i].state == TASK_DEAD) {
            if (cpu_local[cpu_id].run_queue[i].stack) {
                pool_free(cpu_local[cpu_id].stack_pool, cpu_local[cpu_id].run_queue[i].stack);
                cpu_local[cpu_id].run_queue[i].stack = NULL;
            }
            t = &cpu_local[cpu_id].run_queue[i];
            break;
        }
    }
    if (!t) {
        if (cpu_local[cpu_id].run_count >= MAX_TASKS) {
            KLOG_WARN("TASK","Max tasks reached");
            return NULL;
        }
        t = &cpu_local[cpu_id].run_queue[cpu_local[cpu_id].run_count++];
    }

    t->id          = cpu_local[cpu_id].next_id++;
    t->state       = TASK_READY;
    t->wakeup_tick = 0;

    strncpy(t->name, name, sizeof(t->name) - 1);
    t->name[sizeof(t->name) - 1] = '\0';

    t->stack = pool_alloc(cpu_local[cpu_id].stack_pool);
    if (!t->stack) {
        KLOG_ERROR("TASK","Failed to allocate stack for '%s'", name);
        t->state = TASK_DEAD;
        return NULL;
    }

    /* Stack canary at the lowest address — first word overwritten on overflow */
    *(uint64_t *)t->stack = STACK_CANARY_MAGIC;

    /* Align stack top to 16 bytes (x86-64 ABI requirement) */
    uint64_t *sp = (uint64_t *)((uint64_t)(t->stack + TASK_STACK_SIZE) & ~0xFULL);

    /* Initial stack frame consumed by task_switch on first resume:
    *   [ task_exit ]  <-- fn returns here
    *   [ task_trampoline ]  <-- ret target inside task_switch
    *   [ rbx = fn ] [ rbp = 0 ] [ r12 = 0 ] [ r13 = 0 ] [ r14 = 0 ] [ r15 = 0 ]  <-- t->rsp
    */
    *--sp = (uint64_t)task_exit_local;
    *--sp = (uint64_t)task_trampoline;
    *--sp = (uint64_t)fn;/* rbx <==> fn */
    *--sp = 0; /* rbp */
    *--sp = 0; /* r12 */
    *--sp = 0; /* r13 */
    *--sp = 0; /* r14 */
    *--sp = 0; /* r15 */

    t->rsp = (uint64_t)sp;

    if(run_count == 0){
        int lapic_id = g_acpi.cpus[cpu_id].apic_id;
        lapic_send_ipi(lapic_id, IPI_RESCHEDULE);
    }

    return t;
}

task_t *task_locate_local(const char *name) {
    uint32_t cpu_id = cpu_self_id();
    for (uint32_t i = 0; i < cpu_local[cpu_id].run_count; i++) {
        if (strncmp(cpu_local[cpu_id].run_queue[i].name, name, sizeof(cpu_local[cpu_id].run_queue[i].name)) == 0)
            return &cpu_local[cpu_id].run_queue[i];
    }
    return NULL;
}

/* Round-robin: pick the next READY task after the current one. */
static task_t *scheduler_next_local(void) {
    uint32_t cpu_id = cpu_self_id();
    lock(&cpu_local[cpu_id].lock);
    if(cpu_local[cpu_id].run_count == 0){
        unlock(&cpu_local[cpu_id].lock);
        return NULL;
    }

    uint32_t start = (uint32_t)(cpu_local[cpu_id].current_task - cpu_local[cpu_id].run_queue + 1) % cpu_local[cpu_id].run_count;
    for (uint32_t i = 0; i < cpu_local[cpu_id].run_count; i++) {
        uint32_t idx = (start + i) % cpu_local[cpu_id].run_count;
        if (cpu_local[cpu_id].run_queue[idx].state == TASK_READY ||
            cpu_local[cpu_id].run_queue[idx].state == TASK_RUNNING) {
            unlock(&cpu_local[cpu_id].lock);
            return &cpu_local[cpu_id].run_queue[idx];
        }
    }

    unlock(&cpu_local[cpu_id].lock);

    return NULL;
}

static inline void canary_check(task_t *t) {
    if (*(uint64_t *)t->stack != STACK_CANARY_MAGIC)
        HALT_AND_CATCH_FIRE("Stack overflow detected in task '%s' (id=%u)", t->name, t->id);
}

void task_yield_local(void) {
    uint64_t rflags;
    asm volatile("pushfq; pop %0; cli" : "=r"(rflags));

    uint32_t cpu_id = cpu_self_id();

    task_t *prev = cpu_local[cpu_id].current_task;
    if (!prev) { enable_interrupts(); return; }
    canary_check(prev);
    task_t *next = scheduler_next_local();

    if (!next || next == prev){
        if(rflags & (1ULL << 9))
            enable_interrupts();
        return;
    }

    if (prev->state == TASK_RUNNING)
        prev->state = TASK_READY;

    next->state  = TASK_RUNNING;
    cpu_local[cpu_id].current_task = next;

    task_switch(&prev->rsp, next->rsp);

    if(rflags & (1ULL << 9))
        enable_interrupts();
}

void task_block_local(void) {
    uint32_t cpu_id = cpu_self_id();
    cpu_local[cpu_id].current_task->state = TASK_BLOCKED;
    task_yield_local();
}

void task_unblock_local(task_t *task) {
    if (task && task->state == TASK_BLOCKED)
        task->state = TASK_READY;
}

void task_exit_local(void) {
    uint32_t cpu_id = cpu_self_id();
    canary_check(cpu_local[cpu_id].current_task);
    KLOG_INFO("TASK","'%s' exited", cpu_local[cpu_id].current_task->name);
    // Stack is freed when this slot is reused by task_create().
    cpu_local[cpu_id].current_task->state = TASK_DEAD;
    task_yield_local();
    while (1) {}
}

void task_assign_local(task_t *t, uint32_t cpu_id){
    lock(&cpu_local[cpu_id].lock);
    cpu_local[cpu_id].run_queue[cpu_local[cpu_id].run_count] = *t;
    cpu_local[cpu_id].run_count++;
    unlock(&cpu_local[cpu_id].lock);
}

// ─── Timed sleep ──────────────────────────────────────────────────────────────

void task_sleep_local(uint64_t ticks) {
    uint32_t cpu_id = cpu_self_id();
    cpu_local[cpu_id].current_task->wakeup_tick = rtc_get_ticks() + ticks;
    task_block_local();
}

/* Called from the timer ISR (with EOI already sent) to wake sleeping tasks. */
void task_tick_update_local(void) {
    uint32_t cpu_id = cpu_self_id();
    uint64_t now = rtc_get_ticks();
    for (uint32_t i = 0; i < cpu_local[cpu_id].run_count; i++) {
        task_t *t = &cpu_local[cpu_id].run_queue[i];
        if (t->state == TASK_BLOCKED && t->wakeup_tick > 0 &&
            now >= t->wakeup_tick) {
            t->wakeup_tick = 0;
            task_unblock_local(t);
        }
    }
}

// ─── Stack canary audit ───────────────────────────────────────────────────────

void task_canary_check_all_local(void) {
    uint32_t cpu_id = cpu_self_id();
    for (uint32_t i = 0; i < cpu_local[cpu_id].run_count; i++) {
        task_t *t = &cpu_local[cpu_id].run_queue[i];
        if (t->state != TASK_DEAD && t->stack)
            canary_check(t);
    }
}

// ─── Scheduler start ──────────────────────────────────────────────────────────

void scheduler_start_local(void) {
    uint32_t cpu_id = cpu_self_id();
    if (cpu_local[cpu_id].run_count == 0) return;
    cpu_local[cpu_id].current_task = &cpu_local[cpu_id].run_queue[0];
    cpu_local[cpu_id].current_task->state = TASK_RUNNING;
    uint64_t dummy;
    task_switch(&dummy, cpu_local[cpu_id].current_task->rsp);
    /* never returns */
}

// ─── Mutex (spin-yield) ───────────────────────────────────────────────────────

void mutex_init(mutex_t *m) {
    m->locked       = 0;
    m->owner        = NULL;
    m->waiter_count = 0;
}

void mutex_lock(mutex_t *m) {
    uint32_t cpu_id = cpu_self_id();
    while (__sync_lock_test_and_set(&m->locked, 1)) {
        /* Lock is held — yield to let the holder run, then retry. */
        if (cpu_local[cpu_id].current_task) task_yield_local();
        else asm volatile("pause");
    }
    m->owner = cpu_local[cpu_id].current_task;
}

void mutex_unlock(mutex_t *m) {
    m->owner = NULL;
    __sync_lock_release(&m->locked);
}

// ─── Semaphore (spin-yield) ───────────────────────────────────────────────────

void sem_init(semaphore_t *s, int32_t initial) {
    __atomic_store_n(&s->count, initial, __ATOMIC_RELAXED);
}

void sem_wait(semaphore_t *s) {
    uint32_t cpu_id = cpu_self_id();
    while (1) {
        int32_t old = __atomic_load_n(&s->count, __ATOMIC_ACQUIRE);
        if (old > 0 &&
            __atomic_compare_exchange_n(&s->count, &old, old - 1,
                                        false, __ATOMIC_ACQ_REL,
                                        __ATOMIC_RELAXED))
            return;
        if (cpu_local[cpu_id].current_task) task_yield_local();
        else asm volatile("pause");
    }
}

void sem_signal(semaphore_t *s) {
    __atomic_fetch_add(&s->count, 1, __ATOMIC_RELEASE);
}
