extern long_mode_start

extern p4_table
extern p3_table
extern p2_table
extern p1_table

extern p3_table_low

extern p2_table_low_0
extern p2_table_low_1
extern p2_table_low_2
extern p2_table_low_3

extern kernel_stack_top
extern gdt64.kernel_code
extern g_booted_efi
extern efi_exit_boot_services_early

%define KERNEL_VMA          0xFFFF800000000000
%define PAGE_SIZE           0x1000
%define KERNEL_STACK_SIZE   PAGE_SIZE*2

section .efi64_entry
bits 64
DEFAULT ABS
global start_efi64
start_efi64:

    mov rsp, (kernel_stack_top - KERNEL_VMA)
    
    push rax
    push rbx

    mov byte [g_booted_efi - KERNEL_VMA], 1

    mov rax, cr3                      ; retrieve grub's cr3
    mov r13, rax                      ; store it for later (r13 = callee-saved, safe)

    call setup_page_tables

    mov rax, p3_table - KERNEL_VMA
    or rax, 0b11
    mov rbx, r13                      ; rbx =  phys adress of PML4 from GRUB
    
    ; firmware page tables are read-only with CR0.WP set
    mov rcx, cr0
    btr rcx, 16
    mov cr0, rcx
    bts rcx, 16

    mov [rbx + 256*8], rax            ; PML4[256] from GRUB -> p3_table (high)

    mov cr0, rcx ; rcx = original CR0 again, restored after the write

    ; (CR3 is NOT changed here: we're still using the GRUB tables, which have now been updated)

    pop rbx        ; mbi_phys
    pop rax        ; magic
    mov r12, rax
    mov rdi, rbx
    mov rax, efi_exit_boot_services_early - KERNEL_VMA
    call rax
    mov rax, r12

    mov rcx, p4_table - KERNEL_VMA
    mov cr3, rcx

    ; Move them to rdi and rsi respectively
    mov rdi, rax   ; Transfer original rax to rdi
    mov rsi, rbx   ; Transfer original rbx to rsi

    ; jump to refresh RIP and new GDT
    mov rax, long_mode_start
    jmp rax

setup_page_tables:
    ; PML4[0] -> p3_table_low (identity)
    mov rax, p3_table_low - KERNEL_VMA
    or rax, 0b11
    mov [p4_table - KERNEL_VMA], rax

    mov rax, p3_table - KERNEL_VMA ; (higher-half at 0xFFFF800000000000)
    or rax, 0b11
    mov [p4_table - KERNEL_VMA + 256 * 8], rax

    ; p3_table[0] -> p2_table
    mov rax, p2_table - KERNEL_VMA
    or rax, 0b11
    mov [p3_table - KERNEL_VMA], rax

    ; Map 4 x 2MB huge pages (0-8MB) in p2_table.
    ; PDE flags: Present(1) + Writable(2) + PageSize(0x80) = 0x83
    ; This covers the kernel binary AND wherever GRUB places the multiboot struct.
    mov rcx, 0
.map_huge:
    mov rax, 0x200000
    mul rcx
    or rax, 0x83
    mov [p2_table - KERNEL_VMA + rcx * 8], rax
    inc rcx
    cmp rcx, 4
    jne .map_huge

.map_identity:
    mov rax, p2_table_low_0 - KERNEL_VMA
    or rax, 0b11
    mov [p3_table_low - KERNEL_VMA + 0*8], rax

    mov rax, p2_table_low_1 - KERNEL_VMA
    or rax, 0b11
    mov [p3_table_low - KERNEL_VMA + 1*8], rax

    mov rax, p2_table_low_2 - KERNEL_VMA
    or rax, 0b11
    mov [p3_table_low - KERNEL_VMA + 2*8], rax

    mov rax, p2_table_low_3 - KERNEL_VMA
    or rax, 0b11
    mov [p3_table_low - KERNEL_VMA + 3*8], rax

    mov rcx, 0
.map_huge_low_0:
    mov rax, 0x200000
    mul rcx
    or rax, 0x83
    mov [p2_table_low_0 - KERNEL_VMA + rcx * 8], rax
    inc rcx
    cmp rcx, 512
    jne .map_huge_low_0

    mov rcx, 0

.map_huge_low_1:
    mov rax, 0x200000
    mul rcx
    add rax, 0x40000000
    or rax, 0x83
    mov [p2_table_low_1 - KERNEL_VMA + rcx * 8], rax
    inc rcx
    cmp rcx, 512
    jne .map_huge_low_1

    mov rcx, 0

.map_huge_low_2:
    mov rax, 0x200000
    mul rcx
    add rax, 0x80000000
    or rax, 0x83
    mov [p2_table_low_2 - KERNEL_VMA + rcx * 8], rax
    inc rcx
    cmp rcx, 512
    jne .map_huge_low_2

    mov rcx, 0

.map_huge_low_3:
    mov rax, 0x200000
    mul rcx
    add rax, 0xC0000000
    or rax, 0x83
    mov [p2_table_low_3 - KERNEL_VMA + rcx * 8], rax
    inc rcx
    cmp rcx, 512
    jne .map_huge_low_3

    ret