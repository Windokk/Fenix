<div align="center">

<img src="github/fenix-logo.png" alt="Logo" width="200"/>

# Welcome to Fenix !
<br>
<img src="github/shield.svg" alt="shield"/>
<br>

### A Unix-like operating system (x84_64)
<br>
</div>

## How to run : 

- Build a cross-compiler (x86_64_elf) : gcc, binutils
- Install xorriso, nasm, grub2 and qemu
- Clone the project
- Init submodules (```git submodule update --init --recursive```)
- Modify line 9 of kernel/vendor/uacpi/include/uacpi/platform/types.h : 
    ```c
    #ifdef UACPI_OVERRIDE_TYPES
    #include "acpi/uacpi_types.h"
    #else
    ```
- Execute run.sh

## Roadmap (Last Updated May 2026)

### Phase 1 - Foundation : Done ■■■■■■■■■■ 100%
- [x] Multiboot2 boot via GRUB 2
- [x] 32-bit → 64-bit long mode transition
- [x] Higher-half kernel mapping (PML4 paging)
- [x] GDT + TSS (64-bit)
- [x] IDT + 8259 PIC interrupt handling
- [x] Physical frame allocator
- [x] Virtual memory (4 KB pages) — kernel, MMIO pool, ACPI table window, APIC region
- [x] Kernel heap
- [x] Serial driver (debug output)
- [x] VESA framebuffer (mode info + virtual address mapping)
- [x] PS/2 controller init
- [x] PS/2 keyboard driver (scancode → ASCII, ring buffer, modifier keys)
- [x] PS/2 mouse driver (3-byte packets, cursor rendering)
- [x] Graphics primitives (pixel, line, rectangle, filled rectangle, circle, filled circle)
- [x] Bitmap font (NotoSans / Perfect_DOS_VGA_437, 16 px, 4 bpp)

### Phase 2 - Visible Kernel : Done ■■■■■■■■■■ 100%
- [x] Text rendering (`gfx_draw_text` using bitmap font)
- [x] `printf` output redirected to framebuffer (alongside serial)
- [x] RTC driver (read real-time clock)
- [x] PIT driver (timer interrupts, basic tick counter)
- [x] Basic on-screen debug console (scrollable text output)

### Phase 3 - Interactive Shell : Done ■■■■■■■▢▢▢ 70%
- [x] Keyboard input line editor (backspace, cursor, history)
- [x] Simple command parser / REPL
- [x] Basic built-in commands (`clear`, `help`, `mem`, `time`, `echo`, `shutdown`, `reboot`)
- [ ] Mouse integration in UI (clickable elements)

### Phase 4 - Multitasking : Done ■■■■■■■■■■ 100%
- [x] Task control block (TCB) and kernel threads
- [x] Context switch (register save/restore, TSS stack swap)
- [x] Round-robin scheduler with `sleep` / yield
- [x] Mutex (spin-yield, owner tracking, waiter queue)
- [x] Semaphore (P/V, spin-yield)

### Phase 5 - ACPI & Advanced Interrupts : Done ■■■■■■■■■■ 100%
- [x] RSDP discovery from Multiboot2 tags (ACPI 1.0 RSDT + 2.0 XSDT)
- [x] MADT parsing: LAPIC, I/O APIC, interrupt source overrides (ISOs), LAPIC NMI
- [x] FADT parsing: PM1 control / event blocks, reset register, SCI IRQ
- [x] HPET table parsing (base address, minimum tick)
- [x] MCFG table parsing (PCIe ECAM segments)
- [x] uACPI integration (namespace init, `_STA` / `_PRS` / `_CRS` evaluation)
- [x] uACPI `uacpi_enter_sleep_state` for S3/S4/S5 (suspend / hibernate / shutdown)
- [x] 16 MB ACPI table window + dynamic MMIO pool for uACPI memory operations
- [x] LAPIC: MMIO mapping, enable, SVR, EOI, NMI LVT wiring
- [x] I/O APIC: MMIO mapping, redirection table programming, mask/unmask per GSI
- [x] SCI IRQ routing through I/O APIC, PM1 fixed-event handler
- [x] Graceful fallback to legacy 8259 PIC when APIC is unavailable

### Phase 6 - Kernel Hardening & Observability : Done ■■■■■▢▢▢▢▢ 50%
- [x] LAPIC timer calibration against ACPI PM Timer => preemptive scheduling tick
- [x] HPET driver (high-resolution monotonic clock source)
- [x] Kernel stack-canary support (magic value at stack bottom, checked on yield/exit)
- [x] Slab / pool allocator to reduce heap fragmentation
- [x] Kernel profiling stubs (cycle-count hooks around scheduler, ISR, heap)
- [ ] Improved debug console: filter by subsystem, severity levels
- [ ] Kernel self-test framework (run at boot, report via serial)

### Phase 7 - SMP & CPU Topology : Done ▢▢▢▢▢▢▢▢▢▢ 0%
- [ ] ACPI CPU enumeration from MADT (all enabled LAPICs)
- [ ] AP startup sequence (INIT–SIPI–SIPI via LAPIC ICR)
- [ ] Per-CPU GDT / TSS / IDT and stack
- [ ] Per-CPU scheduler queues + inter-processor IPI for load balance
- [ ] Spinlock primitive (distinct from mutex — for IRQ-context use)
- [ ] `cpuid`-based feature detection (XSAVE, invariant TSC, etc.)

### Phase 8 - PCI / PCIe Enumeration : Done ▢▢▢▢▢▢▢▢▢▢ 0%
- [ ] ECAM-based PCIe config space walker
- [ ] PCI device/vendor database (compact table in kernel)
- [ ] Generic PCI driver model (probe / remove callbacks)
- [ ] MSI / MSI-X allocation (vector management, LAPIC targeting)
- [ ] virtio-pci transport (QEMU-first: blk, net, gpu)

### Phase 9 - Userspace : Done ▢▢▢▢▢▢▢▢▢▢ 0%
- [ ] Syscall interface (`syscall` / `sysret`, syscall dispatch table)
- [ ] Per-process page tables (CR3 switch on context switch)
- [ ] User-mode task creation (ring 3 jump, user stack setup)
- [ ] Minimal user-mode C runtime (stack frame, `main` wrapper)
- [ ] `brk` / `mmap` anonymous memory for user heap

### Phase 10 - Storage & Filesystem : Done ▢▢▢▢▢▢▢▢▢▢ 0%
- [ ] ATA PIO driver (or virtio-blk for QEMU)
- [ ] AHCI driver (SATA, DMA transfers)
- [ ] FAT32 filesystem (read-only first, then writable)
- [ ] VFS abstraction layer (inode, dentry, file-ops)
- [ ] Simple ramfs / initrd for early boot

### Phase 11 - POSIX & Beyond : Done ▢▢▢▢▢▢▢▢▢▢ 0%
- [ ] `fork` / `exec` / `waitpid`
- [ ] File descriptors (`open`, `read`, `write`, `close`, `dup2`)
- [ ] ELF binary loader (static executables)
- [ ] Anonymous pipes and basic IPC
- [ ] Signals (delivery, `sigaction`, `sigreturn` trampoline)
- [ ] Port a minimal libc (newlib or musl)

## Third-party : 
### Code :
- [uACPI](https://github.com/uACPI/uACPI)

### Resources : 
- Fonts : 
    - Perfect DOS VGA 437 by Zeh Fernando (zeh@zehfernando.com) 
- Images : 
- Video : 
- Sound/Music : 
