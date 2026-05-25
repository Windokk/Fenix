## Intro

This is a personnal cheat sheet to remember all key concepts/elements of the OS

## Vocabulary

**GDT** : Global Descriptor Table : The Global Descriptor Table (GDT) is a binary data structure specific to the IA-32 and x86-64 architectures. It contains entries telling the CPU about memory segments.

**Interrupt** : Interrupts are signals from a device, such as a keyboard or a hard drive, to the CPU, telling it to immediately stop whatever it is currently doing and do something else. For example, a keyboard controller can send an interrupt when a character key was pressed. Then the OS can display the character on screen immediately, even if the CPU was doing something completely unrelated before, and return to what it was doing afterwards.

**ISR**: Interrupt Service Routine : The x86 architecture is an interrupt driven system. External events trigger an interrupt — the normal control flow is interrupted and an *Interrupt Service Routine* (ISR) is called.

**IRQ** : Interrupt Request : Hardware-originated Interrupts

**IDT** : Interrupt Descriptor Table : (Only in Protected and Long Mode) : The Interrupt Descriptor Table (IDT) is a binary data structure specific to the IA-32 and x86-64 architectures. It is the Protected Mode and Long Mode counterpart to the Real Mode Interrupt Vector Table (IVT) telling the CPU where the Interrupt Service Routines (ISR) are located (one per interrupt vector). It is similar to the Global Descriptor Table in structure. 

**PIC** : The 8259 Programmable Interrupt Controller (PIC) is one of the most important chips making up the x86 architecture. Without it, the x86 architecture would not be an interrupt driven architecture. The function of the 8259A is to manage hardware interrupts and send them to the appropriate system interrupt. This allows the system to respond to devices needs without loss of time (from polling the device, for instance). 

**x86/i386** : The x86 (also called IA-32 or i386) architecture is a 32-bit instruction set architecture introduced by Intel with the 80386 processor. It introduced Protected Mode, 4 GB addressable memory, hardware paging, and privilege rings. Most early hobby OS kernels target this architecture before moving to x86_64.

**x86_64** : Also called AMD64 or Intel 64, x86_64 is the 64-bit extension of the x86 architecture. It extends general-purpose registers to 64 bits (RAX, RBX, …), adds 8 extra integer registers (R8–R15), extends the virtual address space to 48 bits (with a 64-bit canonical form), and makes paging mandatory. The kernel in this project runs in x86_64 Long Mode.

**Real Mode (16 bit)** : Real Mode is a simplistic 16-bit mode that is present on all x86 processors. Real Mode was the first x86 mode design and was used by many early operating systems before the birth of Protected Mode. For compatibility purposes, all x86 processors begin execution in Real Mode. 

**Protected Mode (32 bit)** : Protected mode is the main operating mode of modern Intel processors (and clones) since the 80286 (16 bit). On 80386s and later, the 32 bit Protected Mode allows working with several virtual address spaces, each of which has a maximum of 4GB of addressable memory; and enables the system to enforce strict memory and hardware I/O protection as well as restricting the available instruction set via Rings.
A CPU that is initialized by the BIOS starts in Real Mode. Enabling Protected Mode unleashes the real power of your CPU. However, it will prevent you from using most of the BIOS interrupts, since these work in Real Mode (unless you have also written a V86 monitor).

**Long Mode (64 bit)** : Long mode extends general registers to 64 bits (RAX, RBX, RIP, RSP, RFLAGS, etc), and adds eight additional integer registers (R8, R9, ..., R15) plus eight more SSE registers (XMM8 to XMM15) to the CPU. Linear addresses are extended to 64 bit (however, a given CPU may implement less than this) and the physical address space is extended to 52 bits (a given CPU may implement less than this). In essence long mode adds another mode to the CPU.
Long mode does not support hardware task switching or virtual 8086 tasks. In long mode the current CS determines if the code currently running is 64 bit code (true long mode) or 32 bit code (compatibility mode), or even 16-bit protected mode code (still in compatibility mode). Using paging has become mandatory, and segmentation has been stripped down for performance reasons. 

**Syscalls** : A syscall (system call) is the mechanism by which a user-space program requests a service from the kernel. On x86_64, the `syscall` instruction transfers control from ring 3 (user) to ring 0 (kernel) via the LSTAR MSR, which holds the kernel entry point address. The kernel inspects the syscall number (passed in RAX by convention) and dispatches to the appropriate handler before returning with `sysret`.

**MMU** : The Memory Management Unit is a hardware component (integrated into modern CPUs) responsible for translating virtual addresses produced by software into physical addresses that address actual RAM. It walks the page table hierarchy (PML4 → PDPT → PD → PT on x86_64) on every memory access and caches results in the TLB. If a translation is invalid or a permission check fails it fires a Page Fault exception (interrupt vector 14).

**Paging** : Paging is the memory management scheme used in Protected and Long Mode. Physical memory is divided into fixed-size units called **frames** (4 KB by default) and virtual memory into equally-sized **pages**. A multi-level page table maps each virtual page to a physical frame. On x86_64 there are 4 levels: PML4 → PDPT → PD → PT. Each entry stores the physical address of the next level (or the final frame) plus control bits (present, writable, user, NX, …). Fenix uses a higher-half kernel mapped at `0xFFFF800000000000`.

**Heap** : The heap is the region of memory used for dynamic allocation at runtime (`malloc`/`free`). In Fenix the heap starts right after the kernel's BSS section (`kernel_end`) and grows upward. It is managed with a linked list of `mblock_t` nodes. Each node records its size, a `0xDEAD` magic number for corruption detection, and a `free` flag. `malloc` walks the list looking for a free block that is large enough; `free` marks a block as available and coalesces adjacent free blocks.

---

## Memory Layout

**Higher-Half Kernel** : A design where the kernel is linked and mapped at a high virtual address (here `0xFFFF800000000000`) while user space occupies low addresses. This lets user and kernel share the same address space without the kernel being visible/accessible from user code. Physical addresses are converted with `TO_VMA_PTR`/`TO_PHYS_PTR` macros.

**Frame** : A frame is a fixed-size aligned block of *physical* memory (4 KB = `0x1000` bytes in Fenix). Frames are tracked with a **bitmap** (`mmu_frames`): one bit per frame, 1 = allocated, 0 = free. The bitmap is allocated just after `kernel_end` and covers all available RAM reported by the Multiboot2 memory map.

**Page Table Hierarchy (x86_64)** :
- **PML4** (Page Map Level 4) — 512 entries, bits [47:39] of the virtual address index into it.
- **PDPT** (Page Directory Pointer Table) — 512 entries, bits [38:30] index into it.
- **PD** (Page Directory) — 512 entries, bits [29:21] index into it.
- **PT** (Page Table) — 512 entries, bits [20:12] index into it; the entry holds the physical frame address and control bits.
- Bits [11:0] are the byte offset inside the 4 KB page.

**TLB** : The Translation Lookaside Buffer is a cache inside the CPU that stores recent virtual-to-physical address translations. Writing to CR3 (switching page tables) or executing `invlpg` flushes the relevant TLB entries. Stale TLB entries after modifying page tables cause hard-to-debug memory corruption.

**NX bit** : The No-Execute (NX / XD) bit is bit 63 of a page table entry. When set, the CPU refuses to fetch instructions from that page, even if code somehow jumps there. Used to mark data pages (stack, heap, data segment) non-executable as a security measure.

**VMA** : Virtual Memory Address. In Fenix, `KERNEL_START_MEMORY = 0xFFFF800000000000` is the kernel VMA base. Video memory is mapped at `KERNEL_VIDEO_MEMORY = 0xFFFFC00000000000`.

---

## Interrupts & Exceptions

**Exception** : A synchronous interrupt generated by the CPU itself in response to an instruction (e.g. divide by zero = vector 0, page fault = vector 14, general protection fault = vector 13). Some exceptions push an error code onto the stack; others do not.

**DPL** : Descriptor Privilege Level. A 2-bit field in GDT/IDT descriptors that encodes the minimum CPU privilege ring needed to access the descriptor. Ring 0 = kernel, Ring 3 = user. A user program cannot `int` into a gate with DPL 0 (would trigger a GPF).

**Rings** : The x86 architecture defines 4 privilege levels (rings 0–3). Ring 0 (kernel mode) has full hardware access. Ring 3 (user mode) is restricted — it cannot execute privileged instructions or access I/O ports directly. Rings 1 and 2 are rarely used by modern OSes.

**ISR Context (`isr_ctx_t`)** : When an interrupt fires, the CPU pushes RIP, CS, RFLAGS, RSP, SS automatically. Fenix's assembly stubs then push all general-purpose registers (RAX–R15) to build an `isr_ctx_t` struct on the stack, which is passed by pointer to the C handler.

---

## Drivers & Hardware

**VESA / Framebuffer** : VESA (Video Electronics Standards Association) framebuffer mode gives the kernel direct access to a linear array of pixels in memory. The Multiboot2 bootloader fills in a `multiboot_tag_framebuffer` with the physical address, resolution, pitch, and bits-per-pixel. Fenix maps this region at `KERNEL_VIDEO_MEMORY` and exposes it through `video_info_t`.

**PS/2** : The PS/2 controller (Intel 8042) manages keyboard (port 1) and mouse (port 2) communication. Commands are sent to port `0x64`, data to/from port `0x60`. Before using devices the controller must be initialized: disable ports → flush output buffer → read/write config → self-test → enable ports. Fenix implements this in `ps2_init()`.

**PIT** : The Programmable Interval Timer (Intel 8253/8254) generates periodic timer interrupts at a configurable frequency. It fires IRQ0 (vector 32 in Fenix after PIC remapping). Commonly used as the scheduler tick to preempt tasks.

**RTC** : The Real-Time Clock is a battery-backed hardware clock on the CMOS chip. It is read via I/O ports `0x70` (index) and `0x71` (data). It provides the current date and time and can fire IRQ8 for periodic tick events.

**Serial (UART)** : A Universal Asynchronous Receiver/Transmitter allows sending bytes over a serial line (COM port). Typically used for kernel debug output because it works before any graphical display is initialized. Common COM1 base port: `0x3F8`.

**I/O Ports** : On x86, peripheral devices are accessed either through memory-mapped I/O (MMIO) or port I/O using the `in`/`out` instructions. `outb(port, val)` writes a byte; `inb(port)` reads one. The PS/2 controller, PIC, PIT, and RTC all use port I/O.

---

## Boot

**Multiboot2** : A boot protocol specification that GRUB implements. A Multiboot2-compliant bootloader passes a pointer to an information structure (`multiboot_info_t`) in the EBX register when jumping to the kernel entry point. This structure contains tags describing available memory regions, the framebuffer, ACPI tables, boot command line, etc.

**GRUB** : GRand Unified Bootloader. Loads the kernel ELF binary from disk, sets up a minimal environment (enters Protected/Long Mode, sets up a temporary stack), then jumps to the kernel entry point.

**Linker Script (`linker.ld`)** : A file that tells the linker how to lay out the kernel binary in memory — which sections go where (`.text`, `.rodata`, `.data`, `.bss`), the load address vs. the virtual/link address (VMA vs LMA), and symbols like `kernel_end` that the kernel reads at runtime to know where its own image ends.

---

## Scheduling & Multitasking

**Task / Thread** : In Fenix a `task_t` represents a schedulable unit of execution. It stores the saved stack pointer (`rsp`), state, numeric ID, name, and a pointer to its heap-allocated stack. The scheduler uses cooperative/preemptive switching by saving and restoring register state.

**Context Switch** : The act of saving the CPU state (registers, stack pointer) of the currently running task and restoring the saved state of the next task. On x86_64 this is done in assembly: push all callee-saved registers, swap RSP, then pop the new task's registers.

**Scheduler** : The component that decides which task runs next. Fenix's scheduler cycles through tasks with state `TASK_READY`. A task can voluntarily yield with `task_yield()`, block itself with `task_block()`, or be unblocked externally with `task_unblock()`.

**`task_state_t`** : An enum of possible task states in Fenix: `TASK_READY` (can be scheduled), `TASK_RUNNING` (currently on CPU), `TASK_BLOCKED` (waiting for an event), `TASK_DEAD` (exited, resources can be reclaimed).