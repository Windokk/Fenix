<div align="center">

<img src="github/fenix-logo.png" alt="Logo" width="200"/>

# Welcome to Fenix !
<a href="LICENSE"><img src="https://img.shields.io/static/v1?label=License&message=GNU GPL V3&logo=gnu&color=blue" href="https://www.gnu.org/licenses/gpl-3.0.html" width="200"/></a>
<br>
<br>
### A Unix-like Minimalistic Operating System (x84_64)
<br>
</div>

## Roadmap (Last Updated May 2026)

### Phase 1 — Foundation ✅
- [x] Multiboot2 boot via GRUB 2
- [x] 32-bit → 64-bit long mode transition
- [x] Higher-half kernel mapping (PML4 paging)
- [x] GDT + TSS (64-bit)
- [x] IDT + PIC (8259) interrupt handling
- [x] Physical frame allocator
- [x] Virtual memory (4KB pages) — kernel, userspace and framebuffer regions
- [x] Kernel heap
- [x] Serial driver (debug output)
- [x] VESA framebuffer (mode info + virtual address mapping)
- [x] PS/2 controller init
- [x] PS/2 keyboard driver (scancode --> ASCII, ring buffer, modifier keys)
- [x] PS/2 mouse driver (3-byte packets, cursor rendering)
- [x] Graphics primitives (pixel, line, rectangle, filled rectangle, circle, filled circle)
- [x] Bitmap font (NotoSans 24pt, 4bpp)

### Phase 2 — Visible Kernel 🔧 **(current)**
- [x] Text rendering (`gfx_draw_text` using bitmap font)
- [x] `printf` output redirected to framebuffer (alongside serial)
- [ ] RTC driver (read real-time clock)
- [ ] PIT driver (timer interrupts, basic tick counter)
- [ ] Basic on-screen debug console (scrollable text output)

### Phase 3 — Interactive Shell
- [ ] Keyboard input line editor (backspace, cursor)
- [ ] Simple command parser / REPL
- [ ] Basic built-in commands (`clear`, `help`, `mem`, `uptime`)
- [ ] Mouse integration in UI (clickable elements)

### Phase 4 — Multitasking
- [ ] Task control block (TCB) and kernel threads
- [ ] Context switch (register save/restore, TSS stack swap)
- [ ] Round-robin scheduler
- [ ] Kernel-side `sleep` / yield

### Phase 5 — Userspace
- [ ] Syscall interface (`syscall`/`sysret`, syscall table)
- [ ] User-mode task creation (ring 3 jump)
- [ ] User-mode memory isolation (per-process page tables)
- [ ] Basic C runtime for user programs

### Phase 6 — Storage & Filesystem
- [ ] ATA/AHCI disk driver (or virtio-blk for QEMU)
- [ ] FAT32 or ext2 filesystem (read-only first)
- [ ] VFS abstraction layer
- [ ] Writable filesystem support

### Phase 7 — POSIX & Beyond
- [ ] `fork` / `exec` / `wait`
- [ ] File descriptors (`open`, `read`, `write`, `close`)
- [ ] ELF binary loader
- [ ] Pipes and basic IPC
- [ ] Port a minimal libc (newlib or musl)
