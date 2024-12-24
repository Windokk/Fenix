## Intro

This is a personnal cheat sheet to remember all key concepts/elements of the OS

## Vocabulary

**GDT** : Global Descriptor Table : The Global Descriptor Table (GDT) is a binary data structure specific to the IA-32 and x86-64 architectures. It contains entries telling the CPU about memory segments.

**Interrupt** : Interrupts are signals from a device, such as a keyboard or a hard drive, to the CPU, telling it to immediately stop whatever it is currently doing and do something else. For example, a keyboard controller can send an interrupt when a character key was pressed. Then the OS can display the character on screen immediately, even if the CPU was doing something completely unrelated before, and return to what it was doing afterwards.

**ISR**: Interrupt Service Routine : The x86 architecture is an interrupt driven system. External events trigger an interrupt — the normal control flow is interrupted and an *Interrupt Service Routine* (ISR) is called.

**IRQ** : Interrupt Request : Hardware-originated Interrupts

**IDT** : Interrupt Descriptor Table : (Only in Protected and Long Mode) : The Interrupt Descriptor Table (IDT) is a binary data structure specific to the IA-32 and x86-64 architectures. It is the Protected Mode and Long Mode counterpart to the Real Mode Interrupt Vector Table (IVT) telling the CPU where the Interrupt Service Routines (ISR) are located (one per interrupt vector). It is similar to the Global Descriptor Table in structure. 

**PIC** : The 8259 Programmable Interrupt Controller (PIC) is one of the most important chips making up the x86 architecture. Without it, the x86 architecture would not be an interrupt driven architecture. The function of the 8259A is to manage hardware interrupts and send them to the appropriate system interrupt. This allows the system to respond to devices needs without loss of time (from polling the device, for instance). 

**x86/i386** : 

**x86_64** : 

**Real Mode (16 bit)** : Real Mode is a simplistic 16-bit mode that is present on all x86 processors. Real Mode was the first x86 mode design and was used by many early operating systems before the birth of Protected Mode. For compatibility purposes, all x86 processors begin execution in Real Mode. 

**Protected Mode (32 bit)** : Protected mode is the main operating mode of modern Intel processors (and clones) since the 80286 (16 bit). On 80386s and later, the 32 bit Protected Mode allows working with several virtual address spaces, each of which has a maximum of 4GB of addressable memory; and enables the system to enforce strict memory and hardware I/O protection as well as restricting the available instruction set via Rings.
A CPU that is initialized by the BIOS starts in Real Mode. Enabling Protected Mode unleashes the real power of your CPU. However, it will prevent you from using most of the BIOS interrupts, since these work in Real Mode (unless you have also written a V86 monitor).

**Long Mode (64 bit)** : Long mode extends general registers to 64 bits (RAX, RBX, RIP, RSP, RFLAGS, etc), and adds eight additional integer registers (R8, R9, ..., R15) plus eight more SSE registers (XMM8 to XMM15) to the CPU. Linear addresses are extended to 64 bit (however, a given CPU may implement less than this) and the physical address space is extended to 52 bits (a given CPU may implement less than this). In essence long mode adds another mode to the CPU.
Long mode does not support hardware task switching or virtual 8086 tasks. In long mode the current CS determines if the code currently running is 64 bit code (true long mode) or 32 bit code (compatibility mode), or even 16-bit protected mode code (still in compatibility mode). Using paging has become mandatory, and segmentation has been stripped down for performance reasons. 

**Syscalls** : 

**MMU** : 

**Paging** : 

**Heap** : 