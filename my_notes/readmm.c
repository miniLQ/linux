/*
 * The following fragment of code is executed with the MMU enabled.
 *
 *   x0 = __PHYS_OFFSET
 */
SYM_FUNC_START_LOCAL(__primary_switched)
	adrp	x4, init_thread_union      ///init_thread_union指向thread_union数据结构，其中包含系统第一个进程(init进程)的内核栈
	add	sp, x4, #THREAD_SIZE           ///sp指向栈顶
	adr_l	x5, init_task
	msr	sp_el0, x5			// Save thread_info  ///sp_el0在EL1模式下无效，这里用来存init进程的task_struct指针是合适的

	adr_l	x8, vectors			// load VBAR_EL1 with virtual  
	msr	vbar_el1, x8			// vector table address       ///填充异常向量表地址
	isb 													  ///确保以上指令执行完

	stp	xzr, x30, [sp, #-16]!
	mov	x29, sp                                   ///sp存放到x29  

#ifdef CONFIG_SHADOW_CALL_STACK
	adr_l	scs_sp, init_shadow_call_stack	// Set shadow call stack
#endif

	str_l	x21, __fdt_pointer, x5		// Save FDT pointer ///保存设备树的地址

	ldr_l	x4, kimage_vaddr		// Save the offset between
	sub	x4, x4, x0			// the kernel virtual and
	str_l	x4, kimage_voffset, x5		// physical mappings

	// Clear BSS
	///清除未初始化的数据段
	adr_l	x0, __bss_start
	mov	x1, xzr
	adr_l	x2, __bss_stop
	sub	x2, x2, x0
	bl	__pi_memset
	dsb	ishst				// Make zero page visible to PTW

#if defined(CONFIG_KASAN_GENERIC) || defined(CONFIG_KASAN_SW_TAGS)
	bl	kasan_early_init
#endif
	mov	x0, x21				// pass FDT address in x0
	bl	early_fdt_map			// Try mapping the FDT early
	bl	init_feature_override		// Parse cpu feature overrides
#ifdef CONFIG_RANDOMIZE_BASE
	tst	x23, ~(MIN_KIMG_ALIGN - 1)	// already running randomized?
	b.ne	0f
	bl	kaslr_early_init		// parse FDT for KASLR options
	cbz	x0, 0f				// KASLR disabled? just proceed
	orr	x23, x23, x0			// record KASLR offset
	ldp	x29, x30, [sp], #16		// we must enable KASLR, return
	ret					// to __primary_switch()
0:
#endif
	bl	switch_to_vhe			// Prefer VHE if possible
	add	sp, sp, #16
	mov	x29, #0
	mov	x30, #0              //sp指向内核栈顶
	b	start_kernel         //跳转到C语言入口
SYM_FUNC_END(__primary_switched)
//============================================================================================
/*
 * for read kernel source
 */
start_kernel(void)
.head.text        PROGBITS         ffff800010000000  00010000
.text             PROGBITS         ffff800010010000  00020000
.rodata           PROGBITS         ffff8000107b0000  007c0000
.init.text        PROGBITS         ffff8000109a0000  009b0000
 
add-symbol-file vmlinux 0x40010000 -s .head.text 0x40000000 -s .init.text 0x409a0000 -s .rodata 0x407b0000

add-symbol-file vmlinux 0x40081000 -s .head.text 0x40080000 -s .init.text 0x41bb0000 -s .rodata 0x418a0000



qemu-system-aarch64 -machine virt -cpu cortex-a57 -machine type=virt -m 1024 -smp 1 -kernel arch/arm64/boot/Image --append "rdinit=/linuxrc root=/dev/vda rw console=ttyAMA0 loglevel=8"  -nographic --fsdev local,id=kmod_dev,path=$PWD/k_shared,security_model=none -device virtio-9p-device,fsdev=kmod_dev,mount_tag=kmod_mount -S -s

/*
 * The following fragment of code is executed with the MMU enabled.
 *
 *   x0 = __PHYS_OFFSET
 */
SYM_FUNC_START_LOCAL(__primary_switched)
	adrp	x4, init_thread_union
	add	sp, x4, #THREAD_SIZE
	adr_l	x5, init_task
	msr	sp_el0, x5			// Save thread_info

	adr_l	x8, vectors			// load VBAR_EL1 with virtual
	msr	vbar_el1, x8			// vector table address
	isb

	stp	xzr, x30, [sp, #-16]!
	mov	x29, sp

#ifdef CONFIG_SHADOW_CALL_STACK
	adr_l	scs_sp, init_shadow_call_stack	// Set shadow call stack
#endif

	str_l	x21, __fdt_pointer, x5		// Save FDT pointer

	ldr_l	x4, kimage_vaddr		// Save the offset between
	sub	x4, x4, x0			// the kernel virtual and
	str_l	x4, kimage_voffset, x5		// physical mappings

	// Clear BSS
	adr_l	x0, __bss_start
	mov	x1, xzr
	adr_l	x2, __bss_stop
	sub	x2, x2, x0
	bl	__pi_memset
	dsb	ishst				// Make zero page visible to PTW

#if defined(CONFIG_KASAN_GENERIC) || defined(CONFIG_KASAN_SW_TAGS)
	bl	kasan_early_init
#endif
	mov	x0, x21				// pass FDT address in x0
	bl	early_fdt_map			// Try mapping the FDT early
	bl	init_feature_override		// Parse cpu feature overrides
#ifdef CONFIG_RANDOMIZE_BASE
	tst	x23, ~(MIN_KIMG_ALIGN - 1)	// already running randomized?
	b.ne	0f
	bl	kaslr_early_init		// parse FDT for KASLR options
	cbz	x0, 0f				// KASLR disabled? just proceed
	orr	x23, x23, x0			// record KASLR offset
	ldp	x29, x30, [sp], #16		// we must enable KASLR, return
	ret					// to __primary_switch()
0:
#endif
	bl	switch_to_vhe			// Prefer VHE if possible
	add	sp, sp, #16
	mov	x29, #0
	mov	x30, #0
	b	start_kernel
SYM_FUNC_END(__primary_switched)






























