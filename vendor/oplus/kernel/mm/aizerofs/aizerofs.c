// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2025 Oplus. All rights reserved
 */
#include "aizerofs_shrink.h"
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/bvec.h>
#include <linux/kthread.h>
#include <linux/ctype.h>
#include <linux/kprobes.h>
#include <trace/hooks/fsnotify.h>
#include "aizerofs_internal.h"

static bool enabled __read_mostly = false;

struct aizerofs_drop_cache_data {
	char cmd;
	u64 reap_size;
	char bin_path[AIZEROFS_PATH_MAX];
};

#define AIZEROFS_IOCTL_BIN_TO_DMA_BUF 0
#define AIZEROFS_IOCTL_GET_PARAM_IDX 1
#define AIZEROFS_IOCTL_DROP_CACHES _IOWR('a', 2, struct aizerofs_drop_cache_data)

struct aizerofs_to_dma_buf {
	int dma_buf_fd;
	int pad;
	u64 file_offs;
	u64 dma_buf_offs;
	u64 size;
	char bin_path[AIZEROFS_PATH_MAX];
};

#define AIIO_THREADS 4
#define BATCH_IO 128
#define BATCH_SIZE (BATCH_IO * PAGE_SIZE)

struct aizerofs_io {
	struct file *filp;
	loff_t pos;
	unsigned long len;
	unsigned long comp_len;
	struct bio_vec bvec[BATCH_IO];
};

typedef struct file *(*filp_open_t)(const char *filename, int flags, umode_t mode);
filp_open_t filp_open_dup = NULL;

static struct page **io_pages;
static struct task_struct *tasks[AIIO_THREADS];
static struct aizerofs_io io[AIIO_THREADS + 1];
static DEFINE_MUTEX(aizerofs_mutex);

struct aizerofs_stat aizerofs_perf_stat;
char *aizerofs_kill_stat_text[NR_AIZEROFS_KILL_STAT_ITEMS] = {
	"alloc_hit_kill_with_aio",
	"alloc_hit_kill_without_aio",
	"release_hit_kill_with_aio",
	"release_hit_kill_without_aio",
};

static int aizerofs_do_batch_io(struct aizerofs_io *io)
{
	for (unsigned long len = 0; len < io->len; len += BATCH_SIZE) {
		struct bio_vec *bvec = io->bvec;
		struct iov_iter iter;
		struct kiocb kiocb;
		int ret;

retry:
		for (int i = 0; i < BATCH_IO; i++) {
			bvec[i].bv_page = io_pages[(io->pos + len)/ PAGE_SIZE + i];
			bvec[i].bv_offset = 0;
			bvec[i].bv_len = PAGE_SIZE;
		}

		init_sync_kiocb(&kiocb, io->filp);
		kiocb.ki_flags |= IOCB_NOWAIT;
		kiocb.ki_pos = io->pos + len;
		iov_iter_bvec(&iter, ITER_DEST, bvec, BATCH_IO, BATCH_SIZE);
		ret = io->filp->f_op->read_iter(&kiocb, &iter);
		io->comp_len += BATCH_SIZE;
		if (ret < 0 && ret != -EAGAIN) {
			pr_err("%s-%s io error at %llx ret:%d\n",
				__func__, current->comm, io->pos, ret);
			return ret;
		}
		if (ret == -EAGAIN) {
			schedule_timeout_uninterruptible(1);
			pr_err_ratelimited("%s IO retry @%llx\n", __func__, io->pos);
			goto retry;
		}
	}

	return 0;
}

static int aizerofs_io_task(void *data)
{
	struct aizerofs_io *io = (struct aizerofs_io *)data;

	return aizerofs_do_batch_io(io);
}

static int aizerofs_open(struct inode *inode, struct file *file)
{
	if (!aizerofs_enabled()) {
		pr_err("ERROR!!! call %s when aizerofs is disable!\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static long aizerofs_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct aizerofs_to_dma_buf buf;
	struct dma_buf *dbuf;
	loff_t pos, size;
	struct file *bin_file;
	int ret = 0;

	struct aizerofs_dma_buf_cache *dbuf_cache;
	unsigned long off;

	if (!aizerofs_enabled()) {
		pr_err("ERROR!!! call %s when aizerofs is disable!\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
		case AIZEROFS_IOCTL_BIN_TO_DMA_BUF:
			break;
		case AIZEROFS_IOCTL_GET_PARAM_IDX:
			return aizerofs_handle_get_param_idx(arg);
		case AIZEROFS_IOCTL_DROP_CACHES:
			return aizerofs_handle_drop_caches(arg);
		default:
			return -EINVAL;
	}

	ret = copy_from_user(&buf, (void __user *)arg, sizeof(buf));
	if (ret) {
		pr_err("%s fail to get arg\n", __func__);
		return -EFAULT;
	}

	if (unlikely(strlen(buf.bin_path) >= AIZEROFS_PATH_MAX)) {
		pr_err("aizerofs: WARNING: func:%s path:%s len:%ld >= %d \n",
			__func__, buf.bin_path, strlen(buf.bin_path),
			(unsigned int)AIZEROFS_PATH_MAX);
		return -EINVAL;
	}

	/* reset ret=0 */
	ret = 0;

	pos = buf.file_offs;
	buf.bin_path[AIZEROFS_PATH_MAX - 1] = 0;

	bin_file = filp_open_dup(buf.bin_path, O_RDONLY | O_DIRECT, 0);
	if (IS_ERR(bin_file)) {
		pr_err("failed to open %s: %ld\n", buf.bin_path, PTR_ERR(bin_file));
		return PTR_ERR(bin_file);
	}

	size = i_size_read(file_inode(bin_file));
	if (size > SZ_2G || pos >= size || buf.dma_buf_offs) {
		filp_close(bin_file, NULL);
		return -EINVAL;
	}
	buf.size = min_t(unsigned long, buf.size, size - pos);

	dbuf = dma_buf_get(buf.dma_buf_fd);
	if (IS_ERR(dbuf)) {
		pr_err("%s fail to dma_buf_get", __func__);
		filp_close(bin_file, NULL);
		return PTR_ERR(dbuf);
	}

	mutex_lock(&aizerofs_mutex);

	dbuf_cache = find_dmabuf_cache_by_path(buf.bin_path, false);
	if (likely(dbuf_cache)) {
		spin_lock(&dbuf_cache->lock);
		/* the data of dbuf_cache is valid, eg: second read */
		off = dbuf_cache->read_pages << PAGE_SHIFT;

		if (off < buf.size) {
			pos = max_t(unsigned long, pos, off);
			buf.size = min_t(unsigned long, buf.size, buf.size - off);
		} else {
			pos = buf.size - 1;
			buf.size = 0;
		}

		if (!(dbuf_cache->flags & DBUF_CACHE_DISABLE_SHRINK)) {
			pr_err("ERROR!!! %s calling pass-through on %s whose dma-buf has been closed\n",
				__func__, buf.bin_path);
			spin_unlock(&dbuf_cache->lock);
			mutex_unlock(&aizerofs_mutex);
			dma_buf_put(dbuf);
			filp_close(bin_file, NULL);
			return -EINVAL;
		}

		if (!buf.size) {
			dbuf_cache->stop_io_worker = 1;
			spin_unlock(&dbuf_cache->lock);
			flush_work(&dbuf_cache->io_worker);
			mutex_unlock(&aizerofs_mutex);
			pr_info("%s bypass sync read: dbuf_cache:%lx remained_pages:%ld read_pages:%ld total_pages:%ld\n",
				__func__, (unsigned long)dbuf_cache, dbuf_cache->remained_pages, dbuf_cache->read_pages,
				dbuf_cache->total_pages);
			dma_buf_put(dbuf);
			filp_close(bin_file, NULL);

			return 0;
		}

		io_pages = dbuf_cache->pages;
		dbuf_cache->stop_io_worker = 1;
		spin_unlock(&dbuf_cache->lock);
		flush_work(&dbuf_cache->io_worker);
	} else {
		pr_err("ERROR!!! %s calling pass-through on %s without dbuf_cache\n", __func__, buf.bin_path);
		mutex_unlock(&aizerofs_mutex);
		dma_buf_put(dbuf);
		filp_close(bin_file, NULL);
		return -EINVAL;
	}

	pr_info("%s start sync read: dbuf_cache:%lx remained_pages:%ld read_pages:%ld total_pages:%ld\n",
		__func__, (unsigned long)dbuf_cache, dbuf_cache->remained_pages, dbuf_cache->read_pages,
		dbuf_cache->total_pages);

	do {
		unsigned long remained_len = buf.size % (AIIO_THREADS * BATCH_SIZE);
		unsigned long len = buf.size - remained_len;
		int i = 0;

		if (len > 0) {
			for (i = 0; i < AIIO_THREADS; i++) {
				io[i].filp = bin_file;
				io[i].len = len / AIIO_THREADS;
				io[i].comp_len = 0;
				io[i].pos = pos + i * len / AIIO_THREADS;
				tasks[i] = kthread_create(aizerofs_io_task, &io[i], "aizerofs_io/%d", i);
				if (IS_ERR(tasks[i])) {
					pr_err("%s failed to create io thread%d\n", __func__, i);
					break;
				}
			}
			if (i != AIIO_THREADS) {
				for (int j = 0; j < i; j++) {
					kthread_stop(tasks[j]);
					tasks[j] = NULL;
				}
				spin_lock(&dbuf_cache->lock);
				dbuf_cache->flags |= DBUF_CACHE_THREAD_IO_ERR;
				spin_unlock(&dbuf_cache->lock);

				ret = PTR_ERR(tasks[i]);
				tasks[i] = NULL;
				goto out;
			}
			for (i = 0; i < AIIO_THREADS; i++) {
				get_task_struct(tasks[i]);
				wake_up_process(tasks[i]);
			}

			pos += len;
		}

		i = AIIO_THREADS;
		len = remained_len - (remained_len % BATCH_SIZE);
		if (len > 0) {
			io[i].filp = bin_file;
			io[i].len = len;
			io[i].comp_len = 0;
			io[i].pos = pos;
			ret = aizerofs_do_batch_io(&io[i]);
			if (ret < 0) {
				spin_lock(&dbuf_cache->lock);
				dbuf_cache->flags |= DBUF_CACHE_THREAD_IO_ERR;
				spin_unlock(&dbuf_cache->lock);
				pr_err("%s-%s io error at %llx ret:%d\n",
						__func__, current->comm, io->pos, ret);
				goto out;
			}
			pos += len;
		}

		remained_len %= BATCH_SIZE;
		if (remained_len > 0) {
			int npages = DIV_ROUND_UP(remained_len, PAGE_SIZE);
			struct bio_vec *bvec = io[i].bvec;
			struct iov_iter iter;
			struct kiocb kiocb;

io_retry:
			for (i = 0; i < npages; i++) {
				bvec[i].bv_page = io_pages[pos / PAGE_SIZE + i];
				bvec[i].bv_offset = 0;
				bvec[i].bv_len = PAGE_SIZE;
			}
			init_sync_kiocb(&kiocb, bin_file);
			kiocb.ki_pos = pos;
			kiocb.ki_flags |= IOCB_NOWAIT;
			iov_iter_bvec(&iter, ITER_DEST, bvec, npages, npages * PAGE_SIZE);
			ret = bin_file->f_op->read_iter(&kiocb, &iter);
			if (ret < 0 && ret != -EAGAIN) {
				spin_lock(&dbuf_cache->lock);
				dbuf_cache->flags |= DBUF_CACHE_THREAD_IO_ERR;
				spin_unlock(&dbuf_cache->lock);

				pr_err("%s-%s io error at %llx ret:%d\n",
						__func__, current->comm, io->pos, ret);
				goto out;
			}
			if (ret == -EAGAIN) {
				schedule_timeout_uninterruptible(1);
				pr_err_ratelimited("%s IO retry @%llx\n", __func__, io->pos);
				goto io_retry;
			}
		}
	} while (0);

out:
	for (int i = 0; i < AIIO_THREADS && tasks[i]; i++) {
		int task_ret;
		int cnt = 0;

		/* make sure io threads have started */
		while (!READ_ONCE(io[i].comp_len) && cnt++ < 500)
			schedule_timeout_uninterruptible(1);
		AIZEROFS_BUG_ON(cnt >= 500);

		task_ret = kthread_stop(tasks[i]);
		put_task_struct(tasks[i]);
		tasks[i] = NULL;
		if (task_ret) {
			if (dbuf_cache) {
				spin_lock(&dbuf_cache->lock);
				dbuf_cache->flags |= DBUF_CACHE_THREAD_IO_ERR;
				spin_unlock(&dbuf_cache->lock);
			}
			ret = task_ret;
			pr_err("%s thread%i io failed, ret:%d\n", __func__, i, ret);
		}
	}
	mutex_unlock(&aizerofs_mutex);

	pr_info("%s end sync read: dbuf_cache:%lx remained_pages:%ld read_pages:%ld total_pages:%ld ret:%d\n",
		__func__, (unsigned long)dbuf_cache, dbuf_cache->remained_pages, dbuf_cache->read_pages,
		dbuf_cache->total_pages, ret);
	dma_buf_put(dbuf);
	filp_close(bin_file, NULL);

	return ret >= 0 ? 0 : ret;
}

static const struct file_operations aizerofs_fops = {
	.owner = THIS_MODULE,
	.open = aizerofs_open,
	.unlocked_ioctl = aizerofs_ioctl,
};

static struct miscdevice aizerofs_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "aizerofs",
	.fops = &aizerofs_fops,
};

/*************************** drop dbuf cache ***************************/
#define AIZEROFS_TRIM_CACHE		'1'
#define AIZEROFS_TRIM_CLEAN		'2'

static inline int run_trim(char cmd, char *path, unsigned long nr_pages)
{
	struct aizerofs_dma_buf_cache *target_cache;

	if (!path[0]){
		target_cache = NULL;
	} else {
		target_cache = find_dmabuf_cache_by_path(path, false);
		if (!target_cache)
			return -ENOENT;
	}

	switch (cmd) {
	case AIZEROFS_TRIM_CACHE:
		/* only drop dbuf cache */
		dmabuf_cache_shrink(0, nr_pages, target_cache);
		break;
	case AIZEROFS_TRIM_CLEAN:
		if (path[0])
			return dbuf_cache_drop_by_path(path);
		return dbuf_cache_drop_all();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int aizerofs_handle_drop_caches(unsigned long arg)
{
	struct aizerofs_drop_cache_data *data;
	unsigned int in_size;
	int ret = 0;
	unsigned long reap_pages;

	in_size = sizeof(struct aizerofs_drop_cache_data);
	data = kmalloc(in_size, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (copy_from_user(data, (void __user *)arg, in_size) != 0) {
		ret = -EFAULT;
		goto out_free_data;
	}

	if (unlikely(strlen(data->bin_path) >= AIZEROFS_PATH_MAX)) {
		pr_err("aizerofs: WARNING: func:%s path:%s len:%ld >= %d \n",
			__func__, data->bin_path, strlen(data->bin_path),
			(unsigned int)AIZEROFS_PATH_MAX);
			kfree(data);
			return -EINVAL;
	}

	if (!data->reap_size) {
		ret = -EINVAL;
		goto out_free_data;
	}

	reap_pages = min(data->reap_size / PAGE_SIZE, (u64)PAGE_COUNTER_MAX);
	/* nothing to do! */
	if (!reap_pages) {
		ret = 0;
		goto out_free_data;
	}

	return run_trim(data->cmd, data->bin_path, reap_pages);

out_free_data:
	kfree(data);

	return ret;
}

#ifdef CONFIG_AIZEROFS_DEBUG_SYS_ABI
#define AIZEROFS_ATTR(_name) \
	static struct kobj_attribute _name##_attr = __ATTR_RW(_name)

#define AIZEROFS_ATTR_WO(_name) \
	static struct kobj_attribute _name##_attr = __ATTR_WO(_name)

static int page_counter_memparse(const char *buf, const char *max,
		unsigned long *nr_pages)
{
	char *end;
	u64 bytes;

	if (!strcmp(buf, max)) {
		*nr_pages = PAGE_COUNTER_MAX;
		return 0;
	}

	bytes = memparse(buf, &end);
	if (*end != '\0')
		return -EINVAL;

	*nr_pages = min(bytes / PAGE_SIZE, (u64)PAGE_COUNTER_MAX);

	return 0;
}

static int run_cmd(char cmd, char *path, char *reap_size)
{
	int err;
	unsigned long nr_to_scan;

	reap_size = strstrip(reap_size);
	err = page_counter_memparse(reap_size, "", &nr_to_scan);
	if (err)
		return err;

	/* nothing to do! */
	if (!nr_to_scan)
		return 0;

	pr_info("@aizerofs: dropping dbuf %s cmd:%c path:%s reap_size:%s nr_to_scan:%ld @\n",
		__func__, cmd, path, reap_size, nr_to_scan);

	return run_trim(cmd, path, nr_to_scan);
}

/*
 *
 * format:
 * 1/2 [path] [reap_size], 1/2 [path] [reap_size],...
 *
 * 1 -> only drop dbuf cache (AIZEROFS_TRIM_CACHE)
 * 2 -> drop dbuf cache and metadata (AIZEROFS_TRIM_CLEAN)
 *
 * eg: echo "1 /mnt/a.dat" > /sys/kernel/aizerofs/drop_caches
 *     echo "1 /mnt/a.dat,2 /mnt/b.dat 200M" > /sys/kernel/aizerofs/drop_caches
 */
static ssize_t drop_caches_store(struct kobject *kobj,
			    struct kobj_attribute *attr,
			    const char *src, size_t len)
{
	int err = -EINVAL;
	char *cur;
	char *next;
	void *buf;
	char *path;
	size_t path_len;
	char reap_size[32] = {0};

	if (len < 2)
		return -EINVAL;

	buf = kvzalloc(len + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	path_len = len < AIZEROFS_PATH_MAX ? len : AIZEROFS_PATH_MAX;
	path =  kvzalloc(path_len, GFP_KERNEL);
	if (!path) {
		kvfree(buf);
		return -ENOMEM;
	}
	memcpy(buf, src, len);

	next = buf;
	next[len] = '\0';
	while ((cur = strsep(&next, ",;\n"))) {
		int n;
		char cmd;
		int end;

		cur = skip_spaces(cur);
		if (!*cur)
			continue;

		n = sscanf(cur, "%c %n %s %n %s %n",
			   &cmd, &end, path, &end,
			   reap_size, &end);
		if (n < 1 || cur[end]) {
			err = -EINVAL;
			break;
		} else if (n == 2) { /* eg: "1 16M" "1 /mnt/a.txt" */
			if (isdigit(path[0])) {
				memcpy(reap_size, path, 32);
				path[0] = '\0';
			}
		}

		err = run_cmd(cmd, path, reap_size);
		if (err)
			break;

		memset(path, 0, path_len);
		memset(reap_size, 0, 32);
	}

	kvfree(path);
	kvfree(buf);

	return err ? : len;
}
AIZEROFS_ATTR_WO(drop_caches);

bool config_bug_on = false;

static ssize_t bug_on_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", config_bug_on);
}

static ssize_t bug_on_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int val, ret;

	ret = kstrtoint(buf, 10, &val);
	if (ret)
		return ret;

	config_bug_on = !!val;
	pr_info("write val:%d\n", config_bug_on);
	return count;
}

AIZEROFS_ATTR(bug_on);

static struct attribute *aizerofs_attrs[] = {
	&drop_caches_attr.attr,
	&bug_on_attr.attr,
	NULL
};

static struct attribute_group aizerofs_attr_group = {
	.name = "aizerofs",
	.attrs = aizerofs_attrs,
};
#endif /* CONFIG_AIZEROFS_DEBUG_SYS_ABI */

DEFINE_STATIC_KEY_FALSE(aizerofs_enable);
static bool last_enabled = false;
static int aizerofs_enabled_store(const char *val,
		const struct kernel_param *kp)
{
	bool now_enabled;
	int rc = param_set_bool(val, kp);
	if (rc < 0)
		return rc;

	now_enabled = enabled;
	if (last_enabled != now_enabled) {
		last_enabled = now_enabled;
		if (enabled) {
			static_branch_enable(&aizerofs_enable);
		} else {
			static_branch_disable(&aizerofs_enable);
			dmabuf_caches_destroy_all();
		}
	}

	return 0;
}

static const struct kernel_param_ops enabled_param_ops = {
	.set = aizerofs_enabled_store,
	.get = param_get_bool,
};
module_param_cb(enabled, &enabled_param_ops, &enabled, 0600);
MODULE_PARM_DESC(enabled,
	"Enable or disable aizerofs (default: enable)");

static void fsnotify_open_hook(void *data, struct file *file, __u32 *mask)
{
	if (file->f_mode & FMODE_WRITE)
		*mask |= FS_OPEN_WRITE;
}

static int __init aizerofs_init(void)
{
	int ret;
	struct kprobe filp_open_kp = {
		.symbol_name = "filp_open"
	};

	/* get symbol address using kprobe */
	ret = register_kprobe(&filp_open_kp);
	if (ret) {
		pr_err("Failed to get filp_open addr from kprobe! ret=%d\n", ret);
		return ret;
	}

	filp_open_dup = (filp_open_t)filp_open_kp.addr;
	pr_info("suceesfully get filp_open addr:0x%px\n", filp_open_dup);
	unregister_kprobe(&filp_open_kp);

	ret = register_trace_android_vh_fsnotify_open(fsnotify_open_hook, NULL);
	if (ret) {
		pr_err("Failed to register fsnotify_open_hook! ret=%d\n", ret);
		return ret;
	}

	ret = misc_register(&aizerofs_misc_device);
	if (ret) {
		pr_err("Failed to register aizerofs\n");
		unregister_trace_android_vh_fsnotify_open(fsnotify_open_hook, NULL);
		return ret;
	}

	ret = dmabuf_cache_init();
	if (ret) {
		pr_err("Failed to dmabuf_cache_init\n");
		misc_deregister(&aizerofs_misc_device);
		unregister_trace_android_vh_fsnotify_open(fsnotify_open_hook, NULL);
		return ret;
	}

#ifdef CONFIG_AIZEROFS_DEBUG_SYS_ABI
	if (sysfs_create_group(kernel_kobj, &aizerofs_attr_group))
		pr_err("aizerofs: failed to create sysfs group\n");
#endif

	pr_info("aizerofs registered.\n");
	return 0;
}
module_init(aizerofs_init);

static void __exit aizerofs_exit(void)
{
	misc_deregister(&aizerofs_misc_device);
	unregister_trace_android_vh_fsnotify_open(fsnotify_open_hook, NULL);
}
module_exit(aizerofs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BS");
MODULE_DESCRIPTION("AI ZeroCopy Filesystem");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
