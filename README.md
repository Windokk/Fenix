<div align="center">

<img src="github/fenix-logo.png" alt="Logo" width="200"/>

# Welcome to Fenix !
<br>
<img src="github/shield.svg" alt="shield"/>
<br>

### A Unix-like Minimalistic Operating System (x84_64)
<br>
</div>

## How to run : 

- Build a cross-compiler (x86_64_elf) : gcc, binutils
- Install xorriso, nasm, grub2 and qemu
- Execute run.sh

## Roadmap (Last Updated May 2026)

### Phase 1 - Foundation ✅
- [x] Multiboot2 boot via GRUB 2
- [x] 32-bit → 64-bit long mode transition
- [x] Higher-half kernel mapping (PML4 paging)
- [x] GDT + TSS (64-bit)
- [x] IDT + PIC (8259) interrupt handling
- [x] Physical frame allocator
- [x] Virtual memory (4KB pages) - kernel, userspace and framebuffer regions
- [x] Kernel heap
- [x] Serial driver (debug output)
- [x] VESA framebuffer (mode info + virtual address mapping)
- [x] PS/2 controller init
- [x] PS/2 keyboard driver (scancode --> ASCII, ring buffer, modifier keys)
- [x] PS/2 mouse driver (3-byte packets, cursor rendering)
- [x] Graphics primitives (pixel, line, rectangle, filled rectangle, circle, filled circle)
- [x] Bitmap font (NotoSans/Perfect_DOS_VGA_437, 16px, 4bpp)

### Phase 2 - Visible Kernel ✅
- [x] Text rendering (`gfx_draw_text` using bitmap font)
- [x] `printf` output redirected to framebuffer (alongside serial)
- [x] RTC driver (read real-time clock)
- [x] PIT driver (timer interrupts, basic tick counter)
- [x] Basic on-screen debug console (scrollable text output)

### Phase 3 - Interactive Shell
- [x] Keyboard input line editor (backspace, cursor, history)
- [x] Simple command parser / REPL
- [x] Basic built-in commands (`clear`, `help`, `mem`, `time`, `echo`, `shutdown`, `reboot`)
- [ ] Mouse integration in UI (clickable elements)

### Phase 4 - Multitasking ✅
- [x] Task control block (TCB) and kernel threads
- [x] Context switch (register save/restore, TSS stack swap)
- [x] Round-robin scheduler
- [x] Kernel-side `sleep` / yield

### Phase 5 - Userspace 🔧 **(current)**
- [ ] Syscall interface (`syscall`/`sysret`, syscall table)
- [ ] User-mode task creation (ring 3 jump)
- [ ] User-mode memory isolation (per-process page tables)
- [ ] Basic C runtime for user programs

### Phase 6 - Storage & Filesystem
- [ ] ATA/AHCI disk driver (or virtio-blk for QEMU)
- [ ] FAT32 or ext2 filesystem (read-only first)
- [ ] VFS abstraction layer
- [ ] Writable filesystem support

### Phase 7 - POSIX & Beyond
- [ ] `fork` / `exec` / `wait`
- [ ] File descriptors (`open`, `read`, `write`, `close`)
- [ ] ELF binary loader
- [ ] Pipes and basic IPC
- [ ] Port a minimal libc (newlib or musl)

## Third-party : 
### Code :
- [uACPI](https://github.com/uACPI/uACPI)

### Resources : 
- Fonts : 
    - Perfect DOS VGA 437 by Zeh Fernando (zeh@zehfernando.com)
    - Noto Sans by The Noto Project Authors (https://github.com/notofonts/latin-greek-cyrillic)
- Images : 
- Video : 
- Sound/Music : 
