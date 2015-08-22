
.set .L_kEferLme, 0x100
.set .L_kEferNx, 0x800

.section .trampoline, "a"
.code16
.global trampoline
trampoline:
	cli

	# zero ds so we can access our code and data
	xor %ax, %ax
	mov %ax, %ds

	# inform bsp code that we're awake
	movl $1, trampolineStatus
	
	# wait until bsp code allows us to proceed
.L_spin:
	cmp $2, trampolineStatus
	jne .L_spin
	
	# now we can initialize the processor and jump into kernel code
	lgdt .L_gdtr
	
	# enable protection
	mov %cr0, %eax
	or $1, %eax
	mov %eax, %cr0

	jmpl $0x8, $.L_protected

.L_protected:
.code32
	mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %es
	xor %ax, %ax
	mov %ax, %fs
	mov %ax, %gs

	# enable PAE paging
	mov %cr4, %eax
	or $0x20, %eax
	mov %eax, %cr4
	
	# enable long mode (not active until we enable paging)
	mov $0xC0000080, %ecx
	rdmsr
	or $(.L_kEferLme | .L_kEferNx), %eax
	wrmsr
	
	# setup the pml4
	mov trampolinePml4, %eax
	mov %eax, %cr3
	
	# enable paging
	mov %cr0, %eax
	or $0x80000000, %eax
	mov %eax, %cr0
	
	jmpl $0x18, $.L_longmode

.L_longmode:
.code64
	xor %ax, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	mov %ax, %fs
	mov %ax, %gs
	
	mov trampolineStack, %rsp
	movabs $thorRtSecondaryEntry, %rax
	call *%rax
	ud2

.align 16
.L_gdt_start:
	.int 0
	.int 0
	.int 0x0000FFFF
	.int 0x00CF9800
	.int 0x0000FFFF
	.int 0x00CF9200
	.int 0
	.int 0x00A09800
.L_gdt_end:

.L_gdtr:
	.short .L_gdt_end - .L_gdt_start
	.long .L_gdt_start

.align 16
.global trampolineStatus
trampolineStatus:
	.int 0
.global trampolinePml4
trampolinePml4:
	.int 0
.global trampolineStack
trampolineStack:
	.quad 0
