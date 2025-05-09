// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/pid.h>
#include <linux/fdtable.h>
#include <linux/rcupdate.h>
#include <linux/fs.h>
#include <linux/dma-buf.h>
#include <linux/sched/task.h>
#include <linux/version.h>
#include "msm_cvp_common.h"
#include "cvp_hfi_api.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_core.h"
#include "msm_cvp_dsp.h"
#include "eva_shared_def.h"
#include "cvp_presil.h"

extern bool trigger_smmu_fault;

void cvp_buf_map_set_vaddr(struct cvp_dma_buf_vmap *vmap, void *vaddr)
{
	#if (KERNEL_VERSION(5, 16, 0) > LINUX_VERSION_CODE)
		dma_buf_map_set_vaddr(&vmap->map, vaddr);
	#else
		iosys_map_set_vaddr(&vmap->map, vaddr);
	#endif
}

#define CLEAR_USE_BITMAP(idx, inst) \
	do { \
		clear_bit(idx, &inst->dma_cache.usage_bitmap); \
		dprintk(CVP_MEM, "clear %x bit %d dma_cache bitmap 0x%llx\n", \
			hash32_ptr(inst->session), smem->bitmap_index, \
			inst->dma_cache.usage_bitmap); \
	} while (0)

#define SET_USE_BITMAP(idx, inst) \
	do { \
		set_bit(idx, &inst->dma_cache.usage_bitmap); \
		dprintk(CVP_MEM, "Set %x bit %d dma_cache bitmap 0x%llx\n", \
			hash32_ptr(inst->session), idx, \
			inst->dma_cache.usage_bitmap); \
	} while (0)

struct cvp_oob_pool wncc_buf_pool;

static void _wncc_print_cvpwnccbufs_table(struct msm_cvp_inst* inst);
static int _wncc_unmap_metadata_bufs(struct eva_kmd_hfi_packet* in_pkt,
	struct eva_kmd_oob_wncc *wncc_oob,
	struct eva_kmd_wncc_metadata** wncc_metadata);

void msm_cvp_print_inst_bufs(struct msm_cvp_inst *inst, bool log);

int print_smem(u32 tag, const char *str, struct msm_cvp_inst *inst,
		struct msm_cvp_smem *smem)
{
	int i;
	char name[PKT_NAME_LEN] = "Unknown";


	if (!(tag & msm_cvp_debug))
		return 0;

	if (!inst || !smem) {
		dprintk(CVP_ERR, "Invalid inst 0x%llx or smem 0x%llx\n",
				inst, smem);
		return -EINVAL;
	}

	if (smem->dma_buf) {
		i = get_pkt_index_from_type(smem->pkt_type);
		if (i > 0)
			strlcpy(name, cvp_hfi_defs[i].name, PKT_NAME_LEN);

		if (!atomic_read(&smem->refcount))
			dprintk(tag,
				" UNUSED mapping %s: 0x%llx size %d iova %#x idx %d pkt_type %s buf_idx %#x fd %d\n",
				str, smem->dma_buf,
				smem->size, smem->device_addr, smem->bitmap_index, name, smem->buf_idx, smem->fd);
		else
			dprintk(tag,
				"%s: %x : 0x%llx size %d flags %#x iova %#x idx %d ref %d pkt_type %s buf_idx %#x fd %d\n",
				str, hash32_ptr(inst->session), smem->dma_buf,
				smem->size, smem->flags, smem->device_addr,
				smem->bitmap_index, atomic_read(&smem->refcount),
				name, smem->buf_idx, smem->fd);
	}
	return 0;
}

static void print_internal_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct cvp_internal_buf *cbuf)
{
	if (!(tag & msm_cvp_debug) || !inst || !cbuf)
		return;

	if (cbuf->smem->dma_buf) {
		dprintk(tag,
		"%s: %x : fd %d off %d 0x%llx %s size %d iova %#x\n",
		str, hash32_ptr(inst->session), cbuf->fd,
		cbuf->offset, cbuf->smem->dma_buf, cbuf->smem->dma_buf->name,
		cbuf->size, cbuf->smem->device_addr);
	} else {
		dprintk(tag,
		"%s: %x : idx %2d fd %d off %d size %d iova %#x\n",
		str, hash32_ptr(inst->session), cbuf->index, cbuf->fd,
		cbuf->offset, cbuf->size, cbuf->smem->device_addr);
	}
}

void print_cvp_buffer(u32 tag, const char *str, struct msm_cvp_inst *inst,
		struct cvp_internal_buf *cbuf)
{
	if (!inst || !cbuf) {
		dprintk(CVP_ERR,
			"%s Invalid params inst %pK, cbuf %pK\n",
			str, inst, cbuf);
		return;
	}

	print_smem(tag, str, inst, cbuf->smem);
}

static void _log_smem(struct inst_snapshot *snapshot, struct msm_cvp_inst *inst,
		struct msm_cvp_smem *smem, bool logging)
{

	if (print_smem(CVP_ERR, "bufdump", inst, smem))
		return;
	if (!logging || !snapshot)
		return;
	if (snapshot && snapshot->smem_index < MAX_ENTRIES) {
		struct smem_data *s;
		s = &snapshot->smem_log[snapshot->smem_index];
		snapshot->smem_index++;
		s->size = smem->size;
		s->flags = smem->flags;
		s->device_addr = smem->device_addr;
		s->bitmap_index = smem->bitmap_index;
		s->refcount = atomic_read(&smem->refcount);
		s->pkt_type = smem->pkt_type;
		s->buf_idx = smem->buf_idx;
	}
}

static void _log_buf(struct inst_snapshot *snapshot, enum smem_prop prop,
		struct msm_cvp_inst *inst, struct cvp_internal_buf *cbuf,
		bool logging)
{
	struct cvp_buf_data *buf = NULL;
	u32 index;
	if (prop == SMEM_CDSP && cbuf->smem->pkt_type == 0)
		return;
	print_cvp_buffer(CVP_ERR, "bufdump", inst, cbuf);
	if (!logging)
		return;
	if (snapshot) {
		if (prop == SMEM_CDSP && snapshot->dsp_index < MAX_ENTRIES) {
			index = snapshot->dsp_index;
			buf = &snapshot->dsp_buf_log[index];
			snapshot->dsp_index++;
		} else if (prop == SMEM_PERSIST &&
				snapshot->persist_index < MAX_ENTRIES) {
			index = snapshot->persist_index;
			buf = &snapshot->persist_buf_log[index];
			snapshot->persist_index++;
		}
		if (buf) {
			buf->device_addr = cbuf->smem->device_addr;
			buf->size = cbuf->size;
		}
	}
}

void print_client_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct eva_kmd_buffer *cbuf)
{
	if (!(tag & msm_cvp_debug) || !str || !inst || !cbuf)
		return;

	dprintk(tag,
		"%s: %x : idx %2d fd %d off %d size %d type %d flags 0x%x"
		" reserved[0] %u\n",
		str, hash32_ptr(inst->session), cbuf->index, cbuf->fd,
		cbuf->offset, cbuf->size, cbuf->type, cbuf->flags,
		cbuf->reserved[0]);
}

static bool __is_buf_valid(struct msm_cvp_inst *inst,
		struct eva_kmd_buffer *buf)
{
	struct cvp_hal_session *session;
	struct cvp_internal_buf *cbuf = (struct cvp_internal_buf *)0xdeadbeef;
	bool found = false;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return false;
	}

	if (buf->fd < 0) {
		dprintk(CVP_ERR, "%s: Invalid fd = %d", __func__, buf->fd);
		return false;
	}

	if (buf->offset) {
		dprintk(CVP_ERR,
			"%s: offset is deprecated, set to 0.\n",
			__func__);
		return false;
	}

	session = (struct cvp_hal_session *)inst->session;

	mutex_lock(&inst->cvpdspbufs.lock);
	list_for_each_entry(cbuf, &inst->cvpdspbufs.list, list) {
		if (cbuf->fd == buf->fd) {
			if (cbuf->size != buf->size) {
				dprintk(CVP_ERR, "%s: buf size mismatch\n",
					__func__);
				mutex_unlock(&inst->cvpdspbufs.lock);
				return false;
			}
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->cvpdspbufs.lock);
	if (found) {
		print_internal_buffer(CVP_ERR, "duplicate", inst, cbuf);
		return false;
	}

	return true;
}

static struct file *msm_cvp_fget(unsigned int fd, struct task_struct *task,
			fmode_t mask, unsigned int refs)
{
	struct files_struct *files = task->files;
	struct file *file;

	if (!files)
		return NULL;

	rcu_read_lock();
loop:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0))
	file = fcheck_files(files, fd);
#else
	file = files_lookup_fd_rcu(files, fd);
#endif
	if (file) {
		/* File object ref couldn't be taken.
		 * dup2() atomicity guarantee is the reason
		 * we loop to catch the new file (or NULL pointer)
		 */
		if (file->f_mode & mask)
			file = NULL;
		else if (!get_file_rcu(file))
			goto loop;
	}
	rcu_read_unlock();

	return file;
}

static struct dma_buf *cvp_dma_buf_get(struct file *file, int fd,
			struct task_struct *task)
{
	if (file->f_op != gfa_cv.dmabuf_f_op) {
		dprintk(CVP_WARN, "fd doesn't refer to dma_buf\n");
		return ERR_PTR(-EINVAL);
	}

	return file->private_data;
}

int msm_cvp_map_buf_dsp(struct msm_cvp_inst *inst, struct eva_kmd_buffer *buf)
{
	int rc = 0;
	struct cvp_internal_buf *cbuf = NULL;
	struct msm_cvp_smem *smem = NULL;
	struct dma_buf *dma_buf = NULL;
	struct file *file;

	if (!__is_buf_valid(inst, buf))
		return -EINVAL;

	if (!inst->task)
		return -EINVAL;

	file = msm_cvp_fget(buf->fd, inst->task, FMODE_PATH, 1);
	if (file == NULL) {
		dprintk(CVP_WARN, "%s fail to get file from fd %d %s\n", __func__, buf->fd, inst->proc_name);
		return -EINVAL;
	}

	dma_buf = cvp_dma_buf_get(
			file,
			buf->fd,
			inst->task);
	if (dma_buf == ERR_PTR(-EINVAL)) {
		dprintk(CVP_ERR, "%s: Invalid fd = %d", __func__, buf->fd);
		rc = -EINVAL;
		goto exit;
	}

	if (dma_buf->size < buf->size) {
		dprintk(CVP_ERR, "%s DSP client buffer too large %d > %d\n",
			__func__, buf->size, dma_buf->size);
		rc =  -EINVAL;
		goto exit;
	}

	dprintk(CVP_MEM, "dma_buf from internal %llu\n", dma_buf);

	cbuf = cvp_kmem_cache_zalloc(&cvp_driver->buf_cache, GFP_KERNEL);
	if (!cbuf) {
		rc = -ENOMEM;
		goto exit;
	}

	smem = cvp_kmem_cache_zalloc(&cvp_driver->smem_cache, GFP_KERNEL);
	if (!smem) {
		rc = -ENOMEM;
		goto exit;
	}

	smem->dma_buf = dma_buf;
	smem->bitmap_index = MAX_DMABUF_NUMS;
	smem->pkt_type = 0;
	smem->buf_idx = 0;
	smem->fd = buf->fd;
	dprintk(CVP_MEM, "%s: dma_buf = %llx\n", __func__, dma_buf);
	rc = msm_cvp_map_smem(inst, smem, "map dsp");
	if (rc) {
		print_client_buffer(CVP_ERR, "map failed", inst, buf);
		goto exit;
	}

	atomic_inc(&smem->refcount);
	cbuf->smem = smem;
	cbuf->fd = buf->fd;
	cbuf->size = buf->size;
	cbuf->offset = buf->offset;
	cbuf->ownership = CLIENT;
	cbuf->index = buf->index;

	buf->reserved[0] = (uint32_t)smem->device_addr;

	mutex_lock(&inst->cvpdspbufs.lock);
	list_add_tail(&cbuf->list, &inst->cvpdspbufs.list);
	mutex_unlock(&inst->cvpdspbufs.lock);

	return rc;

exit:
	fput(file);
	if (smem) {
		if (smem->device_addr)
			msm_cvp_unmap_smem(inst, smem, "unmap dsp");
		msm_cvp_smem_put_dma_buf(smem->dma_buf);
		cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
	}
	if (cbuf)
		cvp_kmem_cache_free(&cvp_driver->buf_cache, cbuf);
	return rc;
}

int msm_cvp_unmap_buf_dsp(struct msm_cvp_inst *inst, struct eva_kmd_buffer *buf)
{
	int rc = 0;
	bool found;
	struct cvp_internal_buf *cbuf = (struct cvp_internal_buf *)0xdeadbeef;
	struct cvp_hal_session *session;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	session = (struct cvp_hal_session *)inst->session;
	if (!session) {
		dprintk(CVP_ERR, "%s: invalid session\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&inst->cvpdspbufs.lock);
	found = false;
	list_for_each_entry(cbuf, &inst->cvpdspbufs.list, list) {
		if (cbuf->fd == buf->fd) {
			found = true;
			break;
		}
	}
	if (!found) {
		mutex_unlock(&inst->cvpdspbufs.lock);
		print_client_buffer(CVP_ERR, "invalid", inst, buf);
		return -EINVAL;
	}

	if (cbuf->smem->device_addr) {
		u64 idx = inst->unused_dsp_bufs.ktid;
		inst->unused_dsp_bufs.smem[idx] = *(cbuf->smem);
		inst->unused_dsp_bufs.nr++;
		inst->unused_dsp_bufs.nr =
			(inst->unused_dsp_bufs.nr > MAX_FRAME_BUFFER_NUMS)?
			MAX_FRAME_BUFFER_NUMS : inst->unused_dsp_bufs.nr;
		inst->unused_dsp_bufs.ktid = ++idx % MAX_FRAME_BUFFER_NUMS;

		msm_cvp_unmap_smem(inst, cbuf->smem, "unmap dsp");
		msm_cvp_smem_put_dma_buf(cbuf->smem->dma_buf);
		atomic_dec(&cbuf->smem->refcount);
	}
	list_del(&cbuf->list);
	mutex_unlock(&inst->cvpdspbufs.lock);

	cvp_kmem_cache_free(&cvp_driver->smem_cache, cbuf->smem);
	cvp_kmem_cache_free(&cvp_driver->buf_cache, cbuf);
	return rc;
}

int msm_cvp_map_buf_wncc(struct msm_cvp_inst *inst,
	struct eva_kmd_buffer *buf)
{
	int rc = 0, i;
	bool found = false;
	struct cvp_internal_buf* cbuf = (struct cvp_internal_buf *)0xdeadbeef;
	struct msm_cvp_smem* smem = NULL;
	struct dma_buf* dma_buf = NULL;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params", __func__);
		return -EINVAL;
	}

	if (!inst->session) {
		dprintk(CVP_ERR, "%s: invalid session", __func__);
		return -EINVAL;
	}

	if (buf->index) {
		dprintk(CVP_ERR, "%s: buf index is NOT 0 fd=%d",
			__func__, buf->fd);
		return -EINVAL;
	}

	if (buf->fd < 0) {
		dprintk(CVP_ERR, "%s: invalid fd = %d", __func__, buf->fd);
		return -EINVAL;
	}

	if (buf->offset) {
		dprintk(CVP_ERR, "%s: offset is not supported, set to 0.",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&inst->cvpwnccbufs.lock);
	list_for_each_entry(cbuf, &inst->cvpwnccbufs.list, list) {
		if (cbuf->fd == buf->fd) {
			if (cbuf->size != buf->size) {
				dprintk(CVP_ERR, "%s: buf size mismatch",
					__func__);
				mutex_unlock(&inst->cvpwnccbufs.lock);
				return -EINVAL;
			}
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->cvpwnccbufs.lock);
	if (found) {
		print_internal_buffer(CVP_ERR, "duplicate", inst, cbuf);
		return -EINVAL;
	}

	dma_buf = msm_cvp_smem_get_dma_buf(buf->fd);
	if (!dma_buf) {
		dprintk(CVP_ERR, "%s: invalid fd = %d", __func__, buf->fd);
		return -EINVAL;
	}

	cbuf = cvp_kmem_cache_zalloc(&cvp_driver->buf_cache, GFP_KERNEL);
	if (!cbuf) {
		msm_cvp_smem_put_dma_buf(dma_buf);
		return -ENOMEM;
	}

	smem = cvp_kmem_cache_zalloc(&cvp_driver->smem_cache, GFP_KERNEL);
	if (!smem) {
		cvp_kmem_cache_free(&cvp_driver->buf_cache, cbuf);
		msm_cvp_smem_put_dma_buf(dma_buf);
		return -ENOMEM;
	}

	smem->dma_buf = dma_buf;
	smem->bitmap_index = MAX_DMABUF_NUMS;
	smem->pkt_type = 0;
	smem->buf_idx = 0;
	smem->fd = buf->fd;
	dprintk(CVP_MEM, "%s: dma_buf = %llx", __func__, dma_buf);
	rc = msm_cvp_map_smem(inst, smem, "map wncc");
	if (rc) {
		dprintk(CVP_ERR, "%s: map failed", __func__);
		print_client_buffer(CVP_ERR, __func__, inst, buf);
		goto exit;
	}

	cbuf->smem = smem;
	cbuf->fd = buf->fd;
	cbuf->size = buf->size;
	cbuf->offset = buf->offset;
	cbuf->ownership = CLIENT;
	cbuf->index = buf->index;

	/* Added for PreSil/RUMI testing */
#ifdef USE_PRESIL
	dprintk(CVP_DBG,
		"wncc buffer is %x for cam_presil_send_buffer"
		" with MAP_ADDR_OFFSET %x",
		(u64)(smem->device_addr) - MAP_ADDR_OFFSET, MAP_ADDR_OFFSET);
	cam_presil_send_buffer((u64)smem->dma_buf, 0,
		(u32)cbuf->offset, (u32)cbuf->size,
		(u64)(smem->device_addr) - MAP_ADDR_OFFSET,
		(uintptr_t)NULL, false);
#endif

	mutex_lock(&inst->cvpwnccbufs.lock);
	if (inst->cvpwnccbufs_table == NULL) {
		inst->cvpwnccbufs_table =
			(struct msm_cvp_wncc_buffer*) kzalloc(
				sizeof(struct msm_cvp_wncc_buffer) *
				EVA_KMD_WNCC_MAX_SRC_BUFS,
				GFP_KERNEL);
		if (!inst->cvpwnccbufs_table) {
			mutex_unlock(&inst->cvpwnccbufs.lock);
			goto exit;
		}
	}

	for (i = 0; i < EVA_KMD_WNCC_MAX_SRC_BUFS; i++)
	{
		if (inst->cvpwnccbufs_table[i].iova == 0)
		{
			list_add_tail(&cbuf->list, &inst->cvpwnccbufs.list);
			inst->cvpwnccbufs_num++;
			inst->cvpwnccbufs_table[i].fd = buf->fd;
			inst->cvpwnccbufs_table[i].iova = smem->device_addr;
			inst->cvpwnccbufs_table[i].size = smem->size;

			/* buf reserved[0] used to store wncc src buf id */
			buf->reserved[0] = i + EVA_KMD_WNCC_SRC_BUF_ID_OFFSET;
			/* cbuf ktid used to store wncc src buf id */
			cbuf->ktid = i + EVA_KMD_WNCC_SRC_BUF_ID_OFFSET;

			dprintk(CVP_MEM, "%s: wncc buf iova: 0x%08X",
				__func__, inst->cvpwnccbufs_table[i].iova);
			break;
		}
	}
	if (i == EVA_KMD_WNCC_MAX_SRC_BUFS) {
		dprintk(CVP_ERR,
			"%s: wncc buf table full - max (%u) already registered",
			__func__, EVA_KMD_WNCC_MAX_SRC_BUFS);
		/* _wncc_print_cvpwnccbufs_table(inst); */
		mutex_unlock(&inst->cvpwnccbufs.lock);
		rc = -EDQUOT;
		goto exit;
	}
	mutex_unlock(&inst->cvpwnccbufs.lock);

	return rc;

exit:
	if (smem->device_addr)
		msm_cvp_unmap_smem(inst, smem, "unmap wncc");
	msm_cvp_smem_put_dma_buf(smem->dma_buf);
	cvp_kmem_cache_free(&cvp_driver->buf_cache, cbuf);
	cbuf = NULL;
	cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
	smem = NULL;
	return rc;
}

int msm_cvp_unmap_buf_wncc(struct msm_cvp_inst *inst,
	struct eva_kmd_buffer *buf)
{
	int rc = 0;
	bool found;
	struct cvp_internal_buf *cbuf = (struct cvp_internal_buf *)0xdeadbeef;
	uint32_t buf_id, buf_idx;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params", __func__);
		return -EINVAL;
	}

	if (!inst->session) {
		dprintk(CVP_ERR, "%s: invalid session", __func__);
		return -EINVAL;
	}

	if (buf->index) {
		dprintk(CVP_ERR, "%s: buf index is NOT 0 fd=%d",
			__func__, buf->fd);
		return -EINVAL;
	}

	buf_id = buf->reserved[0];
	if (buf_id < EVA_KMD_WNCC_SRC_BUF_ID_OFFSET || buf_id >=
		(EVA_KMD_WNCC_MAX_SRC_BUFS + EVA_KMD_WNCC_SRC_BUF_ID_OFFSET)) {
		dprintk(CVP_ERR, "%s: invalid buffer id %d",
			__func__, buf->reserved[0]);
		return -EINVAL;
	}

	mutex_lock(&inst->cvpwnccbufs.lock);
	if (inst->cvpwnccbufs_num == 0) {
		dprintk(CVP_ERR, "%s: no wncc buffers currently mapped", __func__);
		mutex_unlock(&inst->cvpwnccbufs.lock);
		return -EINVAL;
	}

	buf_idx = buf_id - EVA_KMD_WNCC_SRC_BUF_ID_OFFSET;
	if (inst->cvpwnccbufs_table[buf_idx].iova == 0) {
		dprintk(CVP_ERR, "%s: buffer id %d not found",
			__func__, buf_id);
		mutex_unlock(&inst->cvpwnccbufs.lock);
		return -EINVAL;
	}

	buf->fd = inst->cvpwnccbufs_table[buf_idx].fd;
	found = false;
	list_for_each_entry(cbuf, &inst->cvpwnccbufs.list, list) {
		if (cbuf->fd == buf->fd) {
			found = true;
			break;
		}
	}
	if (!found) {
		dprintk(CVP_ERR, "%s: buffer id %d not found",
			__func__, buf_id);
		print_client_buffer(CVP_ERR, __func__, inst, buf);
		_wncc_print_cvpwnccbufs_table(inst);
		mutex_unlock(&inst->cvpwnccbufs.lock);
		return -EINVAL;
	}
	if (cbuf->smem->device_addr) {
		u64 idx = inst->unused_wncc_bufs.ktid;
		inst->unused_wncc_bufs.smem[idx] = *(cbuf->smem);
		inst->unused_wncc_bufs.nr++;
		inst->unused_wncc_bufs.nr =
			(inst->unused_wncc_bufs.nr > NUM_WNCC_BUFS)?
			NUM_WNCC_BUFS : inst->unused_wncc_bufs.nr;
		inst->unused_wncc_bufs.ktid = ++idx % NUM_WNCC_BUFS;
	}

	if (cbuf->smem->device_addr) {
		msm_cvp_unmap_smem(inst, cbuf->smem, "unmap wncc");
		msm_cvp_smem_put_dma_buf(cbuf->smem->dma_buf);
	}

	list_del(&cbuf->list);
	inst->cvpwnccbufs_table[buf_idx].fd = 0;
	inst->cvpwnccbufs_table[buf_idx].iova = 0;
	inst->cvpwnccbufs_table[buf_idx].size = 0;
	inst->cvpwnccbufs_num--;
	if (inst->cvpwnccbufs_num == 0) {
		kfree(inst->cvpwnccbufs_table);
		inst->cvpwnccbufs_table = NULL;
	}
	mutex_unlock(&inst->cvpwnccbufs.lock);

	cvp_kmem_cache_free(&cvp_driver->smem_cache, cbuf->smem);
	cvp_kmem_cache_free(&cvp_driver->buf_cache, cbuf);
	return rc;
}

static void _wncc_print_oob(struct eva_kmd_oob_wncc* wncc_oob)
{
	u32 i, j;

	if (!wncc_oob) {
		dprintk(CVP_ERR, "%s: invalid params", __func__);
		return;
	}

	dprintk(CVP_DBG, "%s: wncc OOB --", __func__);
	dprintk(CVP_DBG, "%s: num_layers: %u", __func__, wncc_oob->num_layers);
	for (i = 0; i < wncc_oob->num_layers; i++) {
		dprintk(CVP_DBG, "%s:   layers[%u].num_addrs: %u",
			__func__, i, wncc_oob->layers[i].num_addrs);

		for (j = 0; j < wncc_oob->layers[i].num_addrs; j++) {
			dprintk(CVP_DBG,
				"%s:    layers[%u].addrs[%u]: %04u 0x%08x",
				__func__, i, j,
				wncc_oob->layers[i].addrs[j].buffer_id,
				wncc_oob->layers[i].addrs[j].offset);
		}
	}
}

static void _wncc_print_cvpwnccbufs_table(struct msm_cvp_inst* inst)
{
	u32 i, entries = 0;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params", __func__);
		return;
	}

	if (inst->cvpwnccbufs_num == 0) {
		dprintk(CVP_DBG, "%s: wncc buffer look-up table is empty",
			__func__);
		return;
	}

	if (!inst->cvpwnccbufs_table) {
		dprintk(CVP_ERR, "%s: invalid params", __func__);
		return;
	}

	dprintk(CVP_DBG, "%s: wncc buffer table:", __func__);
	for (i = 0; i < EVA_KMD_WNCC_MAX_SRC_BUFS &&
		entries < inst->cvpwnccbufs_num; i++) {
		if (inst->cvpwnccbufs_table[i].iova != 0) {
			dprintk(CVP_DBG,
				"%s: buf_idx=%04d --> "
				"fd=%03d, iova=0x%08x, size=%d",
				__func__, i,
				inst->cvpwnccbufs_table[i].fd,
				inst->cvpwnccbufs_table[i].iova,
				inst->cvpwnccbufs_table[i].size);
			entries++;
		}
	}
}

static void _wncc_print_metadata_buf(u32 num_layers, u32 num_addrs,
	struct eva_kmd_wncc_metadata** wncc_metadata)
{
	u32 i, j, iova;

	if (num_layers < 1 || num_layers > EVA_KMD_WNCC_MAX_LAYERS ||
		!wncc_metadata) {
		dprintk(CVP_ERR, "%s: invalid params", __func__);
		return;
	}

	dprintk(CVP_DBG, "%s: wncc metadata buffers --", __func__);
	dprintk(CVP_DBG, "%s: num_layers: %u", __func__, num_layers);
	dprintk(CVP_DBG, "%s: num_addrs:  %u", __func__, num_addrs);
	for (i = 0; i < num_layers; i++) {
		for (j = 0; j < num_addrs; j++) {
			iova = (wncc_metadata[i][j].iova_msb << 22) |
				wncc_metadata[i][j].iova_lsb;
			dprintk(CVP_DBG,
				"%s:   wncc_metadata[%u][%u]: "
				"%4u %3u %4u %3u 0x%08x %1u %4d %4d %4d %4d",
				__func__, i, j,
				wncc_metadata[i][j].loc_x_dec,
				wncc_metadata[i][j].loc_x_frac,
				wncc_metadata[i][j].loc_y_dec,
				wncc_metadata[i][j].loc_y_frac,
				iova,
				wncc_metadata[i][j].scale_idx,
				wncc_metadata[i][j].aff_coeff_3,
				wncc_metadata[i][j].aff_coeff_2,
				wncc_metadata[i][j].aff_coeff_1,
				wncc_metadata[i][j].aff_coeff_0);
		}
	}
}

static int _wncc_copy_oob_from_user(struct eva_kmd_hfi_packet* in_pkt,
	struct eva_kmd_oob_wncc* wncc_oob)
{
	int rc = 0;
	u32 oob_type = 0;
	struct eva_kmd_oob_buf* oob_buf_u;
	struct eva_kmd_oob_wncc* wncc_oob_u;
	struct eva_kmd_oob_wncc* wncc_oob_k;
	unsigned int i;
	u32 num_addrs;

	if (!in_pkt || !wncc_oob) {
		dprintk(CVP_ERR, "%s: invalid params", __func__);
		return -EINVAL;
	}

	oob_buf_u = in_pkt->oob_buf;
	if (!access_ok(oob_buf_u, sizeof(*oob_buf_u))) {
		dprintk(CVP_ERR, "%s: invalid OOB buf pointer", __func__);
		return -EINVAL;
	}

	if (!access_ok(&oob_buf_u->oob_type, sizeof(oob_buf_u->oob_type))) {
		dprintk(CVP_ERR,
			"%s: bad OOB buf pointer, oob_type inaccessible",
			__func__);
		return -EINVAL;
	}
	rc = get_user(oob_type, &oob_buf_u->oob_type);
	if (rc)
		return rc;
	if (oob_type != EVA_KMD_OOB_WNCC) {
		dprintk(CVP_ERR, "%s: incorrect OOB type (%d) for wncc",
			__func__, oob_type);
		return -EINVAL;
	}

	wncc_oob_u = &oob_buf_u->wncc;
	wncc_oob_k = wncc_oob;

	if (!access_ok(&wncc_oob_u->metadata_bufs_offset,
		sizeof(wncc_oob_u->metadata_bufs_offset))) {
		dprintk(CVP_ERR,
			"%s: bad OOB buf pointer, wncc.metadata_bufs_offset inaccessible",
			__func__);
		return -EINVAL;
	}
	rc = get_user(wncc_oob_k->metadata_bufs_offset,
		&wncc_oob_u->metadata_bufs_offset);
	if (rc)
		return rc;
	if (wncc_oob_k->metadata_bufs_offset > ((sizeof(in_pkt->pkt_data)
		- sizeof(struct cvp_buf_type)) / sizeof(__u32))) {
		dprintk(CVP_ERR, "%s: invalid wncc metadata bufs offset",
			__func__);
		return -EINVAL;
	}

	if (!access_ok(&wncc_oob_u->num_layers,
		sizeof(wncc_oob_u->num_layers))) {
		dprintk(CVP_ERR,
			"%s: bad OOB buf pointer, wncc.num_layers inaccessible",
			__func__);
		return -EINVAL;
	}
	rc = get_user(wncc_oob_k->num_layers, &wncc_oob_u->num_layers);
	if (rc)
		return rc;
	if (wncc_oob_k->num_layers < 1 ||
		wncc_oob_k->num_layers > EVA_KMD_WNCC_MAX_LAYERS) {
		dprintk(CVP_ERR, "%s: invalid wncc num layers", __func__);
		return -EINVAL;
	}

	for (i = 0; i < wncc_oob_k->num_layers; i++) {

		if (!access_ok(&wncc_oob_u->layers[i].num_addrs,
			sizeof(wncc_oob_u->layers[i].num_addrs))) {
			dprintk(CVP_ERR,
				"%s: bad OOB buf pointer, wncc.layers[%u].num_addrs inaccessible",
				__func__, i);
			return -EINVAL;
		}
		rc = get_user(wncc_oob_k->layers[i].num_addrs,
			&wncc_oob_u->layers[i].num_addrs);
		if (rc)
			break;

		num_addrs = wncc_oob_k->layers[i].num_addrs;
		if (num_addrs < 1 || num_addrs > EVA_KMD_WNCC_MAX_ADDRESSES) {
			dprintk(CVP_ERR,
				"%s: invalid wncc num addrs for layer %u",
				__func__, i);
			rc = -EINVAL;
			break;
		}

		if (!access_ok(wncc_oob_u->layers[i].addrs,
				num_addrs * sizeof(struct eva_kmd_wncc_addr)) ||
			!access_ok(&wncc_oob_u->layers[i].addrs[num_addrs - 1],
				sizeof(struct eva_kmd_wncc_addr))) {
			dprintk(CVP_ERR,
				"%s: bad OOB buf pointer, wncc.layers[%u].addrs inaccessible",
				__func__, i);
			return -EINVAL;
		}
		rc = copy_from_user(wncc_oob_k->layers[i].addrs,
			wncc_oob_u->layers[i].addrs,
			num_addrs * sizeof(struct eva_kmd_wncc_addr));
		if (rc)
			break;
	}

	if (false)
		_wncc_print_oob(wncc_oob);

	return rc;
}

static int _wncc_map_metadata_bufs(struct eva_kmd_hfi_packet* in_pkt,
	struct eva_kmd_oob_wncc *wncc_oob,
	struct eva_kmd_wncc_metadata** wncc_metadata)
{
	int rc = 0, i;
	struct cvp_buf_type* wncc_metadata_bufs;
	struct dma_buf* dmabuf;
	struct cvp_dma_buf_vmap vmap = {0};
	__u32 num_layers, metadata_bufs_offset;

	cvp_buf_map_set_vaddr(&vmap, (void *)0xdeadbeaf);

	if (!in_pkt || !wncc_metadata || !wncc_oob) {
		dprintk(CVP_ERR, "%s: invalid params", __func__);
		return -EINVAL;
	}

	num_layers = wncc_oob->num_layers;
	metadata_bufs_offset = wncc_oob->metadata_bufs_offset;
	if (num_layers < 1 || num_layers > EVA_KMD_WNCC_MAX_LAYERS) {
		dprintk(CVP_ERR, "%s: invalid wncc num layers", __func__);
		return -EINVAL;
	}
	if (metadata_bufs_offset > ((sizeof(in_pkt->pkt_data)
		- num_layers * sizeof(struct cvp_buf_type)) / sizeof(__u32))) {
		dprintk(CVP_ERR, "%s: invalid wncc metadata bufs offset",
			__func__);
		return -EINVAL;
	}

	wncc_metadata_bufs = (struct cvp_buf_type*)
		&in_pkt->pkt_data[metadata_bufs_offset];
	for (i = 0; i < num_layers; i++) {
		dmabuf = dma_buf_get(wncc_metadata_bufs[i].fd);
		if (IS_ERR(dmabuf)) {
			rc = PTR_ERR(dmabuf);
			dprintk(CVP_ERR,
				"%s: dma_buf_get() failed for "
				"wncc_metadata_bufs[%d], rc %d",
				__func__, i, rc);
			break;
		}

		if (dmabuf->size < wncc_oob->layers[i].num_addrs *
			sizeof(struct eva_kmd_wncc_metadata)) {
			dprintk(CVP_ERR,
				"%s: wncc_metadata_bufs[%d] size insufficient for num addrs in oob",
				__func__, i);
			dma_buf_put(dmabuf);
			rc = -EINVAL;
			break;
		}

		rc = dma_buf_begin_cpu_access(dmabuf, DMA_TO_DEVICE);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: dma_buf_begin_cpu_access() failed "
				"for wncc_metadata_bufs[%d], rc %d",
				__func__, i, rc);
			dma_buf_put(dmabuf);
			break;
		}

		rc = msm_cvp_dma_buf_vmap(dmabuf, &vmap);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: dma_buf_vmap() failed for "
				"wncc_metadata_bufs[%d]",
				__func__, i);
			dma_buf_end_cpu_access(dmabuf, DMA_TO_DEVICE);
			dma_buf_put(dmabuf);
			break;
		}
		dprintk(CVP_DBG,
			"%s: wncc_metadata_bufs[%d] map.is_iomem is %d",
			__func__, i, vmap.map.is_iomem);
		wncc_metadata[i] = (struct eva_kmd_wncc_metadata *)vmap.vaddr;

		dma_buf_put(dmabuf);
	}

	if (rc)
		_wncc_unmap_metadata_bufs(in_pkt, wncc_oob, wncc_metadata);

	return rc;
}

static int _wncc_unmap_metadata_bufs(struct eva_kmd_hfi_packet* in_pkt,
	struct eva_kmd_oob_wncc *wncc_oob,
	struct eva_kmd_wncc_metadata** wncc_metadata)
{
	int rc = 0, i;
	struct cvp_buf_type* wncc_metadata_bufs;
	struct dma_buf* dmabuf;
	struct cvp_dma_buf_vmap vmap = {0};
	__u32 num_layers, metadata_bufs_offset;

	if (!in_pkt || !wncc_metadata || !wncc_oob) {
		dprintk(CVP_ERR, "%s: invalid params", __func__);
		return -EINVAL;
	}

	num_layers = wncc_oob->num_layers;
	metadata_bufs_offset = wncc_oob->metadata_bufs_offset;
	if (num_layers < 1 || num_layers > EVA_KMD_WNCC_MAX_LAYERS) {
		dprintk(CVP_ERR, "%s: invalid wncc num layers", __func__);
		return -EINVAL;
	}
	if (metadata_bufs_offset > ((sizeof(in_pkt->pkt_data)
		- num_layers * sizeof(struct cvp_buf_type)) / sizeof(__u32))) {
		dprintk(CVP_ERR, "%s: invalid wncc metadata bufs offset",
			__func__);
		return -EINVAL;
	}

	wncc_metadata_bufs = (struct cvp_buf_type*)
		&in_pkt->pkt_data[metadata_bufs_offset];
	for (i = 0; i < num_layers; i++) {
		if (!wncc_metadata[i]) {
			continue;
		}

		dmabuf = dma_buf_get(wncc_metadata_bufs[i].fd);
		if (IS_ERR(dmabuf)) {
			rc = -PTR_ERR(dmabuf);
			dprintk(CVP_ERR,
				"%s: dma_buf_get() failed for "
				"wncc_metadata_bufs[%d], rc %d",
				__func__, i, rc);
			break;
		}

		cvp_buf_map_set_vaddr(&vmap, (void *)wncc_metadata[i]);
		msm_cvp_dma_buf_vunmap(dmabuf, &vmap);
		wncc_metadata[i] = NULL;

		rc = dma_buf_end_cpu_access(dmabuf, DMA_TO_DEVICE);
		dma_buf_put(dmabuf);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: dma_buf_end_cpu_access() failed "
				"for wncc_metadata_bufs[%d], rc %d",
				__func__, i, rc);
			break;
		}
	}

	return rc;
}

static int init_wncc_bufs(void)
{
	int i;

	for (i = 0; i < NUM_WNCC_BUFS; i++) {
		wncc_buf_pool.bufs[i] = (struct eva_kmd_oob_wncc*)kzalloc(
				sizeof(struct eva_kmd_oob_wncc), GFP_KERNEL);
		if (!wncc_buf_pool.bufs[i]) {
			i--;
			goto exit_fail;
		}
	}
	wncc_buf_pool.used_bitmap = 0;
	wncc_buf_pool.allocated = true;
	return 0;

exit_fail:
	while (i >= 0) {
		kfree(wncc_buf_pool.bufs[i]);
		i--;
	}
	return -ENOMEM;
}

static int alloc_wncc_buf(struct wncc_oob_buf *wob)
{
	int rc, i;

	mutex_lock(&wncc_buf_pool.lock);
	if (!wncc_buf_pool.allocated) {
		rc = init_wncc_bufs();
		if (rc) {
			mutex_unlock(&wncc_buf_pool.lock);
			return rc;
		}
	}

	for (i = 0; i < NUM_WNCC_BUFS; i++) {
		if (!(wncc_buf_pool.used_bitmap & BIT(i))) {
			wncc_buf_pool.used_bitmap |= BIT(i);
			wob->bitmap_idx = i;
			wob->buf = wncc_buf_pool.bufs[i];
			mutex_unlock(&wncc_buf_pool.lock);
			return 0;
		}
	}
	mutex_unlock(&wncc_buf_pool.lock);
	wob->bitmap_idx = 0xff;
	wob->buf = (struct eva_kmd_oob_wncc*)kzalloc(
			sizeof(struct eva_kmd_oob_wncc), GFP_KERNEL);
	if (!wob->buf)
		rc = -ENOMEM;
	else
		rc = 0;

	return rc;
}

static void free_wncc_buf(struct wncc_oob_buf *wob)
{
	if (!wob)
		return;

	if (wob->bitmap_idx == 0xff) {
		kfree(wob->buf);
		return;
	}

	if (wob->bitmap_idx < NUM_WNCC_BUFS) {
		mutex_lock(&wncc_buf_pool.lock);
		wncc_buf_pool.used_bitmap &= ~BIT(wob->bitmap_idx);
		memset(wob->buf, 0, sizeof(struct eva_kmd_oob_wncc));
		wob->buf = NULL;
		mutex_unlock(&wncc_buf_pool.lock);
	}
}

static unsigned int eva_calculate_crc(unsigned int *pbuf, uint32_t size)
{
	unsigned long crc_value = 0;

	for (uint32_t i = 0; i < (size / sizeof(uint32_t)); i++)
		crc_value += *(pbuf + i);
	return (unsigned int)(crc_value & 0xFFFFFFFF);
}

static int msm_cvp_proc_oob_wncc(struct msm_cvp_inst* inst,
	struct eva_kmd_hfi_packet* in_pkt)
{
	int rc = 0;
	struct eva_kmd_oob_wncc* wncc_oob;
	struct wncc_oob_buf wob;
	struct eva_kmd_wncc_metadata* wncc_metadata[EVA_KMD_WNCC_MAX_LAYERS];
	struct cvp_buf_type *wncc_metadata_bufs;
	unsigned int i, j;
	bool empty = false;
	u32 buf_id, buf_idx, buf_offset, iova;

	if (!inst || !inst->core || !in_pkt) {
		dprintk(CVP_ERR, "%s: invalid params", __func__);
		return -EINVAL;
	}

	rc = alloc_wncc_buf(&wob);
	if (rc)
		return -ENOMEM;

	wncc_oob = wob.buf;
	rc = _wncc_copy_oob_from_user(in_pkt, wncc_oob);
	if (rc) {
		dprintk(CVP_ERR, "%s: OOB buf copying failed", __func__);
		goto exit;
	}

	memset(wncc_metadata, 0,
		sizeof(*wncc_metadata) * EVA_KMD_WNCC_MAX_LAYERS);
	rc = _wncc_map_metadata_bufs(in_pkt, wncc_oob, wncc_metadata);
	if (rc) {
		dprintk(CVP_ERR, "%s: failed to map wncc metadata bufs",
			__func__);
		goto exit;
	}

	mutex_lock(&inst->cvpwnccbufs.lock);
	if (inst->cvpwnccbufs_num == 0 || inst->cvpwnccbufs_table == NULL) {
		dprintk(CVP_ERR, "%s: no wncc bufs currently mapped", __func__);
		empty = true;
		rc = -EINVAL;
	}

	for (i = 0; !empty && i < wncc_oob->num_layers; i++) {
		for (j = 0; j < wncc_oob->layers[i].num_addrs; j++) {
			buf_id = wncc_oob->layers[i].addrs[j].buffer_id;
			if (buf_id < EVA_KMD_WNCC_SRC_BUF_ID_OFFSET ||
				buf_id >= (EVA_KMD_WNCC_SRC_BUF_ID_OFFSET +
					EVA_KMD_WNCC_MAX_SRC_BUFS)) {
				dprintk(CVP_ERR,
					"%s: invalid wncc buf id %u "
					"in layer #%u address #%u",
					__func__, buf_id, i, j);
				rc = -EINVAL;
				break;
			}

			buf_idx = buf_id - EVA_KMD_WNCC_SRC_BUF_ID_OFFSET;
			if (inst->cvpwnccbufs_table[buf_idx].iova == 0) {
				dprintk(CVP_ERR,
					"%s: unmapped wncc buf id %u "
					"in layer #%u address #%u",
					__func__, buf_id, i, j);
				/* _wncc_print_cvpwnccbufs_table(inst); */
				rc = -EINVAL;
				break;
			}

			buf_offset = wncc_oob->layers[i].addrs[j].offset;
			if (buf_offset >=
				inst->cvpwnccbufs_table[buf_idx].size) {
				/* NOTE: This buffer offset validation is
				 * not comprehensive since wncc src image
				 * resolution information is not known to
				 * KMD. UMD is responsible for comprehensive
				 * validation.
				 */
				dprintk(CVP_ERR,
					"%s: invalid wncc buf offset %u "
					"in layer #%u address #%u",
					__func__, buf_offset, i, j);
				rc = -EINVAL;
				break;
			}

			iova = inst->cvpwnccbufs_table[buf_idx].iova +
				buf_offset;
			wncc_metadata[i][j].iova_lsb = iova;
			wncc_metadata[i][j].iova_msb = iova >> 22;
		}

		wncc_metadata_bufs = (struct cvp_buf_type *)
			&in_pkt->pkt_data[wncc_oob->metadata_bufs_offset];
		if ((wncc_metadata_bufs[i].debug_flags & 0x00000001) != 0) {
			wncc_metadata_bufs[i].crc =
				eva_calculate_crc((unsigned int *)wncc_metadata[i],
				wncc_metadata_bufs[i].size);
			dprintk(CVP_MEM, "%s: updated CRC 0x%x", __func__,
				wncc_metadata_bufs[i].crc);
		}
	}
	mutex_unlock(&inst->cvpwnccbufs.lock);

	if (false)
		_wncc_print_metadata_buf(wncc_oob->num_layers,
			wncc_oob->layers[0].num_addrs, wncc_metadata);

	if (_wncc_unmap_metadata_bufs(in_pkt, wncc_oob, wncc_metadata)) {
		dprintk(CVP_ERR, "%s: failed to unmap wncc metadata bufs",
			__func__);
	}

exit:
	free_wncc_buf(&wob);
	return rc;
}

int msm_cvp_proc_oob(struct msm_cvp_inst* inst,
	struct eva_kmd_hfi_packet* in_pkt)
{
	int rc = 0;
	struct cvp_hfi_cmd_session_hdr* cmd_hdr =
		(struct cvp_hfi_cmd_session_hdr*)in_pkt;

	if (!inst || !inst->core || !in_pkt) {
		dprintk(CVP_ERR, "%s: invalid params", __func__);
		return -EINVAL;
	}

	switch (cmd_hdr->packet_type) {
	case HFI_CMD_SESSION_CVP_WARP_NCC_FRAME:
		rc = msm_cvp_proc_oob_wncc(inst, in_pkt);
		break;
	default:
		break;
	}

	return rc;
}

void msm_cvp_cache_operations(struct msm_cvp_smem *smem, u32 type,
				u32 offset, u32 size)
{
	enum smem_cache_ops cache_op;

	if (msm_cvp_cacheop_disabled)
		return;

	if (!smem) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return;
	}

	switch (type) {
	case EVA_KMD_BUFTYPE_INPUT:
		cache_op = SMEM_CACHE_CLEAN;
		break;
	case EVA_KMD_BUFTYPE_OUTPUT:
		cache_op = SMEM_CACHE_INVALIDATE;
		break;
	default:
		cache_op = SMEM_CACHE_CLEAN_INVALIDATE;
	}

	dprintk(CVP_MEM,
		"%s: cache operation enabled for dma_buf: %llx, cache_op: %d, offset: %d, size: %d\n",
		__func__, smem->dma_buf, cache_op, offset, size);
	msm_cvp_smem_cache_operations(smem->dma_buf, cache_op, offset, size);
}

static struct msm_cvp_smem *msm_cvp_session_find_smem(struct msm_cvp_inst *inst,
				struct dma_buf *dma_buf,
				u32 pkt_type)
{
	struct msm_cvp_smem *smem;
	struct msm_cvp_frame *frame = (struct msm_cvp_frame *)0xdeadbeef;
	struct cvp_internal_buf *buf = (struct cvp_internal_buf *)0xdeadbeef;
	int i;

	if (inst->dma_cache.nr > MAX_DMABUF_NUMS)
		return NULL;

	mutex_lock(&inst->dma_cache.lock);
	for (i = 0; i < inst->dma_cache.nr; i++)
		if (inst->dma_cache.entries[i]->dma_buf == dma_buf) {
			SET_USE_BITMAP(i, inst);
			smem = inst->dma_cache.entries[i];
			smem->bitmap_index = i;
			smem->pkt_type = pkt_type;
			atomic_inc(&smem->refcount);
			/*
			 * If we find it, it means we already increased
			 * refcount before, so we put it to avoid double
			 * incremental.
			 */
			msm_cvp_smem_put_dma_buf(smem->dma_buf);
			mutex_unlock(&inst->dma_cache.lock);
			print_smem(CVP_MEM, "found in cache", inst, smem);
			return smem;
		}

	mutex_unlock(&inst->dma_cache.lock);

	/* earch persist list */
	mutex_lock(&inst->persistbufs.lock);
	list_for_each_entry(buf, &inst->persistbufs.list, list) {
		smem = buf->smem;
		if (smem && smem->dma_buf == dma_buf) {
			atomic_inc(&smem->refcount);
			mutex_unlock(&inst->persistbufs.lock);
			print_smem(CVP_MEM, "found in persist", inst, smem);
			return smem;
		}
	}
	mutex_unlock(&inst->persistbufs.lock);

	/* Search frame list */
	mutex_lock(&inst->frames.lock);
	list_for_each_entry(frame, &inst->frames.list, list) {
		for (i = 0; i < frame->nr; i++) {
			smem = frame->bufs[i].smem;
			if (smem && smem->dma_buf == dma_buf) {
				atomic_inc(&smem->refcount);
				mutex_unlock(&inst->frames.lock);
				print_smem(CVP_MEM, "found in frame",
					inst, smem);
				return smem;
			}
		}
	}
	mutex_unlock(&inst->frames.lock);

	return NULL;
}

static int msm_cvp_session_add_smem(struct msm_cvp_inst *inst,
				struct msm_cvp_smem *smem)
{
	unsigned int i;
	struct msm_cvp_smem *smem2;

	mutex_lock(&inst->dma_cache.lock);
	if (inst->dma_cache.nr < MAX_DMABUF_NUMS) {
		inst->dma_cache.entries[inst->dma_cache.nr] = smem;
		SET_USE_BITMAP(inst->dma_cache.nr, inst);
		smem->bitmap_index = inst->dma_cache.nr;
		inst->dma_cache.nr++;
		i = smem->bitmap_index;
	} else {
		i = find_first_zero_bit(&inst->dma_cache.usage_bitmap,
				MAX_DMABUF_NUMS);
		if (i < MAX_DMABUF_NUMS) {
			smem2 = inst->dma_cache.entries[i];
			msm_cvp_unmap_smem(inst, smem2, "unmap cpu");
			msm_cvp_smem_put_dma_buf(smem2->dma_buf);
			cvp_kmem_cache_free(&cvp_driver->smem_cache, smem2);

			inst->dma_cache.entries[i] = smem;
			smem->bitmap_index = i;
			SET_USE_BITMAP(i, inst);
		} else {
			dprintk(CVP_WARN,
			"%s: reached limit, fallback to buf mapping list\n"
			, __func__);
			atomic_inc(&smem->refcount);
			mutex_unlock(&inst->dma_cache.lock);
			return -ENOMEM;
		}
	}

	atomic_inc(&smem->refcount);
	mutex_unlock(&inst->dma_cache.lock);
	dprintk(CVP_MEM, "Add entry %d into cache\n", i);

	return 0;
}

static struct msm_cvp_smem *msm_cvp_session_get_smem(struct msm_cvp_inst *inst,
						struct cvp_buf_type *buf,
						bool is_persist,
						u32 pkt_type)
{
	int rc = 0, found = 1;
	struct msm_cvp_smem *smem = NULL;
	struct dma_buf *dma_buf = NULL;

	if (buf->fd < 0) {
		dprintk(CVP_ERR, "%s: Invalid fd = %d", __func__, buf->fd);
		return NULL;
	}

	dma_buf = msm_cvp_smem_get_dma_buf(buf->fd);
	if (!dma_buf) {
		dprintk(CVP_ERR, "%s: Invalid fd = %d", __func__, buf->fd);
		return NULL;
	}

	if (is_persist) {
		smem = cvp_kmem_cache_zalloc(&cvp_driver->smem_cache, GFP_KERNEL);
		if (!smem)
			return NULL;

		smem->dma_buf = dma_buf;
		smem->bitmap_index = MAX_DMABUF_NUMS;
		smem->pkt_type = pkt_type;
		smem->flags |= SMEM_PERSIST;
		smem->fd = buf->fd;
		atomic_inc(&smem->refcount);
		rc = msm_cvp_map_smem(inst, smem, "map cpu");
		if (rc)
			goto exit;
		if (!IS_CVP_BUF_VALID(buf, smem)) {
			dprintk(CVP_ERR,
				"%s: invalid offset %d or size %d persist\n",
				__func__, buf->offset, buf->size);
			goto exit2;
		}
		return smem;
	}

	smem = msm_cvp_session_find_smem(inst, dma_buf, pkt_type);
	if (!smem) {
		found = 0;
		smem = cvp_kmem_cache_zalloc(&cvp_driver->smem_cache, GFP_KERNEL);
		if (!smem)
			return NULL;

		smem->dma_buf = dma_buf;
		smem->bitmap_index = MAX_DMABUF_NUMS;
		smem->pkt_type = pkt_type;
		smem->fd = buf->fd;
		if (is_params_pkt(pkt_type))
			smem->flags |= SMEM_PERSIST;
		rc = msm_cvp_map_smem(inst, smem, "map cpu");
		if (rc)
			goto exit;
		if (!IS_CVP_BUF_VALID(buf, smem)) {
			dprintk(CVP_ERR,
				"%s: invalid buf %d %d fd %d dma 0x%llx %s %d type %#x\n",
				__func__, buf->offset, buf->size, buf->fd,
				dma_buf, dma_buf->name, dma_buf->size, pkt_type);
			goto exit2;
		}
		rc = msm_cvp_session_add_smem(inst, smem);
		if (rc && rc != -ENOMEM)
			goto exit2;
		return smem;
	}

	if (!IS_CVP_BUF_VALID(buf, smem)) {
		dprintk(CVP_ERR, "%s: invalid offset %d or size %d found\n",
			__func__, buf->offset, buf->size);
		if (found) {
			mutex_lock(&inst->dma_cache.lock);
			atomic_dec(&smem->refcount);
			mutex_unlock(&inst->dma_cache.lock);
			return NULL;
		}
		goto exit2;
	}

	return smem;

exit2:
	msm_cvp_unmap_smem(inst, smem, "unmap cpu");
exit:
	msm_cvp_smem_put_dma_buf(dma_buf);
	cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
	smem = NULL;
	return smem;
}

static int msm_cvp_unmap_user_persist_buf(struct msm_cvp_inst *inst,
				struct cvp_buf_type *buf,
				u32 pkt_type, u32 buf_idx, u32 *iova)
{
	struct msm_cvp_smem *smem = NULL;
        struct list_head *ptr;
        struct list_head *next;
        struct cvp_internal_buf *pbuf;
        struct dma_buf *dma_buf;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	dma_buf = msm_cvp_smem_get_dma_buf(buf->fd);
	if (!dma_buf)
		return -EINVAL;

	mutex_lock(&inst->persistbufs.lock);
	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		if (!ptr) {
			mutex_unlock(&inst->persistbufs.lock);
			return -EINVAL;
		}
		pbuf = list_entry(ptr, struct cvp_internal_buf, list);
		if (dma_buf == pbuf->smem->dma_buf && (pbuf->smem->flags & SMEM_PERSIST)) {
			*iova = pbuf->smem->device_addr;
			dprintk(CVP_MEM,
				"Unmap persist fd %d, dma_buf %#llx iova %#x\n",
				pbuf->fd, pbuf->smem->dma_buf, *iova);
			list_del(&pbuf->list);
			if (*iova) {
				msm_cvp_unmap_smem(inst, pbuf->smem, "unmap user persist");
				msm_cvp_smem_put_dma_buf(pbuf->smem->dma_buf);
				pbuf->smem->device_addr = 0;
			}
			cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
			pbuf->smem = NULL;
			cvp_kmem_cache_free(&cvp_driver->buf_cache, pbuf);
			mutex_unlock(&inst->persistbufs.lock);
			dma_buf_put(dma_buf);
			return 0;
		}
	}
	mutex_unlock(&inst->persistbufs.lock);
	dma_buf_put(dma_buf);

	return -EINVAL;
}

static int msm_cvp_map_user_persist_buf(struct msm_cvp_inst *inst,
				struct cvp_buf_type *buf,
				u32 pkt_type, u32 buf_idx, u32 *iova)
{
	struct msm_cvp_smem *smem = NULL;
	struct list_head *ptr;
	struct list_head *next;
	struct cvp_internal_buf *pbuf;
	struct dma_buf *dma_buf;
	int ret;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	dma_buf = msm_cvp_smem_get_dma_buf(buf->fd);
	if (!dma_buf)
		return -EINVAL;

	mutex_lock(&inst->persistbufs.lock);
	if (!inst->persistbufs.list.next) {
		mutex_unlock(&inst->persistbufs.lock);
		return -EINVAL;
	}
	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		if (!ptr)
			return -EINVAL;
		pbuf = list_entry(ptr, struct cvp_internal_buf, list);
		if (dma_buf == pbuf->smem->dma_buf) {
			pbuf->size =
				(pbuf->size >= buf->size) ?
				pbuf->size : buf->size;
			*iova = pbuf->smem->device_addr + buf->offset;
			mutex_unlock(&inst->persistbufs.lock);
			atomic_inc(&pbuf->smem->refcount);
			dma_buf_put(dma_buf);
			dprintk(CVP_MEM,
				"map persist Reuse fd %d, dma_buf %#llx\n",
				pbuf->fd, pbuf->smem->dma_buf);
			return 0;
		}
	}
	mutex_unlock(&inst->persistbufs.lock);

	dma_buf_put(dma_buf);

	pbuf = cvp_kmem_cache_zalloc(&cvp_driver->buf_cache, GFP_KERNEL);
	if (!pbuf) {
		dprintk(CVP_ERR, "%s failed to allocate kmem obj\n",
			__func__);
		return -ENOMEM;
	}

	if (is_params_pkt(pkt_type))
		smem = msm_cvp_session_get_smem(inst, buf, false, pkt_type);
	else
		smem = msm_cvp_session_get_smem(inst, buf, true, pkt_type);

	if (!smem) {
		ret = -ENOMEM;
		goto exit;
	}

	smem->pkt_type = pkt_type;
	smem->buf_idx = buf_idx;
	smem->fd = buf->fd;
	pbuf->smem = smem;
	pbuf->fd = buf->fd;
	pbuf->size = buf->size;
	pbuf->offset = buf->offset;
	pbuf->ownership = CLIENT;

	mutex_lock(&inst->persistbufs.lock);
	list_add_tail(&pbuf->list, &inst->persistbufs.list);
	mutex_unlock(&inst->persistbufs.lock);

	print_internal_buffer(CVP_MEM, "map persist", inst, pbuf);

#ifdef USE_PRESIL42
	presil42_send_map_user_persist_buffer(smem, iova, pbuf);
#endif

	*iova = smem->device_addr + buf->offset;

	return 0;

exit:
	cvp_kmem_cache_free(&cvp_driver->buf_cache, pbuf);
	return ret;
}

/* for trigger smmu fault */
static u32 frame_count;

static u32 msm_cvp_map_frame_buf(struct msm_cvp_inst *inst,
			struct cvp_buf_type *buf,
			struct msm_cvp_frame *frame,
			u32 pkt_type, u32 buf_idx)
{
	u32 iova = 0;
	struct msm_cvp_smem *smem = NULL;
	struct msm_cvp_core *core;
	struct cvp_hfi_ops *ops_tbl;
	struct iris_hfi_device *dev = NULL;
	u32 nr;
	u32 type;
	u32 ipcc_reg_base_iova;
	u32 ipcc_reg_size;

	core = cvp_driver->cvp_core;
	if (core) {
		ops_tbl = core->dev_ops;
		if (ops_tbl)
			dev = ops_tbl->hfi_device_data;
	}

	if (!dev)
		return -EINVAL;

	ipcc_reg_base_iova = dev->res->ipcc_reg_base_iova;
	ipcc_reg_size = dev->res->ipcc_reg_size;

	if (!inst || !frame) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return 0;
	}

	nr = frame->nr;
	if (nr == MAX_FRAME_BUFFER_NUMS) {
		dprintk(CVP_ERR, "%s: max frame buffer reached\n", __func__);
		return 0;
	}

	smem = msm_cvp_session_get_smem(inst, buf, false, pkt_type);
	if (!smem)
		return 0;

	smem->buf_idx = buf_idx;

	frame->bufs[nr].fd = buf->fd;
	frame->bufs[nr].smem = smem;
	frame->bufs[nr].size = buf->size;
	frame->bufs[nr].offset = buf->offset;

	print_internal_buffer(CVP_MEM, "map cpu", inst, &frame->bufs[nr]);

	frame->nr++;

	type = EVA_KMD_BUFTYPE_INPUT | EVA_KMD_BUFTYPE_OUTPUT;
	msm_cvp_cache_operations(smem, type, buf->offset, buf->size);

#ifdef USE_PRESIL42
	presil42_send_map_frame_buffer(smem, iova, buf);
#endif

	iova = smem->device_addr + buf->offset;

	if (trigger_smmu_fault) {
		frame_count++;
		if (frame_count % 200 == 0) {
			iova -= 0x1000000;
			if ((iova >= ipcc_reg_base_iova) &&
				(iova <= ipcc_reg_base_iova + ipcc_reg_size))
				iova += ipcc_reg_size * 2;
			frame_count = 0;
			trigger_smmu_fault = false;
			dprintk(CVP_ERR, "generating fault address %#x", iova);
		}
	}

	return iova;
}

static void msm_cvp_unmap_frame_buf(struct msm_cvp_inst *inst,
			struct msm_cvp_frame *frame)
{
	u32 i;
	u32 type;
	struct msm_cvp_smem *smem = NULL;
	struct cvp_internal_buf *buf;

	type = EVA_KMD_BUFTYPE_OUTPUT;

	for (i = 0; i < frame->nr; ++i) {
		buf = &frame->bufs[i];
		smem = buf->smem;
		msm_cvp_cache_operations(smem, type, buf->offset, buf->size);

#ifdef USE_PRESIL42
	presil42_unmap_frame_buf(smem, buf);
#endif

		if (smem->bitmap_index >= MAX_DMABUF_NUMS) {
			/* smem not in dmamap cache */
			if (atomic_dec_and_test(&smem->refcount)) {
				msm_cvp_unmap_smem(inst, smem, "unmap cpu");
				dma_heap_buffer_free(smem->dma_buf);
				smem->buf_idx |= 0xdead0000;
				cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
				buf->smem = NULL;
			}
		} else {
			mutex_lock(&inst->dma_cache.lock);
			if (atomic_dec_and_test(&smem->refcount)) {
				CLEAR_USE_BITMAP(smem->bitmap_index, inst);
				print_smem(CVP_MEM, "Map dereference",
					inst, smem);
				smem->buf_idx |= 0x10000000;
			}
			mutex_unlock(&inst->dma_cache.lock);
		}
	}

	cvp_kmem_cache_free(&cvp_driver->frame_cache, frame);
}

static void backup_frame_buffers(struct msm_cvp_inst *inst,
			struct msm_cvp_frame *frame)
{
	/* Save frame buffers before unmap them */
	int i = frame->nr;

	if (i == 0 || i > MAX_FRAME_BUFFER_NUMS)
		return;

	inst->last_frame.ktid = frame->ktid;
	inst->last_frame.nr = frame->nr;

	do {
		i--;
		if (frame->bufs[i].smem->bitmap_index < MAX_DMABUF_NUMS) {
			/*
			 * Frame buffer info can be found in dma_cache table,
			 * Skip saving
			 */
			inst->last_frame.nr = 0;
			return;
		}

		inst->last_frame.smem[i] = *(frame->bufs[i].smem);
	} while (i);
}

void msm_cvp_unmap_frame(struct msm_cvp_inst *inst, u64 ktid)
{
	struct msm_cvp_frame *frame = (struct msm_cvp_frame *)0xdeadbeef, *dummy1;
	bool found;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return;
	}

	ktid &= (FENCE_BIT - 1);
	dprintk(CVP_MEM, "%s: (%#x) unmap frame %llu\n",
			__func__, hash32_ptr(inst->session), ktid);

	found = false;
	mutex_lock(&inst->frames.lock);
	list_for_each_entry_safe(frame, dummy1, &inst->frames.list, list) {
		if (frame->ktid == ktid) {
			found = true;
			list_del(&frame->list);
			dprintk(CVP_CMD, "%s: "
				"pkt_type %08x sess_id %08x trans_id <> ktid %llu\n",
				__func__, frame->pkt_type,
				hash32_ptr(inst->session),
				frame->ktid);
			/* Save the previous frame mappings for debug */
			backup_frame_buffers(inst, frame);
			msm_cvp_unmap_frame_buf(inst, frame);
			break;
		}
	}
	mutex_unlock(&inst->frames.lock);

	if (!found)
		dprintk(CVP_CMD, "%s frame %llu not found!\n", __func__, ktid);
}

/*
 * Unmap persistent buffer before sending RELEASE_PERSIST_BUFFERS to FW
 * This packet is sent after SESSION_STOP. The assumption is FW/HW will
 * NOT access any of the 3 persist buffer.
 */
int msm_cvp_unmap_user_persist(struct msm_cvp_inst *inst,
			struct eva_kmd_hfi_packet *in_pkt,
			unsigned int offset, unsigned int buf_num)
{
	struct cvp_buf_type *buf;
	struct cvp_hfi_cmd_session_hdr *cmd_hdr;
	int i, ret;
	u32 iova;

	if (!offset || !buf_num)
		return 0;

	if (offset < (sizeof(struct cvp_hfi_cmd_session_hdr)/sizeof(u32))) {
		dprintk(CVP_ERR, "%s: Incorrect offset in cmd %d\n", __func__, offset);
		return -EINVAL;
	}
	cmd_hdr = (struct cvp_hfi_cmd_session_hdr *)in_pkt;
	for (i = 0; i < buf_num; i++) {
		buf = (struct cvp_buf_type *)&in_pkt->pkt_data[offset];
		offset += sizeof(*buf) >> 2;

		if (buf->fd < 0 || !buf->size)
			continue;

		ret = msm_cvp_unmap_user_persist_buf(inst, buf,
				cmd_hdr->packet_type, i, &iova);
		if (ret) {
			dprintk(CVP_ERR,
				"%s: buf %d unmap failed.\n",
				__func__, i);

			return ret;
		}
		buf->fd = iova;
	}
	return 0;
}

int msm_cvp_map_user_persist(struct msm_cvp_inst *inst,
			struct eva_kmd_hfi_packet *in_pkt,
			unsigned int offset, unsigned int buf_num)
{
	struct cvp_buf_type *buf;
	struct cvp_hfi_cmd_session_hdr *cmd_hdr;
	int i, ret;
	u32 iova;
	u64 ktid;

	if (!offset || !buf_num)
		return 0;
	if (offset < (sizeof(struct cvp_hfi_cmd_session_hdr)/sizeof(u32))) {
		dprintk(CVP_ERR, "%s: Incorrect offset in cmd %d\n", __func__, offset);
		return -EINVAL;
	}

	/*Add kernel transaction ID for persist packet*/
	ktid = atomic64_inc_return(&inst->core->kernel_trans_id);
	ktid &= (FENCE_BIT - 1);
	cmd_hdr = (struct cvp_hfi_cmd_session_hdr *)in_pkt;
	cmd_hdr->client_data.kdata = ktid;
	for (i = 0; i < buf_num; i++) {
		buf = (struct cvp_buf_type *)&in_pkt->pkt_data[offset];
		offset += sizeof(*buf) >> 2;

		if (buf->fd < 0 || !buf->size)
			continue;

		ret = msm_cvp_map_user_persist_buf(inst, buf,
				cmd_hdr->packet_type, i, &iova);
		if (ret) {
			dprintk(CVP_ERR,
				"%s: buf %d map failed.\n",
				__func__, i);

			return ret;
		}

#ifdef USE_PRESIL42
		presil42_set_buf_fd(buf, iova, "cvp_map_user_persist");
		return 0;
#endif
		buf->fd = iova;
	}
	return 0;
}

int msm_cvp_map_frame(struct msm_cvp_inst *inst,
		struct eva_kmd_hfi_packet *in_pkt,
		unsigned int offset, unsigned int buf_num)
{
	struct cvp_buf_type *buf;
	int i;
	u32 iova;
	u64 ktid;
	struct msm_cvp_frame *frame;
	struct cvp_hfi_cmd_session_hdr *cmd_hdr;
	struct msm_cvp_inst *instance = (struct  msm_cvp_inst *)0xdeadbeef;
	struct msm_cvp_core *core = NULL;

	core = cvp_driver->cvp_core;
	if (!core)
		return -EINVAL;

	/*Add kernel transaction ID for config packet*/
	ktid = atomic64_inc_return(&inst->core->kernel_trans_id);
	ktid &= (FENCE_BIT - 1);
	cmd_hdr = (struct cvp_hfi_cmd_session_hdr *)in_pkt;
	cmd_hdr->client_data.kdata = ktid;

	if (!offset || !buf_num)
		return 0;
	if (offset < (sizeof(struct cvp_hfi_cmd_session_hdr)/sizeof(u32))) {
		dprintk(CVP_ERR, "%s: Incorrect offset in cmd %d\n", __func__, offset);
		return -EINVAL;
	}


	dprintk(CVP_CMD, "%s:   "
		"pkt_type %08x sess_id %08x trans_id %u ktid %llu\n",
		__func__, cmd_hdr->packet_type,
		cmd_hdr->session_id,
		cmd_hdr->client_data.transaction_id,
		cmd_hdr->client_data.kdata & (FENCE_BIT - 1));

	frame = cvp_kmem_cache_zalloc(&cvp_driver->frame_cache, GFP_KERNEL);
	if (!frame)
		return -ENOMEM;

	frame->ktid = ktid;
	frame->nr = 0;
	frame->pkt_type = cmd_hdr->packet_type;

	for (i = 0; i < buf_num; i++) {
		buf = (struct cvp_buf_type *)&in_pkt->pkt_data[offset];
		offset += sizeof(*buf) >> 2;

		if (buf->fd < 0 || !buf->size) {
			buf->fd = 0;
			buf->size = 0;
			continue;
		}

		iova = msm_cvp_map_frame_buf(inst, buf, frame, cmd_hdr->packet_type, i);
		if (!iova) {
			dprintk(CVP_ERR,
				"%s: buf %d register failed.\n",
				__func__, i);
			dprintk(CVP_ERR, "smem_leak_count %d\n", core->smem_leak_count);
			mutex_lock(&core->lock);
			list_for_each_entry(instance, &core->instances, list) {
				msm_cvp_print_inst_bufs(instance, false);
			}
			mutex_unlock(&core->lock);
			msm_cvp_unmap_frame_buf(inst, frame);
			return -EINVAL;
		}

#ifdef USE_PRESIL42
		presil42_set_buf_fd(buf, iova, "cvp_map_frame");
#else
		buf->fd = iova;
#endif
	}

	mutex_lock(&inst->frames.lock);
	list_add_tail(&frame->list, &inst->frames.list);
	mutex_unlock(&inst->frames.lock);
	dprintk(CVP_MEM, "%s: map frame %llu\n", __func__, ktid);

	return 0;
}

int msm_cvp_session_deinit_buffers(struct msm_cvp_inst *inst)
{
	int rc = 0, i;
	struct cvp_internal_buf *cbuf, *dummy;
	struct msm_cvp_frame *frame = (struct msm_cvp_frame *)0xdeadbeef, *dummy1;
	struct msm_cvp_smem *smem;
	struct cvp_hal_session *session;
	struct eva_kmd_buffer buf;
	struct list_head *ptr = (struct list_head *)0xdead;
	struct list_head *next = (struct list_head *)0xdead;

	session = (struct cvp_hal_session *)inst->session;

	mutex_lock(&inst->frames.lock);
	list_for_each_entry_safe(frame, dummy1, &inst->frames.list, list) {
		list_del(&frame->list);
		msm_cvp_unmap_frame_buf(inst, frame);
	}
	mutex_unlock(&inst->frames.lock);

	mutex_lock(&inst->persistbufs.lock);
	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		if (!ptr)
			return -EINVAL;
		cbuf = list_entry(ptr, struct cvp_internal_buf, list);
		smem = cbuf->smem;
		if (!smem) {
			dprintk(CVP_ERR, "%s invalid persist smem\n", __func__);
			mutex_unlock(&inst->persistbufs.lock);
			return -EINVAL;
		}
		if (cbuf->ownership != DRIVER) {
			dprintk(CVP_MEM,
			"%s: %x : fd %d %pK size %d",
			"free user persistent", hash32_ptr(inst->session), cbuf->fd,
			smem->dma_buf, cbuf->size);
			list_del(&cbuf->list);
			if (smem->bitmap_index >= MAX_DMABUF_NUMS) {
				/*
				 * don't care refcount, has to remove mapping
				 * this is user persistent buffer
				 */
				if (smem->device_addr) {
					msm_cvp_unmap_smem(inst, smem,
						"unmap persist");
					msm_cvp_smem_put_dma_buf(
						cbuf->smem->dma_buf);
					smem->device_addr = 0;
				}
				cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
				cbuf->smem = NULL;
				cvp_kmem_cache_free(&cvp_driver->buf_cache, cbuf);
			} else {
				/*
				 * DMM_PARAMS and WAP_NCC_PARAMS cases
				 * Leave dma_cache cleanup to unmap
				 */
				cbuf->smem = NULL;
				cvp_kmem_cache_free(&cvp_driver->buf_cache, cbuf);
			}
		}
	}
	mutex_unlock(&inst->persistbufs.lock);

	mutex_lock(&inst->dma_cache.lock);
	for (i = 0; i < inst->dma_cache.nr; i++) {
		smem = inst->dma_cache.entries[i];
		if (atomic_read(&smem->refcount) == 0) {
			print_smem(CVP_MEM, "free", inst, smem);
		} else if (!(smem->flags & SMEM_PERSIST)) {
			print_smem(CVP_WARN, "in use", inst, smem);
		}
		msm_cvp_unmap_smem(inst, smem, "unmap cpu");
		msm_cvp_smem_put_dma_buf(smem->dma_buf);
		cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
		inst->dma_cache.entries[i] = NULL;
	}
	mutex_unlock(&inst->dma_cache.lock);

	cbuf = (struct cvp_internal_buf *)0xdeadbeef;
	mutex_lock(&inst->cvpdspbufs.lock);
	list_for_each_entry_safe(cbuf, dummy, &inst->cvpdspbufs.list, list) {
		print_internal_buffer(CVP_MEM, "remove dspbufs", inst, cbuf);
		if (cbuf->ownership == CLIENT) {
			msm_cvp_unmap_smem(inst, cbuf->smem, "unmap dsp");
			msm_cvp_smem_put_dma_buf(cbuf->smem->dma_buf);
		} else if (cbuf->ownership == DSP) {
			rc = cvp_dsp_fastrpc_unmap(inst->dsp_handle, cbuf);
			if (rc)
				dprintk(CVP_ERR,
				"%s: failed to unmap buf from DSP\n",
				__func__);

			rc = cvp_release_dsp_buffers(inst, cbuf);
			if (rc)
				dprintk(CVP_ERR,
					"%s Fail to free buffer 0x%x\n",
					__func__, rc);
		}
		list_del(&cbuf->list);
		cvp_kmem_cache_free(&cvp_driver->buf_cache, cbuf);
	}
	mutex_unlock(&inst->cvpdspbufs.lock);

	mutex_lock(&inst->cvpwnccbufs.lock);
	if (inst->cvpwnccbufs_num != 0)
		dprintk(CVP_WARN, "%s: cvpwnccbufs not empty, contains %d bufs",
			__func__, inst->cvpwnccbufs_num);
	list_for_each_entry_safe(cbuf, dummy, &inst->cvpwnccbufs.list, list) {
		print_internal_buffer(CVP_MEM, "remove wnccbufs", inst, cbuf);
		buf.fd = cbuf->fd;
		buf.reserved[0] = cbuf->ktid;

		mutex_unlock(&inst->cvpwnccbufs.lock);
		msm_cvp_unmap_buf_wncc(inst, &buf);
		mutex_lock(&inst->cvpwnccbufs.lock);
	}
	mutex_unlock(&inst->cvpwnccbufs.lock);

	return rc;
}

void msm_cvp_populate_dsp_buf_info(struct cvp_internal_buf *buf,
								struct cvp_hal_session *session,
								u32 session_id,
								struct msm_cvp_core *core)
{
	struct cvp_hfi_ops *dev_ops = (struct cvp_hfi_ops *) core->dev_ops;
	struct iris_hfi_device *cvp_device = (struct iris_hfi_device *) dev_ops->hfi_device_data;
	struct cvp_iface_q_info dsp_debugQ_info = cvp_device->dsp_iface_queues[DEBUG_Q];
	struct cvp_dsp_trace_buf *trace_buf;
	struct cvp_dsp_trace *dsp_debug_trace;

	dsp_debug_trace = (struct cvp_dsp_trace *) dsp_debugQ_info.q_array.align_virtual_addr;

	if (!dsp_debug_trace) {
		dprintk(CVP_ERR, "dsp trace is NULL\n");
		return;
	}
	for (int session_idx = 0; session_idx < EVA_TRACE_MAX_SESSION_NUM; session_idx++) {
		if (dsp_debug_trace->sessions[session_idx].session_id == session_id) {
			u32 buf_cnt = dsp_debug_trace->sessions[session_idx].buf_cnt;

			for (int buf_idx = 0; buf_idx < buf_cnt; buf_idx++) {
				trace_buf = &dsp_debug_trace->sessions[session_idx].buf[buf_idx];
				if (buf->smem->device_addr == trace_buf->iova) {
					buf->smem->buf_idx = trace_buf->buf_idx;
					buf->smem->pkt_type = trace_buf->pkt_type;
					buf->smem->fd = trace_buf->fd;
					return;
				}
			}
		}
	}
}

#define MAX_NUM_FRAMES_DUMP 4
void msm_cvp_print_inst_bufs(struct msm_cvp_inst *inst, bool log)
{
	struct cvp_internal_buf *buf = (struct cvp_internal_buf *)0xdeadbeef;
	struct msm_cvp_frame *frame = (struct msm_cvp_frame *)0xdeadbeef;
	struct msm_cvp_core *core;
	struct inst_snapshot *snap = NULL;
	int i = 0, c = 0;

	// DSP trace related variables
	struct cvp_hal_session *session;
	u32 session_id;

	session = (struct cvp_hal_session *)inst->session;
	session_id = hash32_ptr(session);

	core = cvp_driver->cvp_core;
	if (log && core->log.snapshot_index < 16) {
		snap = &core->log.snapshot[core->log.snapshot_index];
		snap->session = inst->session;
		core->log.snapshot_index++;
	}

	if (!inst) {
		dprintk(CVP_ERR, "%s - invalid param %pK\n",
			__func__, inst);
		return;
	}

	dprintk(CVP_ERR,
			"---Buffer details for inst: %pK %s of type: %d---\n",
			inst, inst->proc_name, inst->session_type);

	dprintk(CVP_ERR, "dma_cache entries %d\n", inst->dma_cache.nr);
	mutex_lock(&inst->dma_cache.lock);
	if (inst->dma_cache.nr <= MAX_DMABUF_NUMS)
		for (i = 0; i < inst->dma_cache.nr; i++)
			_log_smem(snap, inst, inst->dma_cache.entries[i], log);
	mutex_unlock(&inst->dma_cache.lock);

	i = 0;
	dprintk(CVP_ERR, "frame buffer list\n");
	mutex_lock(&inst->frames.lock);
	list_for_each_entry(frame, &inst->frames.list, list) {
		i++;
		if (i <= MAX_NUM_FRAMES_DUMP) {
			dprintk(CVP_ERR, "frame no %d tid %llx bufs\n",
					i, frame->ktid);
			for (c = 0; c < frame->nr; c++)
				_log_smem(snap, inst, frame->bufs[c].smem,
						log);
		}
	}
	if (i > MAX_NUM_FRAMES_DUMP)
		dprintk(CVP_ERR, "Skipped %d frames' buffers\n",
				(i - MAX_NUM_FRAMES_DUMP));
	mutex_unlock(&inst->frames.lock);

	mutex_lock(&inst->cvpdspbufs.lock);
	dprintk(CVP_ERR, "dsp buffer list:\n");
	list_for_each_entry(buf, &inst->cvpdspbufs.list, list) {
		// Populate DSP buffer info from debug queue to kernel instance
		msm_cvp_populate_dsp_buf_info(buf, session, session_id, core);
		// Log print buffer info
		_log_buf(snap, SMEM_CDSP, inst, buf, log);
	}
	mutex_unlock(&inst->cvpdspbufs.lock);

	mutex_lock(&inst->cvpwnccbufs.lock);
	dprintk(CVP_ERR, "wncc buffer list:\n");
	list_for_each_entry(buf, &inst->cvpwnccbufs.list, list)
		print_cvp_buffer(CVP_ERR, "bufdump", inst, buf);
	mutex_unlock(&inst->cvpwnccbufs.lock);

	mutex_lock(&inst->persistbufs.lock);
	dprintk(CVP_ERR, "persist buffer list:\n");
	list_for_each_entry(buf, &inst->persistbufs.list, list)
		_log_buf(snap, SMEM_PERSIST, inst, buf, log);
	mutex_unlock(&inst->persistbufs.lock);

	dprintk(CVP_ERR, "last frame ktid %llx\n", inst->last_frame.ktid);
	for (i = 0; i < inst->last_frame.nr; i++)
		_log_smem(snap, inst, &inst->last_frame.smem[i], log);

	dprintk(CVP_ERR, "unmapped wncc bufs\n");
	for (i = 0; i < inst->unused_wncc_bufs.nr; i++)
		_log_smem(snap, inst, &inst->unused_wncc_bufs.smem[i], log);

	dprintk(CVP_ERR, "unmapped dsp bufs\n");
	for (i = 0; i < inst->unused_dsp_bufs.nr; i++)
		_log_smem(snap, inst, &inst->unused_dsp_bufs.smem[i], log);
}

struct cvp_internal_buf *cvp_allocate_arp_bufs(struct msm_cvp_inst *inst,
			u32 buffer_size)
{
	struct cvp_internal_buf *buf;
	struct msm_cvp_list *buf_list;
	u32 smem_flags = SMEM_UNCACHED;
	int rc = 0;

	if (!inst) {
		dprintk(CVP_ERR, "%s Invalid input\n", __func__);
		return NULL;
	}

	buf_list = &inst->persistbufs;

	if (!buffer_size)
		return NULL;

#ifdef USE_PRESIL42
	presil42_set_smem_flags(smem_flags);
#endif
	/* If PERSIST buffer requires secure mapping, uncomment
	 * below flags setting
	 * smem_flags |= SMEM_SECURE | SMEM_NON_PIXEL;
	 */

	buf = cvp_kmem_cache_zalloc(&cvp_driver->buf_cache, GFP_KERNEL);
	if (!buf) {
		dprintk(CVP_ERR, "%s Out of memory\n", __func__);
		goto fail_kzalloc;
	}

	buf->smem = cvp_kmem_cache_zalloc(&cvp_driver->smem_cache, GFP_KERNEL);
	if (!buf->smem) {
		dprintk(CVP_ERR, "%s Out of memory\n", __func__);
		goto err_no_smem;
	}

	buf->smem->flags = smem_flags;
	rc = msm_cvp_smem_alloc(buffer_size, 1, 0, /* 0: no mapping in kernel space */
		&(inst->core->resources), buf->smem);
	if (rc) {
		dprintk(CVP_ERR, "Failed to allocate ARP memory\n");
		goto err_no_mem;
	}

	buf->smem->pkt_type = buf->smem->buf_idx = 0;
	atomic_inc(&buf->smem->refcount);
	buf->size = buf->smem->size;
	buf->type = HFI_BUFFER_INTERNAL_PERSIST_1;
	buf->ownership = DRIVER;

	mutex_lock(&buf_list->lock);
	list_add_tail(&buf->list, &buf_list->list);
	mutex_unlock(&buf_list->lock);
	return buf;

err_no_mem:
	cvp_kmem_cache_free(&cvp_driver->smem_cache, buf->smem);
err_no_smem:
	cvp_kmem_cache_free(&cvp_driver->buf_cache, buf);
fail_kzalloc:
	return NULL;
}

int cvp_release_arp_buffers(struct msm_cvp_inst *inst)
{
	struct msm_cvp_smem *smem;
	struct list_head *ptr = (struct list_head *)0xdead;
	struct list_head *next = (struct list_head *)0xdead;
	struct cvp_internal_buf *buf;
	int rc = 0;
	struct msm_cvp_core *core;
	struct cvp_hfi_ops *ops_tbl;

	if (!inst) {
		dprintk(CVP_ERR, "Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}

	core = inst->core;
	if (!core) {
		dprintk(CVP_ERR, "Invalid core pointer = %pK\n", core);
		return -EINVAL;
	}
	ops_tbl = core->dev_ops;
	if (!ops_tbl) {
		dprintk(CVP_ERR, "Invalid device pointer = %pK\n", ops_tbl);
		return -EINVAL;
	}

	dprintk(CVP_MEM, "release persist buffer!\n");

	mutex_lock(&inst->persistbufs.lock);
	/* Workaround for FW: release buffer means release all */
	if (inst->state > MSM_CVP_CORE_INIT_DONE && inst->state <= MSM_CVP_CLOSE_DONE) {
		rc = call_hfi_op(ops_tbl, session_release_buffers,
				(void *)inst->session);
		if (!rc) {
			mutex_unlock(&inst->persistbufs.lock);
			rc = wait_for_sess_signal_receipt(inst,
				HAL_SESSION_RELEASE_BUFFER_DONE);
			if (rc)
				dprintk(CVP_WARN,
				"%s: wait release_arp signal failed, rc %d\n",
				__func__, rc);
			mutex_lock(&inst->persistbufs.lock);
		} else {
			dprintk_rl(CVP_WARN, "Fail to send Rel prst buf\n");
		}
	}

	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		if (!ptr)
			return -EINVAL;
		buf = list_entry(ptr, struct cvp_internal_buf, list);
		smem = buf->smem;
		if (!smem) {
			dprintk(CVP_ERR, "%s invalid smem\n", __func__);
			mutex_unlock(&inst->persistbufs.lock);
			return -EINVAL;
		}

		if (buf->ownership == DRIVER) {
			dprintk(CVP_MEM,
			"%s: %x : fd %d %pK size %d",
			"free arp", hash32_ptr(inst->session), buf->fd,
			smem->dma_buf, buf->size);
			list_del(&buf->list);
			atomic_dec(&smem->refcount);
			msm_cvp_smem_free(smem);
			cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
			buf->smem = NULL;
			cvp_kmem_cache_free(&cvp_driver->buf_cache, buf);
		}
	}
	mutex_unlock(&inst->persistbufs.lock);
	return rc;
}

int cvp_allocate_dsp_bufs(struct msm_cvp_inst *inst,
			struct cvp_internal_buf *buf,
			u32 buffer_size,
			u32 secure_type)
{
	u32 smem_flags = SMEM_UNCACHED;
	int rc = 0;

	if (!inst) {
		dprintk(CVP_ERR, "%s Invalid input\n", __func__);
		return -EINVAL;
	}

	if (!buf)
		return -EINVAL;

	if (!buffer_size)
		return -EINVAL;

	switch (secure_type) {
	case 0:
		break;
	case 1:
		smem_flags |= SMEM_SECURE | SMEM_PIXEL;
		break;
	case 2:
		smem_flags |= SMEM_SECURE | SMEM_NON_PIXEL;
		break;
	default:
		dprintk(CVP_ERR, "%s Invalid secure_type %d\n",
			__func__, secure_type);
		return -EINVAL;
	}

	dprintk(CVP_MEM, "%s smem_flags 0x%x\n", __func__, smem_flags);
	buf->smem = cvp_kmem_cache_zalloc(&cvp_driver->smem_cache, GFP_KERNEL);
	if (!buf->smem) {
		dprintk(CVP_ERR, "%s Out of memory\n", __func__);
		goto fail_kzalloc_smem_cache;
	}

	buf->smem->flags = smem_flags;
	rc = msm_cvp_smem_alloc(buffer_size, 1, 0,
			&(inst->core->resources), buf->smem);
	if (rc) {
		dprintk(CVP_ERR, "Failed to allocate DSP buf\n");
		goto err_no_mem;
	}
	buf->smem->pkt_type = buf->smem->buf_idx = 0;
	atomic_inc(&buf->smem->refcount);

	dprintk(CVP_MEM, "%s dma_buf %pK\n", __func__, buf->smem->dma_buf);

	buf->size = buf->smem->size;
	buf->type = HFI_BUFFER_INTERNAL_PERSIST_1;
	buf->ownership = DSP;

	return rc;

err_no_mem:
	cvp_kmem_cache_free(&cvp_driver->smem_cache, buf->smem);
fail_kzalloc_smem_cache:
	return rc;
}

int cvp_release_dsp_buffers(struct msm_cvp_inst *inst,
			struct cvp_internal_buf *buf)
{
	struct msm_cvp_smem *smem;
	int rc = 0;

	if (!inst) {
		dprintk(CVP_ERR, "Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}

	if (!buf) {
		dprintk(CVP_ERR, "Invalid buffer pointer = %pK\n", inst);
		return -EINVAL;
	}

	smem = buf->smem;
	if (!smem) {
		dprintk(CVP_ERR, "%s invalid smem\n", __func__);
		return -EINVAL;
	}

	if (buf->ownership == DSP) {
		dprintk(CVP_MEM,
			"%s: %x : fd %x %s size %d",
			__func__, hash32_ptr(inst->session), buf->fd,
			smem->dma_buf->name, buf->size);
		if (atomic_dec_and_test(&smem->refcount)) {
			msm_cvp_smem_free(smem);
			cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
		}
	} else {
		dprintk(CVP_ERR,
			"%s: wrong owner %d %x : fd %x %s size %d",
			__func__, buf->ownership, hash32_ptr(inst->session),
			buf->fd, smem->dma_buf->name, buf->size);
	}

	return rc;
}

int msm_cvp_register_buffer(struct msm_cvp_inst *inst,
		struct eva_kmd_buffer *buf)
{
	struct cvp_hfi_ops *ops_tbl;
	struct cvp_hal_session *session;
	struct msm_cvp_inst *s;
	int rc = 0;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	session = (struct cvp_hal_session *)inst->session;
	if (!session) {
		dprintk(CVP_ERR, "%s: invalid session\n", __func__);
		rc = -EINVAL;
		goto exit;
	}
	ops_tbl = inst->core->dev_ops;
	print_client_buffer(CVP_HFI, "register", inst, buf);

	if (buf->index)
		rc = msm_cvp_map_buf_dsp(inst, buf);
	else
		rc = msm_cvp_map_buf_wncc(inst, buf);
	dprintk(CVP_DSP, "%s: fd %d, iova 0x%x\n", __func__,
			buf->fd, buf->reserved[0]);
exit:
	cvp_put_inst(s);
	return rc;
}

int msm_cvp_unregister_buffer(struct msm_cvp_inst *inst,
		struct eva_kmd_buffer *buf)
{
	struct msm_cvp_inst *s;
	int rc = 0;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	print_client_buffer(CVP_HFI, "unregister", inst, buf);

	if (buf->index)
		rc = msm_cvp_unmap_buf_dsp(inst, buf);
	else
		rc = msm_cvp_unmap_buf_wncc(inst, buf);
	cvp_put_inst(s);
	return rc;
}

int msm_cvp_dma_buf_vmap(struct dma_buf *dmabuf, struct cvp_dma_buf_vmap *vmap)
{
	int ret = 0;

	#if (KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE)
		ret = dma_buf_vmap_unlocked(dmabuf, &vmap->map);
		vmap->vaddr = vmap->map.vaddr;
	#elif (KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE)
		ret = dma_buf_vmap(dmabuf, &vmap->map);
		vmap->vaddr = vmap->map.vaddr;
	#else
		vmap->vaddr = dma_buf_vmap(dmabuf);
		if (!vmap->vaddr)
			ret = -EINVAL;
	#endif

	return ret;
}

void msm_cvp_dma_buf_vunmap(struct dma_buf *dmabuf, struct cvp_dma_buf_vmap *vmap)
{
	#if (KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE)
		dma_buf_vunmap_unlocked(dmabuf, &vmap->map);
	#elif (KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE)
		dma_buf_vunmap(dmabuf, &vmap->map);
	#else
		dma_buf_vunmap(dmabuf, vmap->vaddr);
	#endif
}

