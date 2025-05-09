/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_hdd_sysfs_wowl_add_ptrn.c
 *
 * implementation for creating sysfs file wowl_add_ptrn
 */

#include <wlan_hdd_includes.h>
#include <wlan_hdd_sysfs.h>
#include "osif_vdev_sync.h"
#include "wlan_hdd_wowl.h"
#include <wlan_hdd_sysfs_wowl_add_ptrn.h>

static ssize_t
__hdd_sysfs_wowl_add_ptrn_store(struct net_device *net_dev,
				char const *buf,
				size_t count)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct hdd_context *hdd_ctx;
	char *buf_local = NULL;
	int ret;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	if (count > MAX_CMD_INPUT)
		return -EINVAL;

	buf_local = (char *)qdf_mem_malloc(sizeof(char) * count);
	if (!buf_local)
		return -EINVAL;

	strscpy(buf_local, buf, count);

	buf_local[count - 1] = '\0';

	hdd_debug("wowl_add_ptrn: count %zu buf_local:(%s)",
		  count, buf_local);

	if (!hdd_add_wowl_ptrn(adapter, buf_local)) {
		hdd_err_rl("Failed to add wowl ptrn");
		qdf_mem_free(buf_local);
		return -EINVAL;
	}

	qdf_mem_free(buf_local);
	return count;
}

static ssize_t
hdd_sysfs_wowl_add_ptrn_store(struct device *dev,
			      struct device_attribute *attr,
			      char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __hdd_sysfs_wowl_add_ptrn_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static DEVICE_ATTR(wowl_add_ptrn, 0220,
		   NULL, hdd_sysfs_wowl_add_ptrn_store);

int hdd_sysfs_wowl_add_ptrn_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev,
				   &dev_attr_wowl_add_ptrn);
	if (error)
		hdd_err("could not create wowl_add_ptrn sysfs file");

	return error;
}

void hdd_sysfs_wowl_add_ptrn_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_wowl_add_ptrn);
}
