/*
 * uACPI kernel API implementation for the Fenix kernel.
 * Provides all uacpi_kernel_* callbacks required by kernel_api.h.
 */

#include <uacpi/uacpi.h>
#include <uacpi/kernel_api.h>
#include <uacpi/status.h>
#include <uacpi/types.h>

#include "acpi/acpi.h"

#include "memory/heap.h"
#include "memory/paging_pages.h"

#include "drivers/serial.h"
#include "drivers/rtc.h"

#include "tasks/task.h"

#include "stdio.h"
#include "string.h"

bool g_uacpi_ready = false;

// RSDP

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out) {
    if (!g_rsdp_phys) return UACPI_STATUS_NOT_FOUND;
    *out = (uacpi_phys_addr)g_rsdp_phys;
    return UACPI_STATUS_OK;
}

// Memory mapping

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    uint64_t offset       = (uint64_t)addr & 0xFFF;
    uint64_t phys_aligned = (uint64_t)addr & ~(uint64_t)0xFFF;

    /* Low memory: already identity-mapped in the kernel VMA */
    if (phys_aligned < (8ULL << 20))
        return (void *)(TO_VMA_U64(phys_aligned) + offset);

    /* Address outside known windows — try to expand the ACPI table window
     * on-the-fly by re-mapping to cover this region, then use the MMIO pool
     * as a fallback. */
    void *mmio = paging_map_mmio((uint64_t)addr, (uint64_t)len);
    if (!mmio)
        serial_print("[uACPI] paging_map_mmio returned NULL!\n");
    return mmio;
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    uint64_t v = (uint64_t)addr & ~(uint64_t)0xFFF;

    /* Low-memory or ACPI table window: statically mapped, nothing to do */
    if (v >= KERNEL_START_MEMORY && v < KERNEL_START_MEMORY + (8ULL << 20))
        return;

    /* MMIO pool: free the pages */
    if (v >= MMIO_POOL_BASE &&
        v <  MMIO_POOL_BASE + (uint64_t)MMIO_POOL_PAGES * PAGE_SIZE)
        paging_unmap_mmio(addr, (uint64_t)len);
}

// Logging

void uacpi_kernel_log(uacpi_log_level level, const uacpi_char *msg) {
    /* Only forward WARN and ERROR to keep boot output clean. */
    const char *prefix;
    switch (level) {
    case UACPI_LOG_ERROR: prefix = "[uACPI ERROR] " ; break;
    case UACPI_LOG_WARN:  prefix = "[uACPI WARN] "  ; break;
    default:              return;
    }
    printf("%s%s", prefix, msg);
}

// Heap allocation

void *uacpi_kernel_alloc(uacpi_size size) {
    return kmalloc((unsigned long)size);
}

void uacpi_kernel_free(void *mem) {
    if (mem) kfree(mem);
}

// Timing

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    /* RTC at 2 Hz => 1 tick = 500,000,000 ns */
    return rtc_get_ticks() * 500000000ULL;
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    /* Crude busy-wait: ~200 pause iterations per microsecond */
    volatile uint32_t n = (uint32_t)usec * 200;
    while (n--) asm volatile("pause");
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
    if (current_task) {
        /* 2 Hz RTC => 1 tick ≈ 500 ms; round up so minimum sleep is 1 tick */
        uint64_t ticks = (msec + 499) / 500;
        if (ticks == 0) ticks = 1;
        task_sleep(ticks);
    } else {
        /* Scheduler not yet running - busy spin (4 stalls × 250 µs = 1 ms) */
        for (uint64_t i = 0; i < msec; i++)
            uacpi_kernel_stall(250);
    }
}

// Mutex

uacpi_handle uacpi_kernel_create_mutex(void) {
    mutex_t *m = (mutex_t *)kmalloc(sizeof(mutex_t));
    if (m) mutex_init(m);
    return (uacpi_handle)m;
}

void uacpi_kernel_free_mutex(uacpi_handle handle) {
    kfree(handle);
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle, uacpi_u16 timeout) {
    mutex_t *m = (mutex_t *)handle;
    if (!m) return UACPI_STATUS_INVALID_ARGUMENT;

    if (timeout == 0) {
        /* Non-blocking single attempt */
        return __sync_lock_test_and_set(&m->locked, 1)
               ? UACPI_STATUS_TIMEOUT : UACPI_STATUS_OK;
    }
    /* Blocking (including infinite 0xFFFF): spin-yield until acquired */
    mutex_lock(m);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle handle) {
    mutex_t *m = (mutex_t *)handle;
    if (m) mutex_unlock(m);
}

// Event (counting semaphore)

uacpi_handle uacpi_kernel_create_event(void) {
    semaphore_t *s = (semaphore_t *)kmalloc(sizeof(semaphore_t));
    if (s) sem_init(s, 0);
    return (uacpi_handle)s;
}

void uacpi_kernel_free_event(uacpi_handle handle) {
    kfree(handle);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle, uacpi_u16 timeout) {
    semaphore_t *s = (semaphore_t *)handle;
    if (!s) return UACPI_FALSE;

    if (timeout == 0) {
        int32_t old = __atomic_load_n(&s->count, __ATOMIC_ACQUIRE);
        if (old > 0 &&
            __atomic_compare_exchange_n(&s->count, &old, old - 1,
                                        false, __ATOMIC_ACQ_REL,
                                        __ATOMIC_RELAXED))
            return UACPI_TRUE;
        return UACPI_FALSE;
    }

    sem_wait(s);
    return UACPI_TRUE;
}

void uacpi_kernel_signal_event(uacpi_handle handle) {
    semaphore_t *s = (semaphore_t *)handle;
    if (s) sem_signal(s);
}

void uacpi_kernel_reset_event(uacpi_handle handle) {
    semaphore_t *s = (semaphore_t *)handle;
    if (s) __atomic_store_n(&s->count, 0, __ATOMIC_RELAXED);
}

// Thread identity

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    /* UACPI_THREAD_ID_NONE == (void *)-1; never return that */
    if (current_task) return (uacpi_thread_id)current_task;
    return (uacpi_thread_id)1;
}

// Interrupt state

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) {
    unsigned long flags;
    asm volatile("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
    return (uacpi_interrupt_state)flags;
}

void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state) {
    asm volatile("push %0; popfq" :: "r"((unsigned long)state) : "memory");
}

// Spinlock

typedef struct { volatile int locked; } spinlock_impl_t;

uacpi_handle uacpi_kernel_create_spinlock(void) {
    spinlock_impl_t *s = (spinlock_impl_t *)kmalloc(sizeof(spinlock_impl_t));
    if (s) s->locked = 0;
    return (uacpi_handle)s;
}

void uacpi_kernel_free_spinlock(uacpi_handle handle) {
    kfree(handle);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
    spinlock_impl_t *s = (spinlock_impl_t *)handle;
    uacpi_cpu_flags flags = uacpi_kernel_disable_interrupts();
    if (s)
        while (__sync_lock_test_and_set(&s->locked, 1))
            asm volatile("pause");
    return flags;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) {
    spinlock_impl_t *s = (spinlock_impl_t *)handle;
    if (s) __sync_lock_release(&s->locked);
    uacpi_kernel_restore_interrupts(flags);
}

// Firmware requests

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *req) {
    switch (req->type) {
    case UACPI_FIRMWARE_REQUEST_TYPE_BREAKPOINT:
        KLOG_WARN("uACPI", "AML Breakpoint\n");
        return UACPI_STATUS_OK;
    case UACPI_FIRMWARE_REQUEST_TYPE_FATAL:
        KLOG_FATAL("uACPI", "AML Fatal: type=0x%X code=0x%X\n",
               req->fatal.type, req->fatal.code);
        return UACPI_STATUS_OK;
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
    }
}

// SCI interrupt routing

static uacpi_interrupt_handler g_sci_handler = NULL;
static uacpi_handle            g_sci_ctx     = NULL;

void uacpi_sci_dispatch(void) {
    if (g_sci_handler)
        g_sci_handler(g_sci_ctx);
    else
        acpi_sci_handle();
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler handler,
    uacpi_handle ctx, uacpi_handle *out_handle)
{
    (void)irq;
    g_sci_handler = handler;
    g_sci_ctx     = ctx;
    *out_handle   = (uacpi_handle)(uintptr_t)irq;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler handler, uacpi_handle irq_handle)
{
    (void)handler; (void)irq_handle;
    g_sci_handler = NULL;
    g_sci_ctx     = NULL;
    return UACPI_STATUS_OK;
}

// PCI configuration space

typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
} pci_device_impl_t;

static uint32_t pci_cf8_addr(uint8_t bus, uint8_t dev, uint8_t fn, uint32_t off) {
    return 0x80000000u
         | ((uint32_t)bus         << 16)
         | ((uint32_t)(dev & 31u) << 11)
         | ((uint32_t)(fn  &  7u) <<  8)
         | (off & 0xFC);
}

static uint32_t pci_read32(pci_device_impl_t *d, uint32_t off) {
    outl(0xCF8, pci_cf8_addr(d->bus, d->device, d->function, off));
    return inl(0xCFC);
}

static void pci_write32(pci_device_impl_t *d, uint32_t off, uint32_t val) {
    outl(0xCF8, pci_cf8_addr(d->bus, d->device, d->function, off));
    outl(0xCFC, val);
}

uacpi_status uacpi_kernel_pci_device_open(
    uacpi_pci_address address, uacpi_handle *out_handle)
{
    /* Only support segment 0 via legacy I/O port mechanism */
    if (address.segment != 0) return UACPI_STATUS_NOT_FOUND;

    pci_device_impl_t *d = (pci_device_impl_t *)kmalloc(sizeof(pci_device_impl_t));
    if (!d) return UACPI_STATUS_OUT_OF_MEMORY;

    d->bus      = (uint8_t)address.bus;
    d->device   = (uint8_t)address.device;
    d->function = (uint8_t)address.function;
    *out_handle = (uacpi_handle)d;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle) { kfree(handle); }

uacpi_status uacpi_kernel_pci_read8(
    uacpi_handle h, uacpi_size off, uacpi_u8 *out)
{
    uint32_t dw = pci_read32((pci_device_impl_t *)h, (uint32_t)off);
    *out = (uacpi_u8)(dw >> ((off & 3u) * 8u));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(
    uacpi_handle h, uacpi_size off, uacpi_u16 *out)
{
    uint32_t dw = pci_read32((pci_device_impl_t *)h, (uint32_t)off);
    *out = (uacpi_u16)(dw >> ((off & 2u) * 8u));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(
    uacpi_handle h, uacpi_size off, uacpi_u32 *out)
{
    *out = pci_read32((pci_device_impl_t *)h, (uint32_t)off);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(
    uacpi_handle h, uacpi_size off, uacpi_u8 val)
{
    pci_device_impl_t *d = (pci_device_impl_t *)h;
    uint32_t dw = pci_read32(d, (uint32_t)off);
    uint32_t sh = (off & 3u) * 8u;
    pci_write32(d, (uint32_t)off,
                (dw & ~(0xFFu << sh)) | ((uint32_t)val << sh));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(
    uacpi_handle h, uacpi_size off, uacpi_u16 val)
{
    pci_device_impl_t *d = (pci_device_impl_t *)h;
    uint32_t dw = pci_read32(d, (uint32_t)off);
    uint32_t sh = (off & 2u) * 8u;
    pci_write32(d, (uint32_t)off,
                (dw & ~(0xFFFFu << sh)) | ((uint32_t)val << sh));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(
    uacpi_handle h, uacpi_size off, uacpi_u32 val)
{
    pci_write32((pci_device_impl_t *)h, (uint32_t)off, val);
    return UACPI_STATUS_OK;
}

// I/O port mapping

typedef struct { uacpi_io_addr base; } io_range_impl_t;

uacpi_status uacpi_kernel_io_map(
    uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle)
{
    (void)len;
    io_range_impl_t *r = (io_range_impl_t *)kmalloc(sizeof(io_range_impl_t));
    if (!r) return UACPI_STATUS_OUT_OF_MEMORY;
    r->base     = base;
    *out_handle = (uacpi_handle)r;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) { kfree(handle); }

uacpi_status uacpi_kernel_io_read8(
    uacpi_handle h, uacpi_size off, uacpi_u8 *out)
{
    *out = inb((uint16_t)(((io_range_impl_t *)h)->base + off));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(
    uacpi_handle h, uacpi_size off, uacpi_u16 *out)
{
    *out = inw((uint16_t)(((io_range_impl_t *)h)->base + off));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(
    uacpi_handle h, uacpi_size off, uacpi_u32 *out)
{
    *out = inl((uint16_t)(((io_range_impl_t *)h)->base + off));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(
    uacpi_handle h, uacpi_size off, uacpi_u8 val)
{
    outb((uint16_t)(((io_range_impl_t *)h)->base + off), val);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(
    uacpi_handle h, uacpi_size off, uacpi_u16 val)
{
    outw((uint16_t)(((io_range_impl_t *)h)->base + off), val);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(
    uacpi_handle h, uacpi_size off, uacpi_u32 val)
{
    outl((uint16_t)(((io_range_impl_t *)h)->base + off), val);
    return UACPI_STATUS_OK;
}

// Work scheduling (synchronous - no thread pool yet)

uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type type, uacpi_work_handler handler, uacpi_handle ctx)
{
    (void)type;
    handler(ctx);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    return UACPI_STATUS_OK;
}
