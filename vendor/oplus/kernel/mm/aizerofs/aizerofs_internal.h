// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2025 Oplus. All rights reserved
 */
#ifndef AIZEROFS_INTERNAL_H
#define AIZEROFS_INTERNAL_H

enum aizerofs_kill_stat_item {
	/* hit kill/oom */
	AIZEROFS_ALLOC_HIT_KILL_WITH_AIO,
	AIZEROFS_ALLOC_HIT_KILL_WITHOUT_AIO,
	AIZEROFS_RELEASE_HIT_KILL_WITH_AIO,
	AIZEROFS_RELEASE_HIT_KILL_WITHOUT_AIO,

	NR_AIZEROFS_KILL_STAT_ITEMS,
};

struct aizerofs_stat {
	atomic64_t kill_stat[NR_AIZEROFS_KILL_STAT_ITEMS];
};

#define DESTROY_STAGE1	1
#define DESTROY_STAGE2	2

/* from mm/page_counter.c */
#if BITS_PER_LONG == 32
#define PAGE_COUNTER_MAX LONG_MAX
#else
#define PAGE_COUNTER_MAX (LONG_MAX / PAGE_SIZE)
#endif

#define FS_OPEN_WRITE 0x80000000

extern bool config_bug_on;

#define AIZEROFS_BUG_ON(condition) do {			\
	if (unlikely(config_bug_on && condition)) {	\
		BUG();									\
	} else if (condition) {						\
		pr_err("[AIZEROFS_WARN_ON]%s:%d tgid:%d pid:%d comm:%s\n",	\
			__func__, __LINE__, current->tgid, current->pid, current->comm);	\
		WARN_ON(1);								\
	}											\
} while (0)

extern struct aizerofs_stat aizerofs_perf_stat;
extern char *aizerofs_kill_stat_text[NR_AIZEROFS_KILL_STAT_ITEMS];

extern int dmabuf_cache_init(void);
extern inline struct aizerofs_dma_buf_cache *find_dmabuf_cache_by_path(char *path, bool allocate);
extern inline struct aizerofs_dma_buf_cache *find_dmabuf_cache_by_dmabuf(struct dma_buf *dbuf);

extern int dbuf_cache_drop_by_path(char *path);
extern int dbuf_cache_drop_all(void);

extern int aizerofs_handle_get_param_idx(unsigned long arg);
extern int aizerofs_handle_put_param_idx(int param_idx);

extern int aizerofs_handle_drop_caches(unsigned long arg);
extern unsigned long dmabuf_cache_shrink(gfp_t gfp_mask, unsigned long nr_to_scan,
					struct aizerofs_dma_buf_cache *target_cache);
extern void dmabuf_cache_destroy(struct aizerofs_dma_buf_cache *target_cache);

#endif

