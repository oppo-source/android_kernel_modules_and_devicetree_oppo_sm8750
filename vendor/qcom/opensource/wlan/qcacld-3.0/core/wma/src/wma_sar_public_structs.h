/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
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

#ifndef __WMA_SAR_PUBLIC_STRUCTS_H
#define __WMA_SAR_PUBLIC_STRUCTS_H

struct sar_limit_event;

enum sar_version {
	SAR_VERSION_1,
	SAR_VERSION_2,
	SAR_VERSION_3,
	SAR_VERSION_4,
	SAR_VERSION_5,
	SAR_VERSION_6,
};

/**
 * enum sar_flag - This enum describes whether CTL grouping is enabled or
 *		   disabled in the firmware.
 *
 * @SAR_FLAG_NONE: None/Invalid.
 * @SAR_SET_CTL_GROUPING_DISABLE: CTL grouping is disabled in firmware.
 * @SAR_DBS_WITH_BT_DISABLE: CTL grouping is enabled in firmware.
 * @SAR_FLAG_MAX: SAR flag max limit.
 */
enum sar_flag {
	SAR_FLAG_NONE = 0,
	SAR_SET_CTL_GROUPING_DISABLE,
	SAR_DBS_WITH_BT_DISABLE,
	SAR_FLAG_MAX,
};

/**
 * typedef wma_sar_cb() - SAR callback function
 * @context: Opaque context provided by caller in the original request
 * @event: SAR limits event
 */
typedef void (*wma_sar_cb)(void *context, struct sar_limit_event *event);

#endif /* __WMA_SAR_PUBLIC_STRUCTS_H */
