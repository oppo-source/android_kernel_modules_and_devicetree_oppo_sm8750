/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Oplus. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/kprobes.h>
#include <../../../kernel/sched/walt/walt.h>
#include <linux/sched/walt.h>

#include <linux/kernel.h>
#include <linux/pm_qos.h>
#include <linux/cpumask.h>
#include <linux/string_helpers.h>

#include <trace/hooks/sched.h>
#include <trace/hooks/power.h>

#include "close_loop.h"

static int cl_aware_glthread;
static int cl_glthread_usage;
static int cl_flutter_usage;

static int cl_aware_multi_enq = 1;
static int cl_aware_multi_enq_ts = 100000000; /* 100 ms */
static s64 cl_aware_multi_enq_ns;
static int cl_main_enq;
static int cl_split_usage;
static int cl_float_usage;

static int cl_debug;
static int cl_reset_on_vsync;
static int cl_update_on_change = 1;
static s64 cl_chk_margin_ns;

/* active state statistics */
static s64 active_acc_ts_ns;
static s64 active_acc_update_ts_ns;
static s64 active_ts_ns;
static s64 active_chk_duration_ts_ns;
static s64 active_acc_frame_cnt;
static s64 active_acc_frame_duration;
static s64 active_acc_frame_prev_cnt;
static s64 active_acc_frame_prev_duration;
static s64 td_acc_frame_cnt;
static s64 td_acc_frame_duration;
static s64 td_acc_frame_prev_cnt;
static s64 td_acc_frame_prev_duration;

/* TimerDispatch */
static s64 cl_td_prev_ns;
static s64 cl_td_next_ns;
static s64 cl_td_period_ns;

/* close loop condition */
static int cl_frame_margin = 20;

/* feature active check */
static int cl_active;

/* feature switch */
int cl_enable;
EXPORT_SYMBOL_GPL(cl_enable);

static struct cl_accumulate cl_acc;

static int cl_aware_boost = 1;
static s64 cl_aware_boost_ts;
static int cl_aware_boost_hyst = 100000000; /* 100ms */

static int cl_aware_block = 1;
static s64 cl_aware_block_ts;
static int cl_aware_block_hyst = 10; /* 10s */

static int cl_aware_ed_task = 1;
static int cl_aware_long_period = 1;
static int cl_aware_long_period_ts = 24999999; /* 24.9 ms (~1.5 frame)*/

#define CL_USAGE_HIST_SIZE 8
static int cl_usage_hist[CL_USAGE_HIST_SIZE];
static int cl_usage_index;
static int cl_usage_avg;
static int cl_usage_sum;
static int cl_usage_curr;
static int cl_usage_hist_dyn = 2; /* short window has faster reaction */
static int cl_usage_hist_update_cnt;

static int cl_aware_usage = 1;
static int cl_aware_usage_dyn_limit = 50;
static int cl_aware_usage_hard_limit = 90;

static int cl_aware_camera = 1;
static int cl_aware_camera_usage;

static int cl_simple_weight[8];
static int cl_boost_weight = 80;
static int cl_adaptive_weight = 1;
static s64 cl_adaptive_weight_ratio = 100;
static int cl_adaptive_weight_min;
static int cl_adaptive_weight_max = 100;
static int amu_enable;
static int cl_amu_update_min_duration;

enum amu_counters {
	SYS_AMU_CONST_CYC = 0,
	SYS_AMU_CORE_CYC  = 1,
	SYS_AMU_INST_RET  = 2,
	SYS_AMU_STALL_MEM = 3,
	SYS_AMU_MAX       = 4,
};

struct amu_data {
	u64 val[SYS_AMU_MAX];
};

static DEFINE_PER_CPU(struct amu_data, amu_cntr);
static DEFINE_PER_CPU(struct amu_data, amu_prev_cntr);
static DEFINE_PER_CPU(struct amu_data, amu_delta);

static DEFINE_PER_CPU(u64, amu_update_delta_time);
static DEFINE_PER_CPU(u64, amu_last_update_time);

#define MAX_CPU_CNT (8)

struct amu_cl_data_t {
	u64 cyc[MAX_CPU_CNT][SYS_AMU_MAX];
} cl_amu_data;

static struct amu_cl_data_t cl_amu_frame_prev;
static struct amu_cl_data_t cl_amu_frame_curr;

static int cl_const_cyc_factor = 7; /* close to max capacity */

static int cl_default_usage = 200;
static int cl_usage_only_count_const_cyc = 1;

static int cl_tp_enable = 1;

static char *cl_reason_msg[CL_REASON_MAX] = {
	"ACTIVE",
	"FRAME_MARGIN_BREAK",
	"RENDER_DELAY_BREAK",
	"VSYNC_RESET_BREAK",
	"AWARE_BOOST_BREAK",
	"NOT_ENABLE",
	"AWARE_USAGE_BREAK",
	"MULTI_ENQ_BREAK",
	"ED_TASK_BREAK",
	"LONG_PERIOD_BREAK",
	"GLTHREAD_BREAK",
	"CAMERA_BREAK",
	"BLOCK_BREAK",
};

static char *cl_tp_msg[CL_TP_TYPE_MAX] = {
	"cl_enable",
	"cl_active",
	"cl_active_reason",
	"cl_util_orig",
	"cl_util_result",
	"cl_util_delta",
	"cl_cpufreq_changed",
	"cl_td_mark",
	"cl_util_weight_ratio",
	"cl_td_period",
	"cl_tp_usage",
	"cl_tp_usage_avg",
	"cl_tp_multi_split",
	"cl_tp_multi_float",
	"cl_tp_glthread",
	"cl_tp_flutter",
};

static inline void active_reason_acc(int idx, int val, int lv)
{
	if (idx == CL_TP_ACTIVE_REASON) {
		(cl_acc.active_reason[val][CL_ACC_CURR])++;
	}
}

static noinline int _tracing_mark_write(const char * buf)
{
	trace_printk(buf);
	return 0;
}

void cl_tp_int(int idx, int val, int lv)
{
	if (cl_tp_enable >= lv) {
		char buf[256];

		snprintf(buf, sizeof(buf), "C|99999|%s|%d\n", cl_tp_msg[idx], val);
		_tracing_mark_write(buf);
	}
	active_reason_acc(idx, val, lv);
}

void cl_tp_ll(int idx, s64 val, int lv)
{
	if (cl_tp_enable >= lv) {
		char buf[256];

		snprintf(buf, sizeof(buf), "C|99999|%s|%lld\n", cl_tp_msg[idx], val);
		_tracing_mark_write(buf);
	}
}

void cl_tp_cpu_int(int idx, int cpu, int val, int lv)
{
	if (cl_tp_enable >= lv) {
		char buf[256];

		snprintf(buf, sizeof(buf), "C|99999|%s_cpu%d|%d\n", cl_tp_msg[idx], cpu, val);
		_tracing_mark_write(buf);
	}
}

void cl_tp_cpu_ul(int idx, int cpu, unsigned long val, int lv)
{
	if (cl_tp_enable >= lv) {
		char buf[256];

		snprintf(buf, sizeof(buf), "C|99999|%s_cpu%d|%lu\n", cl_tp_msg[idx], cpu, val);
		_tracing_mark_write(buf);
	}
}

void cl_tp_cpu_ull(int idx, int cpu, u64 val, int lv)
{
	if (cl_tp_enable >= lv) {
		char buf[256];

		snprintf(buf, sizeof(buf), "C|99999|%s_cpu%d|%lld\n", cl_tp_msg[idx], cpu, val);
		_tracing_mark_write(buf);
	}
}

static void cl_trig_cpufreq_update(int cpu)
{
	if (cl_update_on_change) {
		if (cl_debug)
			pr_err("%s: cpu %d update\n", __func__, cpu);
		walt_trig_cpufreq_update(cpu);
	}
}

static void cl_update_active(int val, s64 now, int reason)
{
	s64 acc_delta;

	if (cl_active) {
		acc_delta = now - active_acc_update_ts_ns;
		if (acc_delta > 0)
			active_acc_ts_ns += acc_delta;
	}
	active_acc_update_ts_ns = now;
	cl_active = val;
	cl_tp_int(CL_TP_ACTIVE, cl_active, CL_TP_CRIT);
	cl_tp_int(CL_TP_ACTIVE_REASON, reason, CL_TP_CRIT);
}


static inline bool cl_usage_chk(void)
{
	if (cl_usage_curr - cl_usage_avg > cl_aware_usage_dyn_limit)
		return false;

	if (cl_usage_curr > cl_aware_usage_hard_limit)
		return false;

	if (cl_usage_avg > cl_aware_usage_hard_limit)
		return false;

	return true;
}

static unsigned long weighted_util(int cpu, unsigned long orig)
{
	int w = cl_simple_weight[cpu];

	if (cl_adaptive_weight) {
		if (cl_adaptive_weight_ratio > 0) {
			w = max(cl_adaptive_weight_min, (int) cl_adaptive_weight_ratio);
			w = min(w, cl_adaptive_weight_max);
		}
	}

	orig = orig * w / 100;
	return orig;
}

unsigned long cl_util(int cpu, unsigned long orig, bool ed_active)
{
	unsigned long result = 0;
	int delta = 0;

	if (!cl_enable)
		return orig;

	if (cl_aware_ed_task && cl_active && ed_active)
		cl_update_active(0, ktime_get_ns(), CL_REASON_ED_TASK_BREAK);

	result = cl_active ? weighted_util(cpu, orig) : orig;
	delta = orig - result;
	cl_tp_cpu_ul(CL_TP_UTIL_ORIG, cpu, orig, CL_TP_INFO);
	cl_tp_cpu_ul(CL_TP_UTIL_RESULT, cpu, result, CL_TP_INFO);
	cl_tp_cpu_int(CL_TP_UTIL_DELTA, cpu, delta, CL_TP_INFO);
	return result;
}
EXPORT_SYMBOL_GPL(cl_util);

unsigned long cl_boost_util(int cpu, unsigned long orig, bool ed_active)
{
	unsigned long result = 0;

	if (!cl_enable)
		return orig;

	if (cl_aware_ed_task && cl_active && ed_active)
		cl_update_active(0, ktime_get_ns(), CL_REASON_ED_TASK_BREAK);

	result = cl_active ? (orig * cl_boost_weight / 100) : orig;

	if (cl_debug)
		pr_err("%s: cpu %d orig %lu scaled %lu\n", __func__, cpu, orig, result);

	return result;
}
EXPORT_SYMBOL_GPL(cl_boost_util);

static int proc_active_show(struct seq_file *m, void *v)
{
	int i;
	s64 acc_cur, total = 0;
	s64 reasons[CL_REASON_MAX] = {0};

	s64 now = ktime_get_ns();
	s64 active_delta = active_acc_ts_ns - active_ts_ns;
	s64 active_full = now - active_chk_duration_ts_ns;

	s64 frame_cnt_delta = active_acc_frame_cnt - active_acc_frame_prev_cnt;
	s64 frame_duration_delta = active_acc_frame_duration - active_acc_frame_prev_duration;

	s64 td_frame_cnt_delta = td_acc_frame_cnt - td_acc_frame_prev_cnt;
	s64 td_frame_duration_delta = td_acc_frame_duration - td_acc_frame_prev_duration;

	active_acc_frame_prev_cnt = active_acc_frame_cnt;
	active_acc_frame_prev_duration = active_acc_frame_duration;

	td_acc_frame_prev_cnt = td_acc_frame_cnt;
	td_acc_frame_prev_duration = td_acc_frame_duration;

	seq_puts(m, "active,full,%\n");
	seq_printf(m, "%lld,%lld,%lld\n",
		active_delta, active_full, active_delta * 100 / active_full);
	active_chk_duration_ts_ns = now;
	active_ts_ns = active_acc_ts_ns;

	seq_printf(m, "reason,count,pct\n");
	for (i = 0; i < CL_REASON_MAX; i++) {
		acc_cur = cl_acc.active_reason[i][CL_ACC_CURR] - cl_acc.active_reason[i][CL_ACC_PREV];
		cl_acc.active_reason[i][CL_ACC_PREV] = cl_acc.active_reason[i][CL_ACC_CURR];
		reasons[i] = acc_cur;
		total += acc_cur;
	}

	for (i = 0; i < CL_REASON_MAX; i++) {
		if (total)
			seq_printf(m, "%s,%lld,%lld\n", cl_reason_msg[i], reasons[i], reasons[i] * 100 / total);
		else
			seq_printf(m, "%s,0,0\n", cl_reason_msg[i]);
	}

	seq_printf(m, "frame_cnt,avg_frame_duration\n");
	if (frame_cnt_delta > 0)
		seq_printf(m, "%lld,%lld\n", frame_cnt_delta, frame_duration_delta / frame_cnt_delta);
	else
		seq_puts(m, "0, 0\n");

	seq_printf(m, "td_frame_cnt,td_avg_frame_duration\n");
	if (td_frame_cnt_delta > 0)
		seq_printf(m, "%lld,%lld\n", td_frame_cnt_delta, td_frame_duration_delta / td_frame_cnt_delta);
	else
		seq_puts(m, "0, 0\n");

	return 0;
}

static int proc_active_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_active_show, inode);
}

static const struct proc_ops proc_active_fops = {
	.proc_open = proc_active_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

void cl_enq_update(int pid) {
	s64 now = ktime_get_ns();

	if (!cl_enable)
		return;

	if (cl_aware_multi_enq) {
		if (pid != cl_main_enq) {
			/* pause for a while */
			cl_aware_multi_enq_ns = now + cl_aware_multi_enq_ts;
			cl_main_enq = pid;
		}
	}
}
EXPORT_SYMBOL_GPL(cl_enq_update);

void cl_chk_margin(int pid) {
	s64 now = ktime_get_ns();
	s64 delta = 0;
	int reason = CL_REASON_ACTIVE;

	if (!cl_enable) {
		if (cl_active) {
			cl_trig_cpufreq_update(-1);
			cl_update_active(0, now, CL_REASON_NOT_ENABLE);
		}
		return;
	}

	delta = now - cl_td_prev_ns;
	active_acc_frame_cnt += 1;
	active_acc_frame_duration += delta;
	cl_adaptive_weight_ratio = delta * 100 / cl_td_period_ns;
	cl_tp_int(CL_TP_WEIGHT_RATIO, cl_adaptive_weight_ratio, CL_TP_CRIT);

	if (cl_td_next_ns - now < cl_td_period_ns * cl_frame_margin / 100) {
		/* condition 1, remind margin large than margin_thres */
		cl_update_active(0, now, CL_REASON_FRAME_MARGIN_BREAK);
	} else if (now <= cl_aware_boost_ts + cl_aware_boost_hyst) {
		cl_update_active(0, now, CL_REASON_AWARE_BOOST_BREAK);
	} else if (cl_aware_usage && !cl_usage_chk()) {
		cl_update_active(0, now, CL_REASON_AWARE_USAGE_BREAK);
	} else if (cl_aware_multi_enq && (cl_split_usage || cl_float_usage || now < cl_aware_multi_enq_ns)) {
		cl_update_active(0, now, CL_REASON_MULTI_ENQ_BREAK);
	} else if (cl_aware_glthread && (cl_glthread_usage || cl_flutter_usage)) {
		cl_update_active(0, now, CL_REASON_GLTHREAD_BREAK);
	} else if (cl_aware_camera && cl_aware_camera_usage) {
		cl_update_active(0, now, CL_REASON_CAMERA_BREAK);
	} else if (cl_aware_block && now <= cl_aware_block_ts + cl_aware_block_hyst * 1000000000L) {
		cl_update_active(0, now, CL_REASON_BLOCK_BREAK);
	} else {
		cl_update_active(1, now, CL_REASON_ACTIVE);
	}

	if (cl_debug) {
		pr_err("%s,%d,%lld,%lld,%lld,%d,%lld,%d,%d\n",
			__func__,
			current->pid,
			now,
			cl_td_next_ns,
			cl_td_period_ns,
			cl_frame_margin,
			cl_aware_multi_enq_ns,
			reason,
			cl_main_enq);
	}

	cl_trig_cpufreq_update(-1);
	cl_chk_margin_ns = now;
}
EXPORT_SYMBOL_GPL(cl_chk_margin);

static int cl_cal_usage(s64 period, s64 now)
{
	int i, j;
	s64 total_const_cyc = 0;
	int usage = cl_default_usage;

	for_each_possible_cpu(i) {
		if (cl_usage_only_count_const_cyc) {
			j = SYS_AMU_CONST_CYC;
			cl_amu_frame_curr.cyc[i][j] = per_cpu(amu_cntr, i).val[j];
			cl_amu_data.cyc[i][j] = cl_amu_frame_curr.cyc[i][j] - cl_amu_frame_prev.cyc[i][j];
			cl_amu_frame_prev.cyc[i][j] = cl_amu_frame_curr.cyc[i][j];
		} else {
			for (j = 0; j < SYS_AMU_MAX; j++) {
				cl_amu_frame_curr.cyc[i][j] = per_cpu(amu_cntr, i).val[j];
				cl_amu_data.cyc[i][j] = cl_amu_frame_curr.cyc[i][j] - cl_amu_frame_prev.cyc[i][j];
				cl_amu_frame_prev.cyc[i][j] = cl_amu_frame_curr.cyc[i][j];
			}
		}
	}

	if (likely(cl_const_cyc_factor > 0)) {
		int cores = cpumask_weight(cpu_possible_mask);
		/* calculate const cycle for 8 cores, and scale to current cores */
		s64 total_const_cyc_max = (period / (s64) (cl_const_cyc_factor << 3)) * cores;

		if (cl_debug)
			pr_err("cores: %d\b", cores);

		if (unlikely(!total_const_cyc_max))
			return usage;

		for_each_possible_cpu(i) {
			total_const_cyc += cl_amu_data.cyc[i][SYS_AMU_CONST_CYC];
		}
		usage = total_const_cyc * 100 / total_const_cyc_max;
	}

	return usage;
}

static void amu_update_this_cpu(u64 time)
{
	int cpu = smp_processor_id();
	int i;

	if (ktime_sub(time, per_cpu(amu_last_update_time, cpu)) <= cl_amu_update_min_duration * NSEC_PER_MSEC)
		return;

	if (cl_usage_only_count_const_cyc) {
		per_cpu(amu_cntr, cpu).val[SYS_AMU_CONST_CYC] = read_sysreg_s(SYS_AMEVCNTR0_CONST_EL0);
		per_cpu(amu_delta, cpu).val[SYS_AMU_CONST_CYC] = per_cpu(amu_cntr, cpu).val[SYS_AMU_CONST_CYC] - per_cpu(amu_prev_cntr, cpu).val[SYS_AMU_CONST_CYC];
		per_cpu(amu_prev_cntr, cpu).val[SYS_AMU_CONST_CYC] = per_cpu(amu_cntr, cpu).val[SYS_AMU_CONST_CYC];
	} else {
		for (i = 0; i < SYS_AMU_MAX; ++i) {
			switch (i) {
			case SYS_AMU_CONST_CYC:
				per_cpu(amu_cntr, cpu).val[i] = read_sysreg_s(SYS_AMEVCNTR0_CONST_EL0);
				break;
			case SYS_AMU_CORE_CYC:
				per_cpu(amu_cntr, cpu).val[i] = read_sysreg_s(SYS_AMEVCNTR0_CORE_EL0);
				break;
			case SYS_AMU_INST_RET:
				per_cpu(amu_cntr, cpu).val[i] = read_sysreg_s(SYS_AMEVCNTR0_INST_RET_EL0);
				break;
			case SYS_AMU_STALL_MEM:
				per_cpu(amu_cntr, cpu).val[i] = read_sysreg_s(SYS_AMEVCNTR0_MEM_STALL);
				break;
			}

			per_cpu(amu_delta, cpu).val[i] = per_cpu(amu_cntr, cpu).val[i] - per_cpu(amu_prev_cntr, cpu).val[i];
			per_cpu(amu_prev_cntr, cpu).val[i] = per_cpu(amu_cntr, cpu).val[i];
		}
	}

	per_cpu(amu_update_delta_time, cpu) = ktime_sub(time, per_cpu(amu_last_update_time, cpu));
	per_cpu(amu_last_update_time, cpu) = time;
}

static void amu_update_tick_handler(void *data, struct rq *rq)
{
	u64 now = ktime_get();

	if (unlikely(!amu_enable))
		return;

	amu_update_this_cpu(now);
}

static void cl_init_amu(void)
{
	if (IS_ENABLED(CONFIG_ARM64_AMU_EXTN)) {
#ifdef CONFIG_ARCH_QCOM
		register_trace_android_vh_scheduler_tick(amu_update_tick_handler, NULL);
		amu_enable = 1;
#else
		amu_enable = 0;
#endif
	}
}

void cl_chk_td_period(bool from_app)
{
	s64 now = ktime_get_ns();
	s64 next = 0;
	s64 period = 0;
	s64 prev = cl_td_prev_ns;
	bool need_update_freq = false;

	if (!cl_enable) {
		if (cl_active) {
			cl_trig_cpufreq_update(-1);
			cl_update_active(0, now, CL_REASON_NOT_ENABLE);
		}
		return;
	}

	period = now - cl_td_prev_ns;
	next = now + period;

	/* update vsync period */
	if (cl_reset_on_vsync) {
		if (cl_active)
			need_update_freq = true;
		cl_update_active(0, now, CL_REASON_VSYNC_RESET_BREAK);
	} else if (cl_chk_margin_ns < cl_td_prev_ns) {
		if (cl_active)
			need_update_freq = true;
		cl_update_active(0, now, CL_REASON_RENDER_DELAY_BREAK);
	}

	/* standalone counting busy window */
	if (cl_aware_usage) {
		int curr;

		cl_usage_index = (cl_usage_index + 1) % cl_usage_hist_dyn;
		curr = cl_usage_index;

		if (!cl_usage_hist_update_cnt) {
			cl_usage_sum -= cl_usage_hist[curr];
			cl_usage_sum = max(cl_usage_sum, 0);

			cl_usage_hist[curr] = cl_cal_usage(period, now);

			cl_usage_curr = cl_usage_hist[curr];
			cl_usage_sum += cl_usage_curr;
			cl_usage_avg = cl_usage_sum / cl_usage_hist_dyn;
			if (!cl_usage_chk())
				cl_update_active(0, now, CL_REASON_AWARE_USAGE_BREAK);

			cl_tp_int(CL_TP_USAGE, cl_usage_curr, CL_TP_CRIT);
			cl_tp_int(CL_TP_USAGE_AVG, cl_usage_avg, CL_TP_CRIT);
		} else {
			if (cl_usage_hist_update_cnt-- == cl_usage_hist_dyn) {
				memset(&cl_usage_hist, 0, sizeof(int) * CL_USAGE_HIST_SIZE);
			}

			cl_usage_curr = 0;
			cl_usage_sum = 0;
			cl_usage_avg = 0;

			/* sanity check */
			if (cl_usage_hist_update_cnt < 0)
				cl_usage_hist_update_cnt = 0;
		}

		if (cl_debug) {
			pr_err("%s: [%d,%d,%d,%d,%d,%d,%d,%d] [%d,%d]\n",
					__func__,
					cl_usage_hist[0], cl_usage_hist[1],
					cl_usage_hist[2], cl_usage_hist[3],
					cl_usage_hist[4], cl_usage_hist[5],
					cl_usage_hist[6], cl_usage_hist[7],
					cl_usage_sum, cl_usage_avg);
		}
	}

	cl_td_next_ns = next;
	cl_td_period_ns = period;
	cl_td_prev_ns = now;

	/* less than 1s */
	if (period <= 1000000000) {
		td_acc_frame_cnt += 1;
		td_acc_frame_duration += period;
	}

	if (need_update_freq)
		cl_trig_cpufreq_update(-1);

	cl_tp_ll(CL_TP_TD_PERIOD, period, CL_TP_CRIT);

	if (cl_debug) {
		pr_err("%s,%lld,%lld,%lld,%d\n",
				__func__,
				prev,
				cl_td_next_ns,
				cl_td_period_ns,
				from_app);
	}
}
EXPORT_SYMBOL_GPL(cl_chk_td_period);

void android_vh_freq_qos_update_request(void *unused, struct freq_qos_request *req, int new_value)
{
	struct freq_constraints *qos = req->qos;
	int type = req->type;
	int old_value = 0;

	if (!cl_enable)
		return;

	if (!cl_aware_boost)
		return;

	if (type == FREQ_QOS_MIN) {
		old_value = qos->min_freq.target_value;
		if (new_value > old_value) {
			/* pull high */
			if (cl_debug)
				pr_err("%s: val %d %d\n", __func__, old_value, new_value);
			cl_update_active(0, ktime_get_ns(), CL_REASON_AWARE_BOOST_BREAK);
			cl_aware_boost_ts = ktime_get_ns();
		}
	}
}
EXPORT_SYMBOL_GPL(android_vh_freq_qos_update_request);

static void android_rvh_after_enqueue_task(void *unused, struct rq *rq,
		struct task_struct *p, int flags)
{
	if (!cl_enable) {
		if (cl_active) {
			cl_update_active(0, ktime_get_ns(), CL_REASON_NOT_ENABLE);
		}
		return;
	}

	/* quick turn off check */
	if (!cl_active)
		return;

	if (cl_aware_ed_task) {
		struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
		if (wrq->ed_task) {
			cl_update_active(0, ktime_get_ns(), CL_REASON_ED_TASK_BREAK);
			return;
		}
	}

	if (cl_aware_long_period) {
		s64 now = ktime_get_ns();

		if (now - cl_chk_margin_ns > cl_aware_long_period_ts) {
			cl_update_active(0, now, CL_REASON_LONG_PERIOD_BREAK);
			return;
		}
	}
}

static ssize_t proc_cl_usage_hist_dyn_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int val, ret;

	ret = kstrtoint_from_user(buf, count, 0, &val);
	if (ret)
		return ret;

	if (val > CL_USAGE_HIST_SIZE)
		return -EINVAL;

	if (val <= 0)
		return -EINVAL;

	cl_usage_hist_dyn = val;
	cl_usage_hist_update_cnt = cl_usage_hist_dyn;

	pr_err("%s: %d\n", __func__, cl_usage_hist_dyn);

	return count;
}

static int proc_cl_usage_hist_dyn_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", cl_usage_hist_dyn);
	return 0;
}

static int proc_cl_usage_hist_dyn_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_cl_usage_hist_dyn_show, inode);
}

static const struct proc_ops proc_cl_usage_hist_dyn_fops = {
	.proc_open = proc_cl_usage_hist_dyn_open,
	.proc_write = proc_cl_usage_hist_dyn_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t proc_cl_simple_weight_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[32], *sptr, *token, *delim = ",";
	int i = 0, err;

	memset(buffer, 0, sizeof(buffer));
	sptr = buffer;

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	token = strsep(&sptr, delim);
	while (token && i < 8) {
		err = kstrtoint(strstrip(token), 10, cl_simple_weight + i);
		if (err)
			return err;
		token = strsep(&sptr, delim);
		i++;
	}

	return count;
}

static int proc_cl_simple_weight_show(struct seq_file *m, void *v)
{
	for (int i = 0; i < 8; ++i) {
		seq_printf(m, "%d,", cl_simple_weight[i]);
	}
	seq_printf(m, "\n");

	return 0;
}

static int proc_cl_simple_weight_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_cl_simple_weight_show, inode);
}

static const struct proc_ops proc_cl_simple_weight_fops = {
	.proc_open = proc_cl_simple_weight_open,
	.proc_write = proc_cl_simple_weight_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_cl_active_reason_show(struct seq_file *m, void *v)
{
	int i;
	u64 acc_cur;

	seq_printf(m, "Active Reason, Prev, Curr\n");
	for (i = 0; i < CL_REASON_MAX; i++) {
		acc_cur = cl_acc.active_reason[i][CL_ACC_CURR] - cl_acc.active_reason[i][CL_ACC_PREV];
		seq_printf(m, "%s,%llu,%llu\n", cl_reason_msg[i], cl_acc.active_reason[i][CL_ACC_PREV], acc_cur);
		cl_acc.active_reason[i][CL_ACC_PREV] = cl_acc.active_reason[i][CL_ACC_CURR];
	}

	return 0;
};

static int proc_cl_active_reason_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_cl_active_reason_show, inode);
};

static const struct proc_ops proc_cl_active_reason_fops = {
	.proc_open = proc_cl_active_reason_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t proc_cl_multi_enq_write(struct file *file, const char __user *buf,
					size_t count, loff_t *ppos)
{
	char buffer[PROC_NUMBUF];
	int err;
	int input_val;

	memset(buffer, 0, sizeof(buffer));

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	err = kstrtoint(strstrip(buffer), 10, &input_val);
	if (err)
		return err;

	if (cl_debug)
		pr_err("%s: task %s %d, Input val %d\n", __func__, current->comm, current->pid, input_val);

	switch (input_val) {
	case CL_SPLIT_START:
		cl_split_usage = 1;
		cl_tp_int(CL_TP_MULTI_SPLIT, cl_split_usage, CL_TP_CRIT);
		break;
	case CL_SPLIT_END:
		cl_split_usage = 0;
		cl_tp_int(CL_TP_MULTI_SPLIT, cl_split_usage, CL_TP_CRIT);
		break;
	case CL_FLOAT_START:
		cl_float_usage = 1;
		cl_tp_int(CL_TP_MULTI_FLOAT, cl_float_usage, CL_TP_CRIT);
		break;
	case CL_FLOAT_END:
		cl_float_usage = 0;
		cl_tp_int(CL_TP_MULTI_FLOAT, cl_float_usage, CL_TP_CRIT);
		break;
	default:
		pr_err("%s: task %s %d, Unknown Input Value : %d\n",
			__func__, current->comm, current->pid, input_val);
		break;
	}
	return count;
}

static ssize_t proc_cl_multi_enq_read(struct file *file, char __user *buf,
					size_t count, loff_t *ppos)
{
	char buffer[PROC_NUMBUF];
	size_t len = 0;

	len = snprintf(buffer, sizeof(buffer), "%d, %d\n", cl_split_usage, cl_float_usage);

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static const struct proc_ops proc_cl_multi_enq_fops = {
        .proc_write = proc_cl_multi_enq_write,
        .proc_read = proc_cl_multi_enq_read,
        .proc_lseek = default_llseek,
};

static ssize_t proc_cl_glthread_write(struct file *file, const char __user *buf,
					size_t count, loff_t *ppos)
{
	char buffer[PROC_NUMBUF];
	int err;
	int input_val;

	memset(buffer, 0, sizeof(buffer));

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	err = kstrtoint(strstrip(buffer), 10, &input_val);
	if (err)
		return err;

	if (cl_debug)
		pr_err("%s: task %s %d, Input val %d\n", __func__, current->comm, current->pid, input_val);

	switch (input_val) {
	case CL_MOVE_TO_BACK:
		cl_glthread_usage = 0;
		cl_flutter_usage = 0;
		cl_tp_int(CL_TP_GLTHREAD, cl_glthread_usage, CL_TP_CRIT);
		cl_tp_int(CL_TP_FLUTTER, cl_flutter_usage, CL_TP_CRIT);
		break;
	case CL_GLTHREAD_LIST_START:
		cl_glthread_usage = 1;
		cl_tp_int(CL_TP_GLTHREAD, cl_glthread_usage, CL_TP_CRIT);
		break;
	case CL_GLTHREAD_LIST_END:
		cl_glthread_usage = 0;
		cl_tp_int(CL_TP_GLTHREAD, cl_glthread_usage, CL_TP_CRIT);
		break;
	case CL_FLUTTER_START:
		cl_flutter_usage = 1;
		cl_tp_int(CL_TP_FLUTTER, cl_flutter_usage, CL_TP_CRIT);
		break;
	case CL_FLUTTER_END:
		cl_flutter_usage = 0;
		cl_tp_int(CL_TP_FLUTTER, cl_flutter_usage, CL_TP_CRIT);
		break;
	default:
		pr_err("%s: task %s %d, Unknown Input Value : %d\n",
			__func__, current->comm, current->pid, input_val);
		break;
	}
	return count;
}


static ssize_t proc_cl_glthread_read(struct file *file, char __user *buf,
					size_t count, loff_t *ppos)
{
	char buffer[PROC_NUMBUF];
	size_t len = 0;

	len = snprintf(buffer, sizeof(buffer), "%d, %d\n", cl_glthread_usage, cl_flutter_usage);

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static const struct proc_ops proc_cl_glthread_fops = {
	.proc_write = proc_cl_glthread_write,
	.proc_read = proc_cl_glthread_read,
	.proc_lseek = default_llseek,
};

struct cl_core_ctl_map {
	const char *desc;
	int *val;
};

static struct cl_core_ctl_map cl_core_ctl_details[] = {
	{ "cl_enable", &cl_enable},
	{ "cl_aware_boost", &cl_aware_boost},
	{ "cl_aware_boost_hyst", &cl_aware_boost_hyst},
	{ "cl_aware_usage", &cl_aware_usage},
	{ "cl_aware_usage_dyn_limit", &cl_aware_usage_dyn_limit},
	{ "cl_aware_usage_hard_limit", &cl_aware_usage_hard_limit},
	{ "cl_aware_ed_task", &cl_aware_ed_task},
	{ "cl_aware_glthread", &cl_aware_glthread},
	{ "cl_aware_long_period", &cl_aware_long_period},
	{ "cl_aware_long_period_ts", &cl_aware_long_period_ts},
	{ "cl_aware_multi_enq", &cl_aware_multi_enq},
	{ "cl_adaptive_weight", &cl_adaptive_weight},
	{ "cl_adaptive_weight_min", &cl_adaptive_weight_min},
	{ "cl_adaptive_weight_max", &cl_adaptive_weight_max},
	{ "cl_boost_weight", &cl_boost_weight},
	{ "cl_frame_margin", &cl_frame_margin},
	{ "cl_reset_on_vsync", &cl_reset_on_vsync},
	{ "cl_update_on_change", &cl_update_on_change},
};

static ssize_t proc_cl_core_ctl_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[128] = {0}, *sptr, *token, *delim = " ";
	int size = sizeof(cl_core_ctl_details) / sizeof(struct cl_core_ctl_map), i = 0, err;

	sptr = buffer;

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	token = strsep(&sptr, delim);
	while (token && i < size) {
		err = kstrtoint(strstrip(token), 10, cl_core_ctl_details[i].val);
		if (err)
			return err;
		token = strsep(&sptr, delim);
		i++;
	}

	/* BLOCK hyst */
	if (cl_enable == 0) {
		cl_aware_block_ts = ktime_get_ns();
	}

	return count;
}

/* only for detail ouput */
static int cl_show_detail;
module_param(cl_show_detail, int, 0644);

static int proc_cl_core_ctl_show(struct seq_file *m, void *v)
{
	int size = sizeof(cl_core_ctl_details) / sizeof(struct cl_core_ctl_map), i;

	if (cl_show_detail) {
		seq_puts(m, "msg: ");
		for (i = 0; i < size; ++i) {
			seq_printf(m, "%d ", *cl_core_ctl_details[i].val);
		}
		seq_printf(m, "\n");
		for (i = 0; i < size; ++i) {
			seq_printf(m, "%s %d\n", cl_core_ctl_details[i].desc, *cl_core_ctl_details[i].val);
		}
		seq_printf(m, "\n");
	} else {
		for (i = 0; i < size; ++i) {
			seq_printf(m, "%d ", *cl_core_ctl_details[i].val);
		}
		seq_printf(m, "\n");
	}

	return 0;
}

static int proc_cl_core_ctl_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_cl_core_ctl_show, inode);
}

static const struct proc_ops proc_cl_core_ctl_fops = {
	.proc_open = proc_cl_core_ctl_open,
	.proc_write = proc_cl_core_ctl_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

PROC_FOPS(cl_debug);
PROC_FOPS(cl_reset_on_vsync);
PROC_FOPS(cl_update_on_change);
PROC_FOPS(cl_frame_margin);
PROC_FOPS(cl_enable);
PROC_FOPS(cl_tp_enable);
PROC_FOPS(cl_aware_multi_enq);
PROC_FOPS(cl_aware_multi_enq_ts);
PROC_FOPS(cl_aware_glthread);
PROC_FOPS(cl_adaptive_weight);
PROC_FOPS(cl_adaptive_weight_max);
PROC_FOPS(cl_adaptive_weight_min);
PROC_FOPS(cl_amu_update_min_duration);
PROC_FOPS(cl_aware_boost);
PROC_FOPS(cl_aware_boost_hyst);
PROC_FOPS(cl_aware_ed_task);
PROC_FOPS(cl_aware_long_period);
PROC_FOPS(cl_aware_long_period_ts);
PROC_FOPS(cl_aware_usage);
PROC_FOPS(cl_aware_usage_dyn_limit);
PROC_FOPS(cl_aware_usage_hard_limit);
PROC_FOPS(cl_aware_camera);
PROC_FOPS(cl_aware_camera_usage);
PROC_FOPS(cl_const_cyc_factor);
PROC_FOPS(cl_default_usage);
PROC_FOPS(cl_usage_only_count_const_cyc);
PROC_FOPS(cl_boost_weight);
PROC_FOPS(cl_aware_block);
PROC_FOPS(cl_aware_block_hyst);

#define PROC_OPLUS_CL "oplus_cl"
static struct proc_dir_entry *cl_root;

struct proc_dir_entry *cl_get_default_proc_dir_entry(void)
{
	if (cl_root)
		return cl_root;

	cl_root = proc_mkdir(PROC_OPLUS_CL, NULL);
	if (!cl_root)
		pr_warn("%s: can't create %s under /proc\n", __func__, PROC_OPLUS_CL);

	return cl_root;
}
EXPORT_SYMBOL_GPL(cl_get_default_proc_dir_entry);

void cl_init(struct proc_dir_entry *dir)
{
	int i, ret;

	for (i = 0; i < 8; ++i) {
		cl_simple_weight[i] = 80;
	}

	if (!dir)
		dir = cl_get_default_proc_dir_entry();

	/* only variable */
	PROC_CREATE("cl_debug", 0664, dir, cl_debug);
	PROC_CREATE("cl_reset_on_vsync", 0664, dir, cl_reset_on_vsync);
	PROC_CREATE("cl_update_on_change", 0664, dir, cl_update_on_change);
	PROC_CREATE("cl_frame_margin", 0664, dir, cl_frame_margin);
	PROC_CREATE("cl_enable", 0666, dir, cl_enable);
	PROC_CREATE("cl_tp_enable", 0664, dir, cl_tp_enable);
	PROC_CREATE("cl_aware_multi_enq", 0664, dir, cl_aware_multi_enq);
	PROC_CREATE("cl_aware_multi_enq_ts", 0664, dir, cl_aware_multi_enq_ts);
	PROC_CREATE("cl_aware_glthread", 0664, dir, cl_aware_glthread);
	PROC_CREATE("cl_adaptive_weight", 0664, dir, cl_adaptive_weight);
	PROC_CREATE("cl_adaptive_weight_max", 0664, dir, cl_adaptive_weight_max);
	PROC_CREATE("cl_adaptive_weight_min", 0664, dir, cl_adaptive_weight_min);
	PROC_CREATE("cl_amu_update_min_duration", 0664, dir, cl_amu_update_min_duration);
	PROC_CREATE("cl_aware_boost", 0664, dir, cl_aware_boost);
	PROC_CREATE("cl_aware_boost_hyst", 0664, dir, cl_aware_boost_hyst);
	PROC_CREATE("cl_aware_ed_task", 0664, dir, cl_aware_ed_task);
	PROC_CREATE("cl_aware_long_period", 0664, dir, cl_aware_long_period);
	PROC_CREATE("cl_aware_long_period_ts", 0664, dir, cl_aware_long_period_ts);
	PROC_CREATE("cl_aware_usage", 0664, dir, cl_aware_usage);
	PROC_CREATE("cl_aware_usage_dyn_limit", 0664, dir, cl_aware_usage_dyn_limit);
	PROC_CREATE("cl_aware_usage_hard_limit", 0664, dir, cl_aware_usage_hard_limit);
	PROC_CREATE("cl_aware_camera", 0664, dir, cl_aware_camera);
	PROC_CREATE("cl_aware_camera_usage", 0666, dir, cl_aware_camera_usage);
	PROC_CREATE("cl_const_cyc_factor", 0664, dir, cl_const_cyc_factor);
	PROC_CREATE("cl_default_usage", 0664, dir, cl_default_usage);
	PROC_CREATE("cl_usage_only_count_const_cyc", 0664, dir, cl_usage_only_count_const_cyc);
	PROC_CREATE("cl_boost_weight", 0664, dir, cl_boost_weight);
	PROC_CREATE("cl_aware_block", 0664, dir, cl_aware_block);
	PROC_CREATE("cl_aware_block_hyst", 0664, dir, cl_aware_block_hyst);

	/* with opts */
	proc_create("cl_simple_weight", 0664, dir, &proc_cl_simple_weight_fops);
	proc_create("cl_active_reason", 0664, dir, &proc_cl_active_reason_fops);
	proc_create("cl_usage_hist_dyn", 0664, dir, &proc_cl_usage_hist_dyn_fops);
	proc_create("active_stat", 0664, dir, &proc_active_fops);
	proc_create("cl_multi_enq", 0664, dir, &proc_cl_multi_enq_fops);
	proc_create("cl_glthread", 0664, dir, &proc_cl_glthread_fops);
	proc_create("cl_core_ctl", 0666, dir, &proc_cl_core_ctl_fops);

	/* keep for auto run temporary */
	proc_create("active_stat", 0444, NULL, &proc_active_fops);

	ret = register_trace_android_rvh_after_enqueue_task(android_rvh_after_enqueue_task, NULL);
	if (!ret) {
		pr_info("vendor hook registe succeed\n");
	}

	ret = register_trace_android_vh_freq_qos_update_request(android_vh_freq_qos_update_request, NULL);
	if (!ret) {
		pr_info("vendor hook registe succeed\n");
	}

	active_acc_update_ts_ns = ktime_get_ns();

	cl_init_amu();

	walt_cl_update_util_ops(cl_util, cl_boost_util);
}

static int __init close_loop_init(void)
{
	pr_err("%s\n", __func__);
	cl_init(NULL);
	return 0;
}

module_init(close_loop_init);
MODULE_DESCRIPTION("Oplus Close Loop Modular");
MODULE_LICENSE("GPL v2");
