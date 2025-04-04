#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/ww_mutex.h>
#include <linux/percpu-rwsem.h>
#include <linux/sched/signal.h>
#include <linux/sched/rt.h>
#include <linux/sched/wake_q.h>
#include <linux/sched/debug.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/debug_locks.h>
#include <linux/osq_lock.h>
#include <linux/sched_clock.h>
#include <linux/jiffies.h>
#include <linux/futex.h>
#include <linux/swap.h>
#include <linux/sched/cputime.h>
#include <../kernel/sched/sched.h>
#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/dtask.h>
#include <trace/hooks/binder.h>
#include <trace/hooks/rwsem.h>
#include <trace/hooks/futex.h>
#include <trace/hooks/fpsimd.h>
#include <trace/hooks/topology.h>
#include <trace/hooks/debug.h>
#include <trace/hooks/wqlockup.h>
#include <trace/hooks/cgroup.h>
#include <trace/hooks/sys.h>
#include <trace/hooks/mm.h>
#include <../kernel/oplus_cpu/sched/sched_assist/sa_common.h>

#define CREATE_TRACE_POINTS
#include "trace_oplus_locking.h"
#undef CREATE_TRACE_POINTS

#include "locking_main.h"

#define REGISTER_TRACE(vendor_hook, handler, data, err)	\
do {								\
	ret = register_trace_##vendor_hook(handler, data);				\
	if (ret) {						\
		pr_err("failed to register_trace_"#vendor_hook", ret=%d\n", ret);	\
		goto err;					\
	}							\
} while (0)

noinline int tracing_mark_write(const char *buf)
{
	trace_printk(buf);
	return 0;
}

inline bool test_task_is_fair(struct task_struct *task)
{
	if (unlikely(!task))
		return false;

	/* valid CFS priority is MAX_RT_PRIO..MAX_PRIO-1 */
	if ((task->prio >= MAX_RT_PRIO) && (task->prio <= MAX_PRIO-1))
		return true;
	return false;
}

#ifndef CONFIG_LOCKING_LAST_ENTITY
static DEFINE_PER_CPU(int, prev_locking_state);
#else
static DEFINE_PER_CPU(int, prev_locking_last);
#endif
static DEFINE_PER_CPU(int, prev_locking_depth);

#define LK_STATE_UNLOCK  (0)
#define LK_STATE_LOCK    (1)
#define LK_STATE_INVALID (2)
#define LK_TICK_HIT_MAX  (2)

#ifdef CONFIG_FAIR_GROUP_SCHED
/* Walk up scheduling entities hierarchy */
#define for_each_sched_entity(se) \
		for (; se; se = se->parent)
#else
#define for_each_sched_entity(se) \
		for (; se; se = NULL)
#endif

void locking_state_systrace_c(unsigned int cpu, struct task_struct *p)
{
	struct oplus_task_struct *ots;
	int locking_state, locking_depth;

	ots = get_oplus_task_struct(p);
	/*
	 * 0: ots alloced but not locking, not be protected.
	 * 1: ots alloced and locking, preempt protected.
	 * 2: ots not alloc, not be protected.
	 */
	if (IS_ERR_OR_NULL(ots)) {
		locking_state = p->pid ? LK_STATE_INVALID : LK_STATE_UNLOCK;
		locking_depth = 0;
	} else {
		locking_state = (ots->locking_start_time > 0 ? LK_STATE_LOCK : LK_STATE_UNLOCK);
		locking_depth = ots->locking_depth;
	}

#ifndef CONFIG_LOCKING_LAST_ENTITY
	if (per_cpu(prev_locking_state, cpu) != locking_state) {
		char buf[256];

		snprintf(buf, sizeof(buf), "C|9999|Cpu%d_locking_state|%d\n",
				cpu, locking_state);
		tracing_mark_write(buf);
		per_cpu(prev_locking_state, cpu) = locking_state;
	}
#else
	if (per_cpu(prev_locking_last, cpu) != LOCKING_DEFAULT) {
		char buf[256];
		snprintf(buf, sizeof(buf), "C|9999|Cpu%d_locking_last|%d\n",
				cpu, LOCKING_DEFAULT);
		tracing_mark_write(buf);
		per_cpu(prev_locking_last, cpu) = LOCKING_DEFAULT;
	}
#endif
	if (per_cpu(prev_locking_depth, cpu) != locking_depth) {
		char buf[256];

		snprintf(buf, sizeof(buf), "C|9999|Cpu%d_locking_depth|%d\n",
				cpu, locking_depth);
		tracing_mark_write(buf);
		per_cpu(prev_locking_depth, cpu) = locking_depth;
	}
}

static inline bool task_inlock(struct oplus_task_struct *ots)
{
	return ots->locking_start_time > 0;
}

#ifdef CONFIG_LOCKING_LAST_ENTITY
#define JIFFIES_OUTTIME_THRESHOLD 25
static int jiffies_outtime(unsigned long last_jiffies)
{
	unsigned long delta;
	//new-created threads may not have last_jiffies set
	if (unlikely((!last_jiffies)))
		return 0;

	delta = jiffies - last_jiffies;
	return (delta < JIFFIES_OUTTIME_THRESHOLD) ? 0 : 1;
}
#endif

void locking_tick_hit(struct task_struct *prev, struct task_struct *next)
{
	struct oplus_task_struct *ots;

	if (likely(prev != next)) {
		ots = get_oplus_task_struct(prev);
		if (!IS_ERR_OR_NULL(ots)) {
			if (ots->lk_tick_hit >= LK_TICK_HIT_MAX)
				ots->lk_tick_hit = 0;
#ifdef CONFIG_LOCKING_LAST_ENTITY
			ots->last_jiffies = jiffies;
#endif
		}
#ifdef CONFIG_LOCKING_LAST_ENTITY
		ots = get_oplus_task_struct(next);
		if (!IS_ERR_OR_NULL(ots))  {
			if (jiffies_outtime(ots->last_jiffies))
				ots->lk_tick_hit = 0;
		}
#endif
	}
}

#ifndef CONFIG_LOCKING_LAST_ENTITY
void enqueue_locking_thread(struct rq *rq, struct task_struct *p)
{
	struct oplus_task_struct *ots = NULL;
	struct oplus_rq *orq = NULL;
	struct list_head *pos, *n;
	unsigned long irqflag;

	if (unlikely(!locking_opt_enable(LK_PROTECT_ENABLE)))
		return;

	if (!rq || !p)
		return;

	ots = get_oplus_task_struct(p);
	orq = (struct oplus_rq *) rq->android_oem_data1;

	if (IS_ERR_OR_NULL(ots) || !orq)
		return;

	if (!oplus_list_empty(&ots->locking_entry))
		return;

	if (!test_task_is_fair(p))
		return;

	if (task_inlock(ots)) {
		bool exist = false;
		spin_lock_irqsave(orq->locking_list_lock, irqflag);
		list_for_each_safe(pos, n, &orq->locking_thread_list) {
			if (pos == &ots->locking_entry) {
				exist = true;
				break;
			}
		}
		if (!exist) {
			get_task_struct(p);
			list_add_tail(&ots->locking_entry, &orq->locking_thread_list);
			orq->rq_locking_task++;
			trace_enqueue_locking_thread(p, ots->locking_depth, orq->rq_locking_task);
		}
		spin_unlock_irqrestore(orq->locking_list_lock, irqflag);
	}
}

void dequeue_locking_thread(struct rq *rq, struct task_struct *p)
{
	struct oplus_task_struct *ots = NULL;
	struct oplus_rq *orq = NULL;
	struct list_head *pos, *n;
	unsigned long irqflag;

	if (!rq || !p)
		return;

	ots = get_oplus_task_struct(p);
	orq = (struct oplus_rq *) rq->android_oem_data1;

	if (IS_ERR_OR_NULL(ots) || !orq)
		return;

	spin_lock_irqsave(orq->locking_list_lock, irqflag);
	if (!oplus_list_empty(&ots->locking_entry)) {
		list_for_each_safe(pos, n, &orq->locking_thread_list) {
			if (pos == &ots->locking_entry) {
				list_del_init(&ots->locking_entry);
				orq->rq_locking_task--;
				trace_dequeue_locking_thread(p, ots->locking_depth, orq->rq_locking_task);
				put_task_struct(p);
				goto done;
			}
		}
	}
done:
	spin_unlock_irqrestore(orq->locking_list_lock, irqflag);
}

static inline bool orq_has_locking_tasks(struct oplus_rq *orq)
{
	bool ret = false;
	unsigned long irqflag;

	if (!orq)
		return false;
	spin_lock_irqsave(orq->locking_list_lock, irqflag);
	ret = !oplus_list_empty(&orq->locking_thread_list);
	spin_unlock_irqrestore(orq->locking_list_lock, irqflag);

	return ret;
}

void replace_next_task_fair_locking(struct rq *rq, struct task_struct **p,
					struct sched_entity **se, bool *repick, bool simple)
{
	struct oplus_rq *orq = NULL;
	struct list_head *pos = NULL;
	struct list_head *n = NULL;
	struct sched_entity *key_se;
	struct task_struct *key_task;
	struct oplus_task_struct *key_ots;
	unsigned long irqflag;

	if (unlikely(!locking_opt_enable(LK_PROTECT_ENABLE)))
		return;

	if (!rq || !p || !se)
		return;

	orq = (struct oplus_rq *)rq->android_oem_data1;
	if (!orq_has_locking_tasks(orq))
		return;
	spin_lock_irqsave(orq->locking_list_lock, irqflag);
	list_for_each_safe(pos, n, &orq->locking_thread_list) {
		key_ots = list_entry(pos, struct oplus_task_struct, locking_entry);

		if (IS_ERR_OR_NULL(key_ots))
			continue;

		key_task = ots_to_ts(key_ots);

		if (IS_ERR_OR_NULL(key_task)) {
			list_del_init(&key_ots->locking_entry);
			orq->rq_locking_task--;
			continue;
		}

		key_se = &key_task->se;

		if (!test_task_is_fair(key_task) || !task_inlock(key_ots)
			|| (key_task->flags & PF_EXITING) || unlikely(!key_se) || test_task_ux(key_task)) {
			list_del_init(&key_ots->locking_entry);
			orq->rq_locking_task--;
			put_task_struct(key_task);
			continue;
		}

		if (unlikely(task_cpu(key_task) != rq->cpu))
			continue;

		/*
		 * new task cpu must equals to this cpu, or is_same_group return null,
		 * it will cause stability issue in pick_next_task_fair()
		 */
		if (task_cpu(key_task) == cpu_of(rq)) {
			*p = key_task;
			*se = key_se;
			*repick = true;
			trace_select_locking_thread(key_task, key_ots->locking_depth, orq->rq_locking_task);
		} else
			pr_err("cpu%d replace key task failed, key_task cpu%d, \n", cpu_of(rq), task_cpu(key_task));

		break;
	}
	spin_unlock_irqrestore(orq->locking_list_lock, irqflag);
}
EXPORT_SYMBOL(replace_next_task_fair_locking);
#else
static inline s64 entity_key(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	return (s64)(se->vruntime - cfs_rq->min_vruntime);
}

static int vruntime_eligible(struct cfs_rq *cfs_rq, u64 vruntime)
{
	struct sched_entity *curr = cfs_rq->curr;
	s64 avg = cfs_rq->avg_vruntime;
	long load = cfs_rq->avg_load;

	if (curr && curr->on_rq) {
		unsigned long weight = scale_load_down(curr->load.weight);

		avg += entity_key(cfs_rq, curr) * weight;
		load += weight;
	}

	return avg >= (s64)(vruntime - cfs_rq->min_vruntime) * load;
}

void set_last_entity(struct task_struct *prev, struct task_struct *next, struct rq *rq)
{
	struct oplus_task_struct *ots;
	struct oplus_rq *orq;

	if (!prev || test_task_is_rt(prev) || test_task_ux(prev))
		return;

	ots = get_oplus_task_struct(prev);
	if (IS_ERR_OR_NULL(ots))
		return;

	if ((task_inlock(ots) && prev->se.on_rq == TASK_ON_RQ_QUEUED) && (test_task_is_rt(next) || test_task_ux(next))){
		orq = (struct oplus_rq *) rq->android_oem_data1;
		if (!orq->last_entity) {
			orq->last_entity = &prev->se;
			if (unlikely(global_debug_enabled & DEBUG_SYSTRACE)) {
				int cpu = cpu_of(rq);
				char buf[256];
				snprintf(buf, sizeof(buf), "C|9999|Cpu%d_locking_last|%d\n",
						cpu, LOCKING_SET_LAST);
				tracing_mark_write(buf);
				per_cpu(prev_locking_last, cpu) = LOCKING_SET_LAST;
			}
		}
	}
}

void pick_last_entity(struct rq *rq, struct task_struct **p,
					struct sched_entity **se, bool *repick, bool simple)
{
	struct oplus_rq *orq = NULL;
	struct sched_entity *key_se = NULL;
	struct task_struct *key_task = NULL;

	if (!rq || !p || !se)
		return;

	orq = (struct oplus_rq *)rq->android_oem_data1;
	if (!orq)
		return;

	if (orq->last_entity) {
		if (vruntime_eligible(cfs_rq_of(orq->last_entity), orq->last_entity->vruntime)) {
			key_se = orq->last_entity;
			key_task = task_of(key_se);
			if (task_cpu(key_task) == cpu_of(rq)) {
				*p = key_task;
				*se = key_se;
				*repick = true;
				if (unlikely(global_debug_enabled & DEBUG_SYSTRACE)) {
					int cpu = cpu_of(rq);
					char buf[256];
					snprintf(buf, sizeof(buf), "C|9999|Cpu%d_locking_last|%d\n",
							cpu, LOCKING_PICK_LAST);
					tracing_mark_write(buf);
					per_cpu(prev_locking_last, cpu) = LOCKING_PICK_LAST;
				}
			} else
				pr_err("cpu%d pick_last_entity failed, key_task cpu%d, \n", cpu_of(rq), task_cpu(key_task));
		} else {
			orq->last_entity = NULL;
			if (unlikely(global_debug_enabled & DEBUG_SYSTRACE)) {
				int cpu = cpu_of(rq);
				char buf[256];
				snprintf(buf, sizeof(buf), "C|9999|Cpu%d_locking_last|%d\n",
						cpu, LOCKING_CLEAR_INELIGIBLE);
				tracing_mark_write(buf);
				per_cpu(prev_locking_last, cpu) = LOCKING_CLEAR_INELIGIBLE;
			}
		}
	}
}

void clear_last_entity(struct rq *rq, struct task_struct *p)
{
	struct oplus_rq *orq = NULL;
	struct sched_entity *se = NULL;

	if (!rq || !p)
		return;

	orq = (struct oplus_rq *) rq->android_oem_data1;
	if (!orq)
		return;

	se = &(p->se);
	if (se && se == orq->last_entity) {
		orq->last_entity = NULL;
		if (unlikely(global_debug_enabled & DEBUG_SYSTRACE)) {
			int cpu = cpu_of(rq);
			char buf[256];
			snprintf(buf, sizeof(buf), "C|9999|Cpu%d_locking_last|%d\n",
					cpu, LOCKING_CLEAR_LAST);
			tracing_mark_write(buf);
			per_cpu(prev_locking_last, cpu) = LOCKING_CLEAR_LAST;
		}
	}

}
#endif

static void check_preempt_tick_locking(void *unused, struct cfs_rq *cfs_rq, struct sched_entity *curr)
{
	struct task_struct *curr_task = entity_is_task(curr) ? task_of(curr) : NULL;
	struct oplus_task_struct *ots;

	if (unlikely(!locking_opt_enable(LK_PROTECT_ENABLE)))
		return;

	if (NULL == curr_task)
		return;

	ots = get_oplus_task_struct(curr_task);
	if (IS_ERR_OR_NULL(ots))
		return;

	if (likely(0 == ots->locking_start_time))
		return;
	ots->lk_tick_hit++;
	if (task_inlock(ots) && (ots->lk_tick_hit >= LK_TICK_HIT_MAX)) {
		// locking protect overtime
		if (time_after(jiffies, ots->locking_start_time))
			ots->locking_start_time = 0;
	}
}

void check_preempt_wakeup_locking(struct rq *rq, struct task_struct *p, bool *preempt, bool *nopreempt)
{
	struct task_struct *curr;
	struct oplus_task_struct *ots;

	if (unlikely(!locking_opt_enable(LK_PROTECT_ENABLE)))
		return;

	curr = rq->curr;
	ots = get_oplus_task_struct(curr);

	if (IS_ERR_OR_NULL(ots))
		return;

	if (task_inlock(ots) && !test_task_ux(p)) {
		*nopreempt = true;
		*preempt = false;
#ifdef CONFIG_LOCKING_LAST_ENTITY
		if (unlikely(global_debug_enabled & DEBUG_SYSTRACE)) {
			int cpu = cpu_of(rq);
			char buf[256];
			snprintf(buf, sizeof(buf), "C|9999|Cpu%d_locking_last|%d\n",
					cpu, LOCKING_PROTECTED);
			tracing_mark_write(buf);
			per_cpu(prev_locking_last, cpu) = LOCKING_PROTECTED;
		}
#endif
	}
}

static inline bool locking_depth_skip(int locking_depth)
{
	/*
	 * rwsem: some thread will lock by myself but unlock in another thread,
	 * which causes to tsk locking_depth record err. Theoretically, a thread
	 * should not hold locks more than 32 layers, we skip depth-protect if so.
	 * */
	return locking_depth > 32;
}

static inline bool current_is_compress_thread(void)
{
	unsigned long im_flag;

	im_flag =  oplus_get_im_flag(current);
	return test_bit(IM_FLAG_COMPRESS_THREAD, &im_flag);
}

#ifndef CONFIG_LOCKING_LAST_ENTITY
static inline bool locking_protect_skip(void)
{
	/* Skip kswapd and compress thread */
	return current_is_kswapd() || current_is_compress_thread();
}
#else
static inline bool locking_protect_skip(void)
{
	return false;
}
#endif

static DEFINE_SPINLOCK(depth_lock);
void record_lock_starttime(struct task_struct *p, unsigned long settime)
{
	struct oplus_task_struct *ots = get_oplus_task_struct(p);

	if (test_task_is_rt(p)) {
		return;
	}

	if (unlikely(locking_protect_skip()))
		return;

	ots = get_oplus_task_struct(p);
	if (IS_ERR_OR_NULL(ots)) {
		return;
	}

	if (ots->locking_depth > 32) {
		ots->locking_start_time = 0;
		return;
	}

	if (settime > 0) {
		spin_lock(&depth_lock);
		ots->locking_depth++;
		spin_unlock(&depth_lock);
		goto set;
	}

	if (unlikely(ots->locking_depth <= 0)) {
		ots->locking_depth = 0;
		goto set;
	}

	spin_lock(&depth_lock);
	--(ots->locking_depth);
	spin_unlock(&depth_lock);

	if (ots->locking_depth) {
		return;
	}

set:
	ots->locking_start_time = settime;
}
EXPORT_SYMBOL(record_lock_starttime);

void opt_ss_lock_contention(struct task_struct *p, unsigned long old_im, int new_im)
{
	if(new_im == IM_FLAG_SS_LOCK_OWNER) {
		bool skip_scene = sched_assist_scene(SA_CAMERA);

		if(unlikely(!global_sched_assist_enabled || skip_scene))
			return;
	}

	/*if the task leave the critical section. clear the locking_state*/
	if (test_bit(IM_FLAG_SS_LOCK_OWNER, &old_im)) {
		record_lock_starttime(p, 0);
		goto out;
	}

	record_lock_starttime(p, jiffies);

out:
	if (unlikely(global_debug_enabled & DEBUG_FTRACE))
		trace_printk("4.comm=%-12s pid=%d tgid=%d old_im=0x%08lx new_im=%d\n",
			p->comm, p->pid, p->tgid, old_im, new_im);
}

static void update_locking_time(unsigned long time, bool in_cs)
{
	struct oplus_task_struct *ots;

	/* Rt thread do not need our help. */
	if (test_task_is_rt(current))
		return;

	/* Do not protect some tasks. */
	if (unlikely(locking_protect_skip()))
		return;

	ots = get_oplus_task_struct(current);
	if (IS_ERR_OR_NULL(ots))
		return;

	if (unlikely(!locking_opt_enable(LK_PROTECT_ENABLE)) && (time > 0 || !ots->locking_depth))
		return;

	/*
	 * We are not really acquired the lock and going into critical section,
	 * do not update locking depth.
	 */
	if (!in_cs)
		goto set;

	if (locking_depth_skip(ots->locking_depth)) {
		/*
		 * If locking_depth record err, we should not
		 * protect the thread which maybe in unlock state.
		 */
		ots->locking_start_time = 0;
		return;
	}

	/*
	 * Current has acquired the lock, increase it's locking depth.
	 * The depth over one means current hold more than one lock.
	 */
	if (time > 0) {
		ots->locking_depth++;
		goto set;
	}

	/*
	 * Current has released the lock, decrease it's locking depth.
	 * The depth become zero means current has leave all the critical section.
	 */
	if (unlikely(ots->locking_depth <= 0)) {
		ots->locking_depth = 0;
		goto set;
	}

	if (--(ots->locking_depth))
		return;

set:
	if (ots->lk_tick_hit < LK_TICK_HIT_MAX) {
		ots->locking_start_time = time;
	}
}

static void android_vh_mutex_wait_start_handler(void *unused, struct mutex *lock)
{
	update_locking_time(jiffies, false);
}

static void android_vh_rtmutex_wait_start_handler(void *unused, struct rt_mutex_base *lock)
{
	update_locking_time(jiffies, false);
}

static void record_lock_starttime_handler(void *unused,
			struct task_struct *tsk, unsigned long settime)
{
	update_locking_time(settime, true);
}

#ifdef CONFIG_PCPU_RWSEM_LOCKING_PROTECT
static void percpu_rwsem_wq_add_handler(void *unused,
			struct percpu_rw_semaphore *sem, bool reader)
{
	if (likely(reader))
		update_locking_time(jiffies, false);
}
#endif

static void android_vh_alter_rwsem_list_add_handler(void *unused, struct rwsem_waiter *waiter,
					struct rw_semaphore *sem, bool *already_on_list)
{
	update_locking_time(jiffies, false);
}

static void record_mutex_lock_starttime_handler(void *unused,
			struct mutex *mutex, unsigned long settime)
{
	record_lock_starttime_handler(NULL, current, settime);
}

static void record_rwsem_lock_starttime_handler(void *unused,
			struct rw_semaphore *sem, unsigned long settime)
{
	record_lock_starttime_handler(NULL, current, settime);
}

static void record_rtmutex_lock_starttime_handler(void *unused,
			struct rt_mutex *lock, unsigned long settime)
{
	record_lock_starttime_handler(NULL, current, settime);
}

#ifdef CONFIG_PCPU_RWSEM_LOCKING_PROTECT
static void record_pcprwsem_starttime_handler(void *unused,
			struct percpu_rw_semaphore *sem, unsigned long settime)
{
	record_lock_starttime_handler(NULL, current, settime);
}
#endif

static int register_dstate_opt_vendor_hooks(void)
{
	int ret = 0;

	REGISTER_TRACE(android_vh_record_mutex_lock_starttime, record_mutex_lock_starttime_handler, NULL, out);
	REGISTER_TRACE(android_vh_record_rtmutex_lock_starttime, record_rtmutex_lock_starttime_handler, NULL, out1);
	REGISTER_TRACE(android_vh_record_rwsem_lock_starttime, record_rwsem_lock_starttime_handler, NULL, out2);

#ifdef CONFIG_PCPU_RWSEM_LOCKING_PROTECT
	REGISTER_TRACE(android_vh_record_pcpu_rwsem_starttime, record_pcprwsem_starttime_handler, NULL, out3);
	REGISTER_TRACE(android_vh_percpu_rwsem_wq_add, percpu_rwsem_wq_add_handler, NULL, out4);
#endif

	REGISTER_TRACE(android_vh_alter_rwsem_list_add, android_vh_alter_rwsem_list_add_handler, NULL, out5);
	REGISTER_TRACE(android_vh_mutex_wait_start, android_vh_mutex_wait_start_handler, NULL, out5);
	REGISTER_TRACE(android_vh_rtmutex_wait_start, android_vh_rtmutex_wait_start_handler, NULL, out5);

	REGISTER_TRACE(android_rvh_entity_tick, check_preempt_tick_locking, NULL, out6);

	return ret;

out6:
	unregister_trace_android_vh_rtmutex_wait_start(
				android_vh_rtmutex_wait_start_handler, NULL);
	unregister_trace_android_vh_mutex_wait_start(
					android_vh_mutex_wait_start_handler, NULL);
	unregister_trace_android_vh_alter_rwsem_list_add(
					android_vh_alter_rwsem_list_add_handler, NULL);
out5:
#ifdef CONFIG_PCPU_RWSEM_LOCKING_PROTECT
	unregister_trace_android_vh_percpu_rwsem_wq_add(
					percpu_rwsem_wq_add_handler, NULL);
out4:
	unregister_trace_android_vh_record_pcpu_rwsem_starttime(
				record_pcprwsem_starttime_handler, NULL);
out3:
#endif
	unregister_trace_android_vh_record_rwsem_lock_starttime(
				record_rwsem_lock_starttime_handler, NULL);
out2:
	unregister_trace_android_vh_record_rtmutex_lock_starttime(
				record_rtmutex_lock_starttime_handler, NULL);
out1:
	unregister_trace_android_vh_record_mutex_lock_starttime(
				record_mutex_lock_starttime_handler, NULL);
out:
	return ret;
}

static void unregister_dstate_opt_vendor_hooks(void)
{
	unregister_trace_android_vh_rtmutex_wait_start(
				android_vh_rtmutex_wait_start_handler, NULL);
	unregister_trace_android_vh_mutex_wait_start(
				android_vh_mutex_wait_start_handler, NULL);
	unregister_trace_android_vh_alter_rwsem_list_add(
			android_vh_alter_rwsem_list_add_handler, NULL);
#ifdef CONFIG_PCPU_RWSEM_LOCKING_PROTECT
	unregister_trace_android_vh_percpu_rwsem_wq_add(
					percpu_rwsem_wq_add_handler, NULL);
	unregister_trace_android_vh_record_pcpu_rwsem_starttime(
				record_pcprwsem_starttime_handler, NULL);
#endif
	unregister_trace_android_vh_record_mutex_lock_starttime(
				record_mutex_lock_starttime_handler, NULL);
	unregister_trace_android_vh_record_rtmutex_lock_starttime(
				record_rtmutex_lock_starttime_handler, NULL);
	unregister_trace_android_vh_record_rwsem_lock_starttime(
				record_rwsem_lock_starttime_handler, NULL);
}


struct sched_assist_locking_ops sa_ops = {
#ifndef CONFIG_LOCKING_LAST_ENTITY
	.replace_next_task_fair = replace_next_task_fair_locking,
	.enqueue_entity  = enqueue_locking_thread,
	.dequeue_entity = dequeue_locking_thread,
#else
	.set_last_entity = set_last_entity,
	.pick_last_entity = pick_last_entity,
	.clear_last_entity = clear_last_entity,
#endif
	.check_preempt_wakeup = check_preempt_wakeup_locking,
	.state_systrace_c = locking_state_systrace_c,
	.locking_tick_hit = locking_tick_hit,
};

int sched_assist_locking_init(void)
{
	int ret = 0;

	register_sched_assist_locking_ops(&sa_ops);

	ret = register_dstate_opt_vendor_hooks();
	if (ret != 0)
		return ret;

	pr_info("%s succeed!\n", __func__);
	return 0;
}

void sched_assist_locking_exit(void)
{
	unregister_dstate_opt_vendor_hooks();
	pr_info("%s exit init succeed!\n", __func__);
}
