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


伙伴系统：
迁移类型，解决碎片化问题
	MIGRATE_UNMOVABLE :在内存中位置固定，不能随意移动，比如内核分配的内存
	MIGRATE_MOVABLE：可以随意移动，用户态app分配的内存；
	MIGRATE_RECLAIMABLE： 不能移动，但可以回收的内存，比如文件映射缓存；
在伙伴系统的空闲链表中，每个链表都根据移动类型划分多个链表；

迁移类型的最小单位是pageblock, 2~order

set_pageblock_migratetype(struct page * page, int migratetype)
get_pageblock_migratetype(page)

两个块大小相同；
两个块地址连续；
两个块必须是同一个大块分离出来的；

页面分配器是基于zone设计的，
系统会优先使用ZONE_HIGHMEM, 然后才是ZONE_NORMAL;
从分配掩码中，知道从那个zone分配内存：
	for_each_zone_zonelist_nodemask(zone, z, zlist, highidx, nodemask)
从分配掩码中，分配器知道从哪个迁移类型中分配内存；
gfpflags_to_migratetype


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

