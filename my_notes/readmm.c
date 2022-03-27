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


内存三大数据结构,node,zone,page
1.pglist_data


contig_page_data

2.zone
3.page 
mem_map， 存放page,应
初始化在

alloc_node_mem_map

mem_map，与物理页面一一对

由page可以找到pfn
	__page_to_pfn(pg)

由pfn可以找到page
__pfn_to_page(pfn)

分配物理页面的核心接口函数：
<include/linux/gfp.h>

/********************************************************************
 * func:分配2的order次幂个连续的物理页面
 * gfp_mask: 分配掩码
 * order: oder应该小于MAX_ORDER(默认11)
 *
 * 返回值：第一个物理页面的page数据结构
 ********************************************************************/
struct page *alloc_pages(gfp_t gfp, unsigned int order)

<mm/page_alloc.c>
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)

分配一个一个物理页面
alloc_page(gfp_mask)
__get_free_page(gfp_mask)
返回全零页面
get_zeroed_page(gfp_t gfp_mask)
=========

释放页面
void __free_pages(struct *page, unsigned int order);

#define __free_page(page) __free_pages((page),0)
#define free_page(addr) free_pages((addr),0)
========================================================================================
/********************************************************************
 * func:分配2的order次幂个连续的物理页面
 * gfp_mask: 分配掩码
 * order: oder应该小于MAX_ORDER(默认11)
 *
 * 返回值：第一个物理页面的page数据结构
 ********************************************************************/
struct page *alloc_pages(gfp_t gfp, unsigned int order)
__alloc_pages_nodemask(






slab:
	kmem_cache_create(const char * name, unsigned int size, unsigned int align, slab_flags_t flags, void(* ctor)(void *))
		kmem_cache_destroy(struct kmem_cache * s)
		kmem_cache_alloc(struct kmem_cache * cachep, gfp_t flags)
		kmem_cache_free(struct kmem_cache * c, void * b)
kmem_cache


kmalloc函数
核心是slab机制，按照内存的2的order次方来创建多个slab描述符；

void *kmalloc(size_t size, gfp_t flags)
void kfree(const void *)




	alloc_pages
	
两个问题：
1.新创建一个slab描述符时，实际上没有分配物理页面；只有调用kmem_cache_alloc()时才真正从伙伴系统申请物理页面？
那刚开始申请缓冲对象都会比较慢，申请次数多了之后，反而会更快(有空闲对象，直接获得)，这样理解正确吗？

2.创建一个新的slab分配器时，返回的是首页page；而page本身是否也是用slab实现的？如果是，这里是不是有个嵌套，鸡生蛋，还是蛋生机鸡问题？

还有个疑问
问题3：slab描述符的成员，slab节点，struct kmem_cache_node *node[MAX_NUMNODES]; ///slab节点，在NUMA系统中，每个节点有一个kmem_cache_node数据结构

vmalloc:
适合分配较大内存；
可能睡眠，不能用在中断上下文；
--------------------------------------

| |||||
------------------------------------------


start_kernel(void)
setup_arch  
early_fixmap_init




early_ioremap(resource_size_t phys_addr, unsigned long size)
do_fork 

copy_process 

dup_mm

mm_init

mm_alloc_pgd   


handle_pte_fault(struct vm_fault * vmf)


context_switch
switch_mm_irqs_off
	switch_mm(struct mm_struct * prev, struct mm_struct * next, struct task_struct * tsk)
__switch_mm

check_and_switch_context
cpu_switch_mm
cpu_do_switch_mm

============================
kmalloc(size_t size, gfp_t)

vmalloc


alloc_zeroed_user_highpage_movable(struct vm_area_struct * vma, unsigned long vaddr)


vm_mmap_pgoff