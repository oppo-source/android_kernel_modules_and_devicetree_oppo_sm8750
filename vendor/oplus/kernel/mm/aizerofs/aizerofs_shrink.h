// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2024 Oplus. All rights reserved
 */
#ifndef ___AIZEROFS_SHRINK_H
#define ___AIZEROFS_SHRINK_H

#include <linux/mm_types.h>
#include <linux/types.h>
#include <linux/completion.h>

#define DBUF_CACHE_DISABLE_SHRINK	0x1
#define DBUF_CACHE_THREAD_IO_ERR	0x2

#define AIZEROFS_PATH_MAX 512
/*
 *
 * pages description:
 * find --->
 * 0				remained_pages-1      total_pages-1
 * |----------------------------|-----------------------|
 *				<--- shrink
 *
 * parameter description:
 * flags: DBUF_CACHE_DISABLE_SHRINK-> disable shrink temporarity.
 * list: add global dbuf_cache_list.
 * pos: read request position currently.
 * remained_pages: how many pages are left after being shrinked
 * total_pages: how many pages this dma-buf supposes to have
 * no io is required for this range).
 * lock: protect this structure.
 * pages: copy from sglist of dma-buf.
 * dbuf: associated dma-buf currently.
 * bin_path: associated file path.
 */
struct aizerofs_dma_buf_cache {
	unsigned long flags;
	struct list_head list;
	struct rcu_head rcu;
	u64 pos;
	unsigned long remained_pages;
	unsigned long total_pages;
	unsigned long allocated_pages;
	unsigned long read_pages;
	unsigned long nr_reclaimed;
	spinlock_t lock;
	struct work_struct io_worker;
	bool stop_io_worker;
	int is_destroying;
	struct page **pages;
	struct dma_buf *dbuf;
	struct fsnotify_mark *mark;
	char bin_path[AIZEROFS_PATH_MAX];
	struct inode *inode, *parent_inode;
};

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_AIZEROCOPY)
static inline bool aizerofs_enabled(void)
{
	DECLARE_STATIC_KEY_FALSE(aizerofs_enable);

	return static_branch_likely(&aizerofs_enable);
}
#else
static inline bool aizerofs_enabled(void)
{
	return false;
}
#endif

/* used by xxx_heap */
extern inline bool handle_dbuf_cache_release(struct dma_buf *dmabuf);
extern struct aizerofs_dma_buf_cache *find_or_create_dbuf_cache(unsigned long *len);
extern inline struct page *get_page_from_dbuf_cache(struct aizerofs_dma_buf_cache *dbuf_cache, unsigned long idx);
extern inline void dbuf_cache_add_pages(struct aizerofs_dma_buf_cache *dbuf_cache,
					struct page *page, unsigned long idx);
extern inline void dbuf_cache_init_dbuf(struct aizerofs_dma_buf_cache *dbuf_cache, struct dma_buf *dbuf);

extern void dbuf_cache_terminate_io_worker(struct aizerofs_dma_buf_cache *dbuf_cache);
extern void dmabuf_caches_destroy_all(void);

#endif /* ___AIZEROFS_SHRINK_H */
