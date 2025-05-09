/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#ifndef _TOUCHPANEL_EXCEPTION_
#define _TOUCHPANEL_EXCEPTION_

typedef enum {
	EXCEP_HARDWARE = 0,
	EXCEP_GESTURE,
	EXCEP_GESTURE_READ,
	EXCEP_FINGERPRINT,
	EXCEP_FACE_DETECT,
	EXCEP_REPORT,
	EXCEP_PROBE,
	EXCEP_RESUME,
	EXCEP_SUSPEND,
	EXCEP_TEST_AUTO,
	EXCEP_TEST_BLACKSCREEN,
	EXCEP_BUS,
	EXCEP_ALLOC,
	EXCEP_FW_UPDATE,
	EXCEP_GRIP,
	EXCEP_IRQ,
	EXCEP_BUS_READY,
} tp_excep_type;

#define FP_EVENT_COST_TIME_OVER_10MS 10000
#define FP_EVENT_COST_TIME_OVER_18MS 18000
#define FP_EVENT_COST_TIME_OVER_26MS 26000

int tp_exception_report(void *tp_exception_data, tp_excep_type excep_tpye, void *summary, unsigned int summary_size);

#endif /*_TOUCHPANEL_EXCEPTION_*/
