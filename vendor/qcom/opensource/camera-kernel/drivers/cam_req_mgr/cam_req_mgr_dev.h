/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_REQ_MGR_DEV_H_
#define _CAM_REQ_MGR_DEV_H_

#include "media/cam_req_mgr.h"
/**
 * struct cam_req_mgr_device - a camera request manager device
 *
 * @video: pointer to struct video device.
 * @v4l2_dev: pointer to struct v4l2 device.
 * @count: number of subdevices registered.
 * @dev_lock: lock for the subdevice count.
 * @state: state of the root device.
 * @open_cnt: open count of subdev
 * @v4l2_sub_ids: bits representing v4l2 event ids subscribed or not
 * @cam_lock: per file handle lock
 * @cam_eventq: event queue
 * @cam_eventq_lock: lock for event queue
 * @shutdown_state: shutdown state
 */
struct cam_req_mgr_device {
	struct video_device *video;
	struct v4l2_device *v4l2_dev;
	int count;
	struct mutex dev_lock;
	bool state;
	int32_t open_cnt;
	uint32_t v4l2_sub_ids;
	struct mutex cam_lock;
	struct v4l2_fh *cam_eventq;
	spinlock_t cam_eventq_lock;
	bool shutdown_state;
};

#define CAM_REQ_MGR_GET_PAYLOAD_PTR(ev, type)        \
	(type *)((char *)ev.u.data)

int cam_req_mgr_notify_message(struct cam_req_mgr_message *msg,
	uint32_t id,
	uint32_t type);

/**
 * @brief : API to register REQ_MGR to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_req_mgr_init(void);

/**
 * @brief : API to remove REQ_MGR from platform framework.
 */
void cam_req_mgr_exit(void);

/**
 * @brief : API to get V4L2 eventIDs subscribed by UMD.
 */
uint32_t cam_req_mgr_get_id_subscribed(void);

/**
 * @brief : API to record bind latency for each driver.
 */
void cam_record_bind_latency(const char *driver_name, unsigned long time_in_usec);
#endif /* _CAM_REQ_MGR_DEV_H_ */
