// SPDX-License-Identifier: GPL-2.0-only
/*
 * Based on arch/arm/kernel/setup.c
 *
 * Copyright (C) 1995-2001 Russell King
 * Copyright (C) 2012 ARM Ltd.
 */

#include <linux/acpi.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/initrd.h>
#include <linux/console.h>
#include <linux/cache.h>
#include <linux/screen_info.h>
#include <linux/init.h>
#include <linux/kexec.h>
#include <linux/root_dev.h>
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/fs.h>
#include <linux/panic_notifier.h>
#include <linux/proc_fs.h>
#include <linux/memblock.h>
#include <linux/of_fdt.h>
#include <linux/efi.h>
#include <linux/psci.h>
#include <linux/sched/task.h>
#include <linux/mm.h>

#include <asm/acpi.h>
#include <asm/fixmap.h>
#include <asm/cpu.h>
#include <asm/cputype.h>
#include <asm/daifflags.h>
#include <asm/elf.h>
#include <asm/cpufeature.h>
#include <asm/cpu_ops.h>
#include <asm/kasan.h>
#include <asm/numa.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/smp_plat.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/traps.h>
#include <asm/efi.h>
#include <asm/xen/hypervisor.h>
#include <asm/mmu_context.h>

static int num_standard_resources;
static struct resource *standard_resources;

phys_addr_t __fdt_pointer __initdata;

/*
 * Standard memory resources
 */
static struct resource mem_res[] = {
	{
		.name = "Kernel code",
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_SYSTEM_RAM
	},
	{
		.name = "Kernel data",
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_SYSTEM_RAM
	}
};

#define kernel_code mem_res[0]
#define kernel_data mem_res[1]

/*
 * The recorded values of x0 .. x3 upon kernel entry.
 */
u64 __cacheline_aligned boot_args[4];

void __init smp_setup_processor_id(void)
{
	u64 mpidr = read_cpuid_mpidr() & MPIDR_HWID_BITMASK;
	set_cpu_logical_map(0, mpidr);

	pr_info("Booting Linux on physical CPU 0x%010lx [0x%08x]\n",
		(unsigned long)mpidr, read_cpuid_id());
}

bool arch_match_cpu_phys_id(int cpu, u64 phys_id)
{
	return phys_id == cpu_logical_map(cpu);
}

struct mpidr_hash mpidr_hash;
/**
 * smp_build_mpidr_hash - Pre-compute shifts required at each affinity
 *			  level in order to build a linear index from an
 *			  MPIDR value. Resulting algorithm is a collision
 *			  free hash carried out through shifting and ORing
 */
static void __init smp_build_mpidr_hash(void)
{
	u32 i, affinity, fs[4], bits[4], ls;
	u64 mask = 0;
	/*
	 * Pre-scan the list of MPIDRS and filter out bits that do
	 * not contribute to affinity levels, ie they never toggle.
	 */
	for_each_possible_cpu(i)
		mask |= (cpu_logical_map(i) ^ cpu_logical_map(0));
	pr_debug("mask of set bits %#llx\n", mask);
	/*
	 * Find and stash the last and first bit set at all affinity levels to
	 * check how many bits are required to represent them.
	 */
	for (i = 0; i < 4; i++) {
		affinity = MPIDR_AFFINITY_LEVEL(mask, i);
		/*
		 * Find the MSB bit and LSB bits position
		 * to determine how many bits are required
		 * to express the affinity level.
		 */
		ls = fls(affinity);
		fs[i] = affinity ? ffs(affinity) - 1 : 0;
		bits[i] = ls - fs[i];
	}
	/*
	 * An index can be created from the MPIDR_EL1 by isolating the
	 * significant bits at each affinity level and by shifting
	 * them in order to compress the 32 bits values space to a
	 * compressed set of values. This is equivalent to hashing
	 * the MPIDR_EL1 through shifting and ORing. It is a collision free
	 * hash though not minimal since some levels might contain a number
	 * of CPUs that is not an exact power of 2 and their bit
	 * representation might contain holes, eg MPIDR_EL1[7:0] = {0x2, 0x80}.
	 */
	mpidr_hash.shift_aff[0] = MPIDR_LEVEL_SHIFT(0) + fs[0];
	mpidr_hash.shift_aff[1] = MPIDR_LEVEL_SHIFT(1) + fs[1] - bits[0];
	mpidr_hash.shift_aff[2] = MPIDR_LEVEL_SHIFT(2) + fs[2] -
						(bits[1] + bits[0]);
	mpidr_hash.shift_aff[3] = MPIDR_LEVEL_SHIFT(3) +
				  fs[3] - (bits[2] + bits[1] + bits[0]);
	mpidr_hash.mask = mask;
	mpidr_hash.bits = bits[3] + bits[2] + bits[1] + bits[0];
	pr_debug("MPIDR hash: aff0[%u] aff1[%u] aff2[%u] aff3[%u] mask[%#llx] bits[%u]\n",
		mpidr_hash.shift_aff[0],
		mpidr_hash.shift_aff[1],
		mpidr_hash.shift_aff[2],
		mpidr_hash.shift_aff[3],
		mpidr_hash.mask,
		mpidr_hash.bits);
	/*
	 * 4x is an arbitrary value used to warn on a hash table much bigger
	 * than expected on most systems.
	 */
	if (mpidr_hash_size() > 4 * num_possible_cpus())
		pr_warn("Large number of MPIDR hash buckets detected\n");
}

static void *early_fdt_ptr __initdata;

void __init *get_early_fdt_ptr(void)
{
	return early_fdt_ptr;
}

asmlinkage void __init early_fdt_map(u64 dt_phys)
{
	int fdt_size;

	early_fixmap_init();
	early_fdt_ptr = fixmap_remap_fdt(dt_phys, &fdt_size, PAGE_KERNEL);
}

static void __init setup_machine_fdt(phys_addr_t dt_phys)
{
	int size;
	///完成fdt的pte页表填写，返回fdt虚拟地址，这里虚拟地址事先定义预留
	///映射了2MB
	void *dt_virt = fixmap_remap_fdt(dt_phys, &size, PAGE_KERNEL);
	const char *name;

	if (dt_virt)
		///把dtb所占内存添加到memblock管理的reserve模块，后续内存分配不会使用这段内存
		//使用完后，会使用memblock_free()释放
		memblock_reserve(dt_phys, size);

	///扫描解析dtb，将内存布局信息填入memblock系统
	if (!dt_virt || !early_init_dt_scan(dt_virt)) {
		pr_crit("\n"
			"Error: invalid device tree blob at physical address %pa (virtual address 0x%p)\n"
			"The dtb must be 8-byte aligned and must not exceed 2 MB in size\n"
			"\nPlease check your bootloader.",
			&dt_phys, dt_virt);

		while (true)
			cpu_relax();
	}

	/* Early fixups are done, map the FDT as read-only now */
	fixmap_remap_fdt(dt_phys, &size, PAGE_KERNEL_RO);

	name = of_flat_dt_get_machine_name();
	if (!name)
		return;

	pr_info("Machine model: %s\n", name);
	dump_stack_set_arch_desc("%s (DT)", name);
}

static void __init request_standard_resources(void)
{
	struct memblock_region *region;
	struct resource *res;
	unsigned long i = 0;
	size_t res_size;

	//内核代码段的起始位置
	kernel_code.start   = __pa_symbol(_stext); 
	//内核代码段的结束位置
	kernel_code.end     = __pa_symbol(__init_begin - 1);
	//内核数据段的起始位置
	kernel_data.start   = __pa_symbol(_sdata);
	//内核数据段的结束位置
	kernel_data.end     = __pa_symbol(_end - 1);

	// memblock.memory的数量
	num_standard_resources = memblock.memory.cnt;
	res_size = num_standard_resources * sizeof(*standard_resources);
	// 在物理内存中分配空间
	standard_resources = memblock_alloc(res_size, SMP_CACHE_BYTES);
	if (!standard_resources)
		panic("%s: Failed to allocate %zu bytes\n", __func__, res_size);

	// 遍历memblock的每个内存区域
	for_each_mem_region(region) {
		res = &standard_resources[i++];
		// 判断是否为保留区域
		if (memblock_is_nomap(region)) {
			res->name  = "reserved";
			res->flags = IORESOURCE_MEM;
		} else {
			// 如果不是保留区域，标记为'System RAM'
			res->name  = "System RAM";
			res->flags = IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY;
		}
		// 将页帧编号转换为物理地址
		res->start = __pfn_to_phys(memblock_region_memory_base_pfn(region));
		res->end = __pfn_to_phys(memblock_region_memory_end_pfn(region)) - 1;

		// 将内存区域注册到iomem_resource
		request_resource(&iomem_resource, res);

		// 如果kernel_code和kernel_data的地址范围在当前的内存区域，将其作为子资源注册到对应的System RAM
		if (kernel_code.start >= res->start &&
		    kernel_code.end <= res->end)
			request_resource(res, &kernel_code);
		if (kernel_data.start >= res->start &&
		    kernel_data.end <= res->end)
			request_resource(res, &kernel_data);
#ifdef CONFIG_KEXEC_CORE
		/* Userspace will find "Crash kernel" region in /proc/iomem. */
		if (crashk_res.end && crashk_res.start >= res->start &&
		    crashk_res.end <= res->end)
			request_resource(res, &crashk_res);
#endif
	}
}

static int __init reserve_memblock_reserved_regions(void)
{
	u64 i, j;

	for (i = 0; i < num_standard_resources; ++i) {
		struct resource *mem = &standard_resources[i];
		phys_addr_t r_start, r_end, mem_size = resource_size(mem);

		if (!memblock_is_region_reserved(mem->start, mem_size))
			continue;

		for_each_reserved_mem_range(j, &r_start, &r_end) {
			resource_size_t start, end;

			start = max(PFN_PHYS(PFN_DOWN(r_start)), mem->start);
			end = min(PFN_PHYS(PFN_UP(r_end)) - 1, mem->end);

			if (start > mem->end || end < mem->start)
				continue;

			reserve_region_with_split(mem, start, end, "reserved");
		}
	}

	return 0;
}
arch_initcall(reserve_memblock_reserved_regions);

u64 __cpu_logical_map[NR_CPUS] = { [0 ... NR_CPUS-1] = INVALID_HWID };

u64 cpu_logical_map(unsigned int cpu)
{
	return __cpu_logical_map[cpu];
}

void __init __no_sanitize_address setup_arch(char **cmdline_p)
{
	///初始化，init_mm的地址，定义在arch/arm64/kernel/vmlinux.lds
	// 创建系统第一个mm结构体，成员函数.pgd指向swapper_pg_dir
	setup_initial_init_mm(_stext, _etext, _edata, _end);

	// 将cmdline_p指针指向boot_command_line，而boot_command_line将会在后面的setup_machine_fdt中解析设备树传入
	*cmdline_p = boot_command_line;

	/*
	 * If know now we are going to need KPTI then use non-global
	 * mappings from the start, avoiding the cost of rewriting
	 * everything later.
	 */
	arm64_use_ng_mappings = kaslr_requires_kpti();

	///注意这里都是创建的静态页表，因为内存子系统尚未建立(伙伴系统还没开始工作)
	early_fixmap_init();   ///fixmap区映射,只创建中间级转换页表,最后一级页表未创建
	early_ioremap_init();  ///早期ioremap映射,依赖fixmap转换表

	/*
	__fdt_pointer: fdt phy, head.S中的__primary_switched阶段会执行str_l	x21, __fdt_pointer, x5，将x21中的fdt地址存放到全局变量__fdt_pointer
	setup_machine_fdt: 校验fdt合法性，并通过下面3个回调函数扫描fdt中预留的memory及传入内核参数
	1. 扫描device tree函数：of_scan_flat_dt
	2. 3个回调函数：
	early_init_dt_scan_chosen： 解析chosen信息，其中一般包括initrd、bootargs，保留命令行参数到boot_command_line全局变量中
	early_init_dt_scan_root: 从fdt中初始化{size, address}结构信息，保存到dt_root_addr_cells,dt_root_size_cells中
	early_init_dt_scan_memory：扫描fdt中memory区域，寻找device_type="memory"的节点，并通过以下函数将该区域添加到memblock管理系统
		early_init_dt_add_memory_arch
		memblock_add：将region添加到memblock.memory中
	*/
	setup_machine_fdt(__fdt_pointer);///设备树映射,读取内存信息后，填入memblock系统，用于后面伙伴系统

	/*
	 * Initialise the static keys early as they may be enabled by the
	 * cpufeature code and early parameters.
	 */
	jump_label_init();
	/* 
	解析boot_command_line, 并设置done标志位
	*/
	parse_early_param();

	/*
	 * Unmask asynchronous aborts and fiq after bringing up possible
	 * earlycon. (Report possible System Errors once we can report this
	 * occurred).
	 */
	local_daif_restore(DAIF_PROCCTX_NOIRQ);

	/*
	 * TTBR0 is only used for the identity mapping at this stage. Make it
	 * point to zero page to avoid speculatively fetching new entries.
	 */
	cpu_uninstall_idmap();

	xen_early_init();
	/*
	 * 1. 使用回调函数fdt_find_uefi_params获取fdt中关于uefi的信息，所有要扫描的参数包含在dt_param这个全局数组中
	 *         例如：system table, memmap address, memmap size, memmap desc. size, memmap desc. version
	 * 2. 将解析出的内存区域加入memblock.reserve区域
	 * 3. 调用uefi_init(), reserve_regions()等函数进行uefi模式初始化
	 * 4. 初始化完成后，将前面reserve的区域unreserve
	 * 
	 */
	efi_init();

	if (!efi_enabled(EFI_BOOT) && ((u64)_text % MIN_KIMG_ALIGN) != 0)
	     pr_warn(FW_BUG "Kernel image misaligned at boot, please fix your bootloader!");

	///整理memblock的内存区域
	arm64_memblock_init();

	///至此，物理内存通过memblock模块添加入了系统，但此时只有dtb，Image所在的两段物理内存可以访问；
	//其他区域的物理内存，还没建立映射，可以通过memblock_alloc分配，但不能访问；
	//接下来通过paging_init建立不能访问的物理区域的页表;
	//
	//paging_init是内存初始化最核心的一步,将完成细粒度内核镜像映射(分别映射每个段),线性映射(内核可以访问整个物理内存)
	paging_init();   ///建立动态页表

	acpi_table_upgrade();

	/* Parse the ACPI tables for possible boot-time configuration */
	acpi_boot_table_init();

	if (acpi_disabled)
		/*
		 * 将dtb转换为device_node tree，根节点为全局变量 of_allnodes
		 * 后续内核会遍历tree来初始化描述的各个device
		 */
		unflatten_device_tree();

	///至此，内核已经可以访问所有物理内存
	///接下来开始初始化内存的关键数据结构，Linux当前默认采用sparse内存模型
	bootmem_init();

	kasan_init();
	/*
	 * 将memblock.memory挂载到iomem_resource资源树下, 资源树是一颗倒挂的树
	 * request_resource：将设备实体登记注册到总线空间链
	 * 在遍历memblock.memory过程中，会检查kernel_code,kernel_data是否属于某region，如果是则挂载到该region下
	 * 
	 */
	request_standard_resources();
	/*
	 * early_ioremap 功能到此就结束了
	 * 在buddy系统还未建立前，要进行寄存器访问基本职能使用early_ioremap 功能
	 */
	early_ioremap_reset();

	if (acpi_disabled)
		psci_dt_init();
	else
		psci_acpi_init();

	init_bootcpu_ops();
	smp_init_cpus();
	smp_build_mpidr_hash();

	/* Init percpu seeds for random tags after cpus are set up. */
	kasan_init_sw_tags();

#ifdef CONFIG_ARM64_SW_TTBR0_PAN
	/*
	 * Make sure init_thread_info.ttbr0 always generates translation
	 * faults in case uaccess_enable() is inadvertently called by the init
	 * thread.
	 */
	init_task.thread_info.ttbr0 = phys_to_ttbr(__pa_symbol(reserved_pg_dir));
#endif

	if (boot_args[1] || boot_args[2] || boot_args[3]) {
		pr_err("WARNING: x1-x3 nonzero in violation of boot protocol:\n"
			"\tx1: %016llx\n\tx2: %016llx\n\tx3: %016llx\n"
			"This indicates a broken bootloader or old kernel\n",
			boot_args[1], boot_args[2], boot_args[3]);
	}
}

static inline bool cpu_can_disable(unsigned int cpu)
{
#ifdef CONFIG_HOTPLUG_CPU
	const struct cpu_operations *ops = get_cpu_ops(cpu);

	if (ops && ops->cpu_can_disable)
		return ops->cpu_can_disable(cpu);
#endif
	return false;
}

static int __init topology_init(void)
{
	int i;

	for_each_online_node(i)
		register_one_node(i);

	for_each_possible_cpu(i) {
		struct cpu *cpu = &per_cpu(cpu_data.cpu, i);
		cpu->hotpluggable = cpu_can_disable(i);
		register_cpu(cpu, i);
	}

	return 0;
}
subsys_initcall(topology_init);

static void dump_kernel_offset(void)
{
	const unsigned long offset = kaslr_offset();

	if (IS_ENABLED(CONFIG_RANDOMIZE_BASE) && offset > 0) {
		pr_emerg("Kernel Offset: 0x%lx from 0x%lx\n",
			 offset, KIMAGE_VADDR);
		pr_emerg("PHYS_OFFSET: 0x%llx\n", PHYS_OFFSET);
	} else {
		pr_emerg("Kernel Offset: disabled\n");
	}
}

static int arm64_panic_block_dump(struct notifier_block *self,
				  unsigned long v, void *p)
{
	dump_kernel_offset();
	dump_cpu_features();
	dump_mem_limit();
	return 0;
}

static struct notifier_block arm64_panic_block = {
	.notifier_call = arm64_panic_block_dump
};

static int __init register_arm64_panic_block(void)
{
	atomic_notifier_chain_register(&panic_notifier_list,
				       &arm64_panic_block);
	return 0;
}
device_initcall(register_arm64_panic_block);
