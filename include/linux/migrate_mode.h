/* SPDX-License-Identifier: GPL-2.0 */
#ifndef MIGRATE_MODE_H_INCLUDED
#define MIGRATE_MODE_H_INCLUDED
/*
 * MIGRATE_ASYNC means never block
 * MIGRATE_SYNC_LIGHT in the current implementation means to allow blocking
 *	on most operations but not ->writepage as the potential stall time
 *	is too significant
 * MIGRATE_SYNC will block when migrating pages
 * MIGRATE_SYNC_NO_COPY will block when migrating pages but will not copy pages
 *	with the CPU. Instead, page copy happens outside the migratepage()
 *	callback and is likely using a DMA engine. See migrate_vma() and HMM
 *	(mm/hmm.c) for users of this mode.
 */
enum migrate_mode {
	///异步模式，分离页面时，发现大量分离页面时，不会停止，根据情况可能推出内存规整
	///详细参考too_many_isolated()
	MIGRATE_ASYNC,
	///同步模式，允许被阻塞，kcompactd线程采用该模式，分离页面时，发现大量分离页面时，可以睡眠
	MIGRATE_SYNC_LIGHT,
	///同步模式，允许被阻塞，手工设置/proc/sys/vm/compact_memory触发内存规整，采用该模式，
	///分离页面时，发现大量分离页面时，可以睡眠
	MIGRATE_SYNC,
	///类似于同步模式，但拷贝页面数据由DMA来完成,比如HMM,mm/hmm.c
	MIGRATE_SYNC_NO_COPY,
};

#endif		/* MIGRATE_MODE_H_INCLUDED */
