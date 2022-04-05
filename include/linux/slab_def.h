/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SLAB_DEF_H
#define	_LINUX_SLAB_DEF_H

#include <linux/kfence.h>
#include <linux/reciprocal_div.h>

/*
 * Definitions unique to the original Linux SLAB allocator.
 */
///每个slab描述符，都有一个struct kmem_cache数据结构来抽象描述
struct kmem_cache {
	struct array_cache __percpu *cpu_cache;  ///本地缓存池，每个CPU一个

/* 1) Cache tunables. Protected by slab_mutex */
	///当本地的对象缓存池为空时，需要从共享缓存池或者slans_partial/slabs_free中，
	///获取batchcount个对象到本地缓存池
	unsigned int batchcount; 	

	///当本地缓存池空闲数目大于limit时，需要释放一些对象，就是batchcount个，便于内核回收或销毁
	unsigned int limit; 
	
	unsigned int shared; ///用于多核系统

	unsigned int size; ///对象的长度，align对齐
	struct reciprocal_value reciprocal_buffer_size;
/* 2) touched by every alloc & free from the backend */
	///对象分配掩码
	slab_flags_t flags;		/* constant flags */      

	///一个slab最多可以有多少个对象
	unsigned int num;		/* # of objs per slab */  

/* 3) cache_grow/shrink */
	/* order of pgs per slab (2^n) */
	///一个slab占用多个2的order次方物理页面
	unsigned int gfporder;							

	/* force GFP flags, e.g. GFP_DMA */
	gfp_t allocflags;
	
	///一个slab分配器有多少个不同的高速缓存行，用于着色
	size_t colour;			/* cache colouring range */ 
	
	///一个着色区长度，和L1高速缓存行大小相同
	unsigned int colour_off;	/* colour offset */     

	///用于OFF_SLAB模式的slab分配器，使用额外的内存来保存slab管理区域
	struct kmem_cache *freelist_cache; 

	///每个对象要占用1字节来存放freelists
	unsigned int freelist_size;

	/* constructor func */
	void (*ctor)(void *obj);

/* 4) cache creation/removal */
	///slab描述符名字
	const char *name;      
	///链表节点，用于把slab描述符添加到全局链表slab_caches中
	struct list_head list;  
	///本描述符引用计数
	int refcount;           
	
	///对象实际大小
	int object_size;   
	 ///对齐长度
	int align;             

/* 5) statistics */
#ifdef CONFIG_DEBUG_SLAB
	unsigned long num_active;
	unsigned long num_allocations;
	unsigned long high_mark;
	unsigned long grown;
	unsigned long reaped;
	unsigned long errors;
	unsigned long max_freeable;
	unsigned long node_allocs;
	unsigned long node_frees;
	unsigned long node_overflow;
	atomic_t allochit;
	atomic_t allocmiss;
	atomic_t freehit;
	atomic_t freemiss;

	/*
	 * If debugging is enabled, then the allocator can add additional
	 * fields and/or padding to every object. 'size' contains the total
	 * object size including these internal fields, while 'obj_offset'
	 * and 'object_size' contain the offset to the user object and its
	 * size.
	 */
	int obj_offset;
#endif /* CONFIG_DEBUG_SLAB */

#ifdef CONFIG_KASAN
	struct kasan_cache kasan_info;
#endif

#ifdef CONFIG_SLAB_FREELIST_RANDOM
	unsigned int *random_seq;
#endif

	unsigned int useroffset;	/* Usercopy region offset */
	unsigned int usersize;		/* Usercopy region size */

	///slab节点，在NUMA系统中，每个节点有一个kmem_cache_node数据结构
	struct kmem_cache_node *node[MAX_NUMNODES]; 
};

static inline void *nearest_obj(struct kmem_cache *cache, struct page *page,
				void *x)
{
	void *object = x - (x - page->s_mem) % cache->size;
	void *last_object = page->s_mem + (cache->num - 1) * cache->size;

	if (unlikely(object > last_object))
		return last_object;
	else
		return object;
}

/*
 * We want to avoid an expensive divide : (offset / cache->size)
 *   Using the fact that size is a constant for a particular cache,
 *   we can replace (offset / cache->size) by
 *   reciprocal_divide(offset, cache->reciprocal_buffer_size)
 */
static inline unsigned int obj_to_index(const struct kmem_cache *cache,
					const struct page *page, void *obj)
{
	u32 offset = (obj - page->s_mem);
	return reciprocal_divide(offset, cache->reciprocal_buffer_size);
}

static inline int objs_per_slab_page(const struct kmem_cache *cache,
				     const struct page *page)
{
	if (is_kfence_address(page_address(page)))
		return 1;
	return cache->num;
}

#endif	/* _LINUX_SLAB_DEF_H */
