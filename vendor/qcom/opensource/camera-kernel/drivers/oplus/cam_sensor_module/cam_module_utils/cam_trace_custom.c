// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

/* Instantiate tracepoints */
#include <linux/types.h>
#include <asm/string.h>
#include "cam_trace_custom.h"

pid_t camera_provider_pid;


EXPORT_SYMBOL(__traceiter_cam_tracing_mark_write);

EXPORT_SYMBOL(__tracepoint_cam_tracing_mark_write);

const char* GetFileName(const char* pFilePath)
{
	const char* pFileName = strrchr(pFilePath, '/');

	if (NULL != pFileName)
	{
		// StrRChr will return a pointer to the /, advance one to the filename
		pFileName += 1;
	}
	else
	{
		pFileName = pFilePath;
	}

	return pFileName;
}
EXPORT_SYMBOL(GetFileName);

void set_camera_provider_pid(pid_t pid)
{
	camera_provider_pid = pid;
}

pid_t get_camera_provider_pid(void)
{
	return camera_provider_pid;
}
EXPORT_SYMBOL(get_camera_provider_pid);

