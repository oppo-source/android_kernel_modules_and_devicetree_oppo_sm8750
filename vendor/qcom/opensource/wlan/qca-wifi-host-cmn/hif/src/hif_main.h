/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * NB: Inappropriate references to "HTC" are used in this (and other)
 * HIF implementations.  HTC is typically the calling layer, but it
 * theoretically could be some alternative.
 */

/*
 * This holds all state needed to process a pending send/recv interrupt.
 * The information is saved here as soon as the interrupt occurs (thus
 * allowing the underlying CE to re-use the ring descriptor). The
 * information here is eventually processed by a completion processing
 * thread.
 */

#ifndef __HIF_MAIN_H__
#define __HIF_MAIN_H__

#include <qdf_atomic.h>         /* qdf_atomic_read */
#include "qdf_lock.h"
#include "cepci.h"
#include "hif.h"
#include "multibus.h"
#include "hif_unit_test_suspend_i.h"
#ifdef HIF_CE_LOG_INFO
#include "qdf_notifier.h"
#endif
#include "pld_common.h"

#define HIF_MIN_SLEEP_INACTIVITY_TIME_MS     50
#define HIF_SLEEP_INACTIVITY_TIMER_PERIOD_MS 60

#define HIF_MAX_BUDGET 0xFFFF

#define HIF_STATS_INC(_handle, _field, _delta) \
{ \
	(_handle)->stats._field += _delta; \
}

/*
 * This macro implementation is exposed for efficiency only.
 * The implementation may change and callers should
 * consider the targid to be a completely opaque handle.
 */
#define TARGID_TO_PCI_ADDR(targid) (*((A_target_id_t *)(targid)))

#ifdef QCA_WIFI_3_0
#define DISABLE_L1SS_STATES 1
#endif

#define MAX_NUM_OF_RECEIVES HIF_NAPI_MAX_RECEIVES

#ifdef QCA_WIFI_3_0_ADRASTEA
#define ADRASTEA_BU 1
#else
#define ADRASTEA_BU 0
#endif

#ifdef QCA_WIFI_3_0
#define HAS_FW_INDICATOR 0
#else
#define HAS_FW_INDICATOR 1
#endif


#define AR9888_DEVICE_ID (0x003c)
#define AR6320_DEVICE_ID (0x003e)
#define AR6320_FW_1_1  (0x11)
#define AR6320_FW_1_3  (0x13)
#define AR6320_FW_2_0  (0x20)
#define AR6320_FW_3_0  (0x30)
#define AR6320_FW_3_2  (0x32)
#define QCA6290_EMULATION_DEVICE_ID (0xabcd)
#define QCA6290_DEVICE_ID (0x1100)
#define QCN9000_DEVICE_ID (0x1104)
#define QCN9224_DEVICE_ID (0x1109)
#define QCN6122_DEVICE_ID (0xFFFB)
#define QCN9160_DEVICE_ID (0xFFF8)
#define QCN6432_DEVICE_ID (0xFFF7)
#define QCA5424_DEVICE_ID (0xFFF6)
#define QCA6390_EMULATION_DEVICE_ID (0x0108)
#define QCA6390_DEVICE_ID (0x1101)
/* TODO: change IDs for HastingsPrime */
#define QCA6490_EMULATION_DEVICE_ID (0x010a)
#define QCA6490_DEVICE_ID (0x1103)
#define MANGO_DEVICE_ID (0x110a)
#define PEACH_DEVICE_ID (0x110e)

/* TODO: change IDs for Moselle */
#define QCA6750_EMULATION_DEVICE_ID (0x010c)
#define QCA6750_DEVICE_ID (0x1105)

/* TODO: change IDs for Hamilton */
#define KIWI_DEVICE_ID (0x1107)

/*TODO: change IDs for Evros */
#define WCN6450_DEVICE_ID (0x1108)

#define WCN7750_DEVICE_ID (0x1110)

#define QCC2072_DEVICE_ID (0x1112)

#define ADRASTEA_DEVICE_ID_P2_E12 (0x7021)
#define AR9887_DEVICE_ID    (0x0050)
#define AR900B_DEVICE_ID    (0x0040)
#define QCA9984_DEVICE_ID   (0x0046)
#define QCA9888_DEVICE_ID   (0x0056)
#define QCA8074_DEVICE_ID   (0xffff) /* Todo: replace this with
					actual number once available.
					currently defining this to 0xffff for
					emulation purpose */
#define QCA8074V2_DEVICE_ID (0xfffe) /* Todo: replace this with actual number */
#define QCA6018_DEVICE_ID (0xfffd) /* Todo: replace this with actual number */
#define QCA5018_DEVICE_ID (0xfffc) /* Todo: replace this with actual number */
#define QCA9574_DEVICE_ID (0xfffa)
#define QCA5332_DEVICE_ID (0xfff9)
/* Genoa */
#define QCN7605_DEVICE_ID  (0x1102) /* Genoa PCIe device ID*/
#define QCN7605_COMPOSITE  (0x9901)
#define QCN7605_STANDALONE  (0x9900)
#define QCN7605_STANDALONE_V2  (0x9902)
#define QCN7605_COMPOSITE_V2  (0x9903)

#define RUMIM2M_DEVICE_ID_NODE0	0xabc0
#define RUMIM2M_DEVICE_ID_NODE1	0xabc1
#define RUMIM2M_DEVICE_ID_NODE2	0xabc2
#define RUMIM2M_DEVICE_ID_NODE3	0xabc3
#define RUMIM2M_DEVICE_ID_NODE4	0xaa10
#define RUMIM2M_DEVICE_ID_NODE5	0xaa11

#define HIF_GET_PCI_SOFTC(scn) ((struct hif_pci_softc *)scn)
#define HIF_GET_IPCI_SOFTC(scn) ((struct hif_ipci_softc *)scn)
#define HIF_GET_CE_STATE(scn) ((struct HIF_CE_state *)scn)
#define HIF_GET_SDIO_SOFTC(scn) ((struct hif_sdio_softc *)scn)
#define HIF_GET_USB_SOFTC(scn) ((struct hif_usb_softc *)scn)
#define HIF_GET_USB_DEVICE(scn) ((struct HIF_DEVICE_USB *)scn)
#define HIF_GET_SOFTC(scn) ((struct hif_softc *)scn)
#define GET_HIF_OPAQUE_HDL(scn) ((struct hif_opaque_softc *)scn)

#ifdef QCA_WIFI_QCN9224
#define NUM_CE_AVAILABLE 16
#else
#define NUM_CE_AVAILABLE 12
#endif
/* Add 1 here to store default configuration in index 0 */
#define NUM_CE_CONTEXT (NUM_CE_AVAILABLE + 1)

#define CE_INTERRUPT_IDX(x) x

#ifdef WLAN_64BIT_DATA_SUPPORT
#define RRI_ON_DDR_MEM_SIZE CE_COUNT * sizeof(uint64_t)
#else
#define RRI_ON_DDR_MEM_SIZE CE_COUNT * sizeof(uint32_t)
#endif

struct ce_int_assignment {
	uint8_t msi_idx[NUM_CE_AVAILABLE];
};

struct hif_ce_stats {
	int hif_pipe_no_resrc_count;
	int ce_ring_delta_fail_count;
};

#ifdef HIF_DETECTION_LATENCY_ENABLE
/**
 * struct hif_tasklet_running_info - running info of tasklet
 * @sched_cpuid: id of cpu on which the tasklet was scheduled
 * @sched_time: time when the tasklet was scheduled
 * @exec_time: time when the tasklet was executed
 */
struct hif_tasklet_running_info {
	int sched_cpuid;
	qdf_time_t sched_time;
	qdf_time_t exec_time;
};

#define HIF_TASKLET_IN_MONITOR CE_COUNT_MAX

struct hif_latency_detect {
	qdf_timer_t timer;
	uint32_t timeout;
	bool is_timer_started;
	bool enable_detection;
	/* threshold when stall happens */
	uint32_t threshold;

	/*
	 * Bitmap to indicate the enablement of latency detection for
	 * the tasklets. bit-X represents for tasklet of WLAN_CE_X,
	 * latency detection is enabled on the corresponding tasklet
	 * when a bit is set.
	 * At the same time, this bitmap also indicates the validity of
	 * elements in array 'tasklet_info', bit-X represents for index-X,
	 * the corresponding element is valid when a bit is set.
	 */
	qdf_bitmap(tasklet_bmap, HIF_TASKLET_IN_MONITOR);

	/*
	 * Array to record running info of tasklets, info of tasklet
	 * for WLAN_CE_X is stored at index-X.
	 */
	struct hif_tasklet_running_info tasklet_info[HIF_TASKLET_IN_MONITOR];
	qdf_time_t credit_request_time;
	qdf_time_t credit_report_time;
};
#endif

/*
 * Note: For MCL, #if defined (HIF_CONFIG_SLUB_DEBUG_ON) needs to be checked
 * for defined here
 */
#if defined(HIF_CONFIG_SLUB_DEBUG_ON) || defined(HIF_CE_DEBUG_DATA_BUF)

#define HIF_CE_MAX_LATEST_HIST 2
#define HIF_CE_MAX_LATEST_EVTS 2

struct latest_evt_history {
	uint64_t irq_entry_ts;
	uint64_t bh_entry_ts;
	uint64_t bh_resched_ts;
	uint64_t bh_exit_ts;
	uint64_t bh_work_ts;
	int cpu_id;
	uint32_t ring_hp;
	uint32_t ring_tp;
};

struct ce_desc_hist {
	qdf_atomic_t history_index[CE_COUNT_MAX];
	uint8_t ce_id_hist_map[CE_COUNT_MAX];
	bool enable[CE_COUNT_MAX];
	bool data_enable[CE_COUNT_MAX];
	qdf_mutex_t ce_dbg_datamem_lock[CE_COUNT_MAX];
	uint32_t hist_index;
	uint32_t hist_id;
	void *hist_ev[CE_COUNT_MAX];
	struct latest_evt_history latest_evts[HIF_CE_MAX_LATEST_HIST][HIF_CE_MAX_LATEST_EVTS];
};

void hif_record_latest_evt(struct ce_desc_hist *ce_hist,
			   uint8_t type,
			   int ce_id, uint64_t time,
			   uint32_t hp, uint32_t tp);
#endif /*defined(HIF_CONFIG_SLUB_DEBUG_ON) || defined(HIF_CE_DEBUG_DATA_BUF)*/

/**
 * struct hif_cfg() - store ini config parameters in hif layer
 * @ce_status_ring_timer_threshold: ce status ring timer threshold
 * @ce_status_ring_batch_count_threshold: ce status ring batch count threshold
 * @disable_wake_irq: disable wake irq
 */
struct hif_cfg {
	uint16_t ce_status_ring_timer_threshold;
	uint8_t ce_status_ring_batch_count_threshold;
	bool disable_wake_irq;
};

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * struct hif_umac_reset_ctx - UMAC HW reset context at HIF layer
 * @intr_tq: Tasklet structure
 * @irq_handler: IRQ handler
 * @cb_handler: Callback handler
 * @cb_ctx: Argument to be passed to @cb_handler
 * @os_irq: Interrupt number for this IRQ
 * @irq_configured: Whether the IRQ has been configured
 */
struct hif_umac_reset_ctx {
	struct tasklet_struct intr_tq;
	bool (*irq_handler)(void *cb_ctx);
	int (*cb_handler)(void *cb_ctx);
	void *cb_ctx;
	uint32_t os_irq;
	bool irq_configured;
};
#endif

#define MAX_SHADOW_REGS 40

#ifdef FEATURE_HIF_DELAYED_REG_WRITE
/**
 * enum hif_reg_sched_delay - ENUM for write sched delay histogram
 * @HIF_REG_WRITE_SCHED_DELAY_SUB_100us: index for delay < 100us
 * @HIF_REG_WRITE_SCHED_DELAY_SUB_1000us: index for delay < 1000us
 * @HIF_REG_WRITE_SCHED_DELAY_SUB_5000us: index for delay < 5000us
 * @HIF_REG_WRITE_SCHED_DELAY_GT_5000us: index for delay >= 5000us
 * @HIF_REG_WRITE_SCHED_DELAY_HIST_MAX: Max value (nnsize of histogram array)
 */
enum hif_reg_sched_delay {
	HIF_REG_WRITE_SCHED_DELAY_SUB_100us,
	HIF_REG_WRITE_SCHED_DELAY_SUB_1000us,
	HIF_REG_WRITE_SCHED_DELAY_SUB_5000us,
	HIF_REG_WRITE_SCHED_DELAY_GT_5000us,
	HIF_REG_WRITE_SCHED_DELAY_HIST_MAX,
};

/**
 * struct hif_reg_write_soc_stats - soc stats to keep track of register writes
 * @enqueues: writes enqueued to delayed work
 * @dequeues: writes dequeued from delayed work (not written yet)
 * @coalesces: writes not enqueued since srng is already queued up
 * @direct: writes not enqueud and writted to register directly
 * @prevent_l1_fails: prevent l1 API failed
 * @q_depth: current queue depth in delayed register write queue
 * @max_q_depth: maximum queue for delayed register write queue
 * @sched_delay: = kernel work sched delay + bus wakeup delay, histogram
 * @dequeue_delay: dequeue operation be delayed
 */
struct hif_reg_write_soc_stats {
	qdf_atomic_t enqueues;
	uint32_t dequeues;
	qdf_atomic_t coalesces;
	qdf_atomic_t direct;
	uint32_t prevent_l1_fails;
	qdf_atomic_t q_depth;
	uint32_t max_q_depth;
	uint32_t sched_delay[HIF_REG_WRITE_SCHED_DELAY_HIST_MAX];
	uint32_t dequeue_delay;
};

/**
 * struct hif_reg_write_q_elem - delayed register write queue element
 * @ce_state: CE state queued for a delayed write
 * @offset: offset of the CE register
 * @enqueue_val: register value at the time of delayed write enqueue
 * @dequeue_val: register value at the time of delayed write dequeue
 * @valid: whether this entry is valid or not
 * @enqueue_time: enqueue time (qdf_log_timestamp)
 * @work_scheduled_time: work scheduled time (qdf_log_timestamp)
 * @dequeue_time: dequeue time (qdf_log_timestamp)
 * @cpu_id: record cpuid when schedule work
 */
struct hif_reg_write_q_elem {
	struct CE_state *ce_state;
	uint32_t offset;
	uint32_t enqueue_val;
	uint32_t dequeue_val;
	uint8_t valid;
	qdf_time_t enqueue_time;
	qdf_time_t work_scheduled_time;
	qdf_time_t dequeue_time;
	int cpu_id;
};
#endif

struct hif_softc {
	struct hif_opaque_softc osc;
	struct hif_config_info hif_config;
	struct hif_target_info target_info;
	void __iomem *mem;
	void __iomem *mem_ce;
	void __iomem *mem_cmem;
	void __iomem *mem_pmm_base;
	enum qdf_bus_type bus_type;
	struct hif_bus_ops bus_ops;
	void *ce_id_to_state[CE_COUNT_MAX];
	qdf_device_t qdf_dev;
	bool hif_init_done;
	bool request_irq_done;
	bool ext_grp_irq_configured;
	bool free_irq_done;
	uint8_t ce_latency_stats;
	/* Packet statistics */
	struct hif_ce_stats pkt_stats;
	enum hif_target_status target_status;
	uint64_t event_enable_mask;

	struct targetdef_s *targetdef;
	struct ce_reg_def *target_ce_def;
	struct hostdef_s *hostdef;
	struct host_shadow_regs_s *host_shadow_regs;

	bool recovery;
	bool notice_send;
	bool per_ce_irq;
	uint32_t ce_irq_summary;
	/* No of copy engines supported */
	unsigned int ce_count;
	struct ce_int_assignment *int_assignment;
	atomic_t active_tasklet_cnt;
	atomic_t active_grp_tasklet_cnt;
	atomic_t active_oom_work_cnt;
	atomic_t link_suspended;
	void *vaddr_rri_on_ddr;
	atomic_t active_wake_req_cnt;
	qdf_dma_addr_t paddr_rri_on_ddr;
#ifdef CONFIG_BYPASS_QMI
	uint32_t *vaddr_qmi_bypass;
	qdf_dma_addr_t paddr_qmi_bypass;
#endif
	int linkstate_vote;
	bool fastpath_mode_on;
	atomic_t tasklet_from_intr;
	int htc_htt_tx_endpoint;
	qdf_dma_addr_t mem_pa;
	bool athdiag_procfs_inited;
#ifdef FEATURE_NAPI
	struct qca_napi_data napi_data;
#endif /* FEATURE_NAPI */
	/* stores ce_service_max_yield_time in ns */
	unsigned long long ce_service_max_yield_time;
	uint8_t ce_service_max_rx_ind_flush;
	struct hif_driver_state_callbacks callbacks;
	uint32_t hif_con_param;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
	uint32_t nss_wifi_ol_mode;
#endif
	void *hal_soc;
	struct hif_ut_suspend_context ut_suspend_ctx;
	uint32_t hif_attribute;
	int wake_irq;
	hif_pm_wake_irq_type wake_irq_type;
	void (*initial_wakeup_cb)(void *);
	void *initial_wakeup_priv;
#ifdef REMOVE_PKT_LOG
	/* Handle to pktlog device */
	void *pktlog_dev;
#endif
#ifdef WLAN_FEATURE_DP_EVENT_HISTORY
	/* Pointer to the srng event history */
	struct hif_event_history *evt_hist[HIF_NUM_INT_CONTEXTS];
#endif

/*
 * Note: For MCL, #if defined (HIF_CONFIG_SLUB_DEBUG_ON) needs to be checked
 * for defined here
 */
#if defined(HIF_CONFIG_SLUB_DEBUG_ON) || defined(HIF_CE_DEBUG_DATA_BUF)
	struct ce_desc_hist hif_ce_desc_hist;
#endif /*defined(HIF_CONFIG_SLUB_DEBUG_ON) || defined(HIF_CE_DEBUG_DATA_BUF)*/
#ifdef IPA_OFFLOAD
	qdf_shared_mem_t *ipa_ce_ring;
#endif
#ifdef IPA_OPT_WIFI_DP
	qdf_atomic_t opt_wifi_dp_rtpm_cnt;
#endif
	struct hif_cfg ini_cfg;
#ifdef HIF_CE_LOG_INFO
	qdf_notif_block hif_recovery_notifier;
#endif
#if defined(HIF_CPU_PERF_AFFINE_MASK) || \
	defined(FEATURE_ENABLE_CE_DP_IRQ_AFFINE)

	/* The CPU hotplug event registration handle */
	struct qdf_cpuhp_handler *cpuhp_event_handle;
#endif
	uint32_t irq_unlazy_disable;
	/* Should the unlzay support for interrupt delivery be disabled */
	/* Flag to indicate whether bus is suspended */
	bool bus_suspended;
	bool pktlog_init;
#ifdef FEATURE_RUNTIME_PM
	/* Variable to track the link state change in RTPM */
	qdf_atomic_t pm_link_state;
#endif
#ifdef HIF_DETECTION_LATENCY_ENABLE
	struct hif_latency_detect latency_detect;
#endif
#ifdef FEATURE_RUNTIME_PM
	qdf_runtime_lock_t prevent_linkdown_lock;
#endif
#ifdef SYSTEM_PM_CHECK
	qdf_atomic_t sys_pm_state;
#endif
#if defined(HIF_IPCI) && defined(FEATURE_HAL_DELAYED_REG_WRITE)
	qdf_atomic_t dp_ep_vote_access;
	qdf_atomic_t ep_vote_access;
#endif
	/* CMEM address target reserved for host usage */
	uint64_t cmem_start;
	/* CMEM size target reserved */
	uint64_t cmem_size;
#ifdef DP_UMAC_HW_RESET_SUPPORT
	struct hif_umac_reset_ctx umac_reset_ctx;
#endif
#ifdef CONFIG_SHADOW_V3
	struct pld_shadow_reg_v3_cfg shadow_regs[MAX_SHADOW_REGS];
	int num_shadow_registers_configured;
#endif
#ifdef WLAN_FEATURE_AFFINITY_MGR
	/* CPU Affinity info of IRQs */
	bool affinity_mgr_supported;
	uint64_t time_threshold;
	struct hif_cpu_affinity ce_irq_cpu_mask[CE_COUNT_MAX];
	struct hif_cpu_affinity irq_cpu_mask[HIF_MAX_GROUP][HIF_MAX_GRP_IRQ];
	qdf_cpu_mask allowed_mask;
#endif
#ifdef FEATURE_DIRECT_LINK
	struct qdf_mem_multi_page_t dl_recv_pages;
	int dl_recv_pipe_num;
#endif
#ifdef WLAN_FEATURE_CE_RX_BUFFER_REUSE
	struct wbuff_mod_handle *wbuff_handle;
#endif
#ifdef FEATURE_HIF_DELAYED_REG_WRITE
	/* queue(array) to hold register writes */
	struct hif_reg_write_q_elem *reg_write_queue;
	/* delayed work to be queued into workqueue */
	qdf_work_t reg_write_work;
	/* workqueue for delayed register writes */
	qdf_workqueue_t *reg_write_wq;
	/* write index used by caller to enqueue delayed work */
	qdf_atomic_t write_idx;
	/* read index used by worker thread to dequeue/write registers */
	uint32_t read_idx;
	struct hif_reg_write_soc_stats wstats;
	qdf_atomic_t active_work_cnt;
#endif /* FEATURE_HIF_DELAYED_REG_WRITE */
#ifdef WLAN_DP_LOAD_BALANCE_SUPPORT
	bool is_load_balance_enabled;
#endif
};

#if defined(NUM_SOC_PERF_CLUSTER) && (NUM_SOC_PERF_CLUSTER > 1)
static inline uint16_t hif_get_perf_cluster_bitmap(void)
{
	return (BIT(CPU_CLUSTER_TYPE_PERF) | BIT(CPU_CLUSTER_TYPE_PERF2));
}
#else /* NUM_SOC_PERF_CLUSTER > 1 */
static inline uint16_t hif_get_perf_cluster_bitmap(void)
{
	return BIT(CPU_CLUSTER_TYPE_PERF);
}
#endif /* NUM_SOC_PERF_CLUSTER > 1 */

static inline
void *hif_get_hal_handle(struct hif_opaque_softc *hif_hdl)
{
	struct hif_softc *sc = (struct hif_softc *)hif_hdl;

	if (!sc)
		return NULL;

	return sc->hal_soc;
}

/**
 * hif_get_cmem_info() - get CMEM address and size from HIF handle
 * @hif_hdl: HIF handle pointer
 * @cmem_start: pointer for CMEM address
 * @cmem_size: pointer for CMEM size
 *
 * Return: None.
 */
static inline
void hif_get_cmem_info(struct hif_opaque_softc *hif_hdl,
		       uint64_t *cmem_start,
		       uint64_t *cmem_size)
{
	struct hif_softc *sc = (struct hif_softc *)hif_hdl;

	*cmem_start = sc->cmem_start;
	*cmem_size = sc->cmem_size;
}

/**
 * hif_get_num_active_tasklets() - get the number of active
 *		tasklets pending to be completed.
 * @scn: HIF context
 *
 * Returns: the number of tasklets which are active
 */
static inline int hif_get_num_active_tasklets(struct hif_softc *scn)
{
	return qdf_atomic_read(&scn->active_tasklet_cnt);
}

/**
 * hif_get_num_active_oom_work() - get the number of active
 *		oom work pending to be completed.
 * @scn: HIF context
 *
 * Returns: the number of oom works which are active
 */
static inline int hif_get_num_active_oom_work(struct hif_softc *scn)
{
	return qdf_atomic_read(&scn->active_oom_work_cnt);
}

/*
 * Max waiting time during Runtime PM suspend to finish all
 * the tasks. This is in the multiple of 10ms.
 */
#ifdef PANIC_ON_BUG
#define HIF_TASK_DRAIN_WAIT_CNT 200
#else
#define HIF_TASK_DRAIN_WAIT_CNT 25
#endif

/**
 * hif_try_complete_tasks() - Try to complete all the pending tasks
 * @scn: HIF context
 *
 * Try to complete all the pending datapath tasks, i.e. tasklets,
 * DP group tasklets and works which are queued, in a given time
 * slot.
 *
 * Returns: QDF_STATUS_SUCCESS if all the tasks were completed
 *	QDF error code, if the time slot exhausted
 */
QDF_STATUS hif_try_complete_tasks(struct hif_softc *scn);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
static inline bool hif_is_nss_wifi_enabled(struct hif_softc *sc)
{
	return !!(sc->nss_wifi_ol_mode);
}
#else
static inline bool hif_is_nss_wifi_enabled(struct hif_softc *sc)
{
	return false;
}
#endif

static inline uint8_t hif_is_attribute_set(struct hif_softc *sc,
						uint32_t hif_attrib)
{
	return sc->hif_attribute == hif_attrib;
}

#ifdef WLAN_FEATURE_DP_EVENT_HISTORY
static inline void hif_set_event_hist_mask(struct hif_opaque_softc *hif_handle)
{
	struct hif_softc *scn = (struct hif_softc *)hif_handle;

	scn->event_enable_mask = HIF_EVENT_HIST_ENABLE_MASK;
}
#else
static inline void hif_set_event_hist_mask(struct hif_opaque_softc *hif_handle)
{
}
#endif /* WLAN_FEATURE_DP_EVENT_HISTORY */

A_target_id_t hif_get_target_id(struct hif_softc *scn);
void hif_dump_pipe_debug_count(struct hif_softc *scn);
void hif_display_bus_stats(struct hif_opaque_softc *scn);
void hif_clear_bus_stats(struct hif_opaque_softc *scn);
bool hif_max_num_receives_reached(struct hif_softc *scn, unsigned int count);
void hif_shutdown_device(struct hif_opaque_softc *hif_ctx);
int hif_bus_configure(struct hif_softc *scn);
void hif_cancel_deferred_target_sleep(struct hif_softc *scn);
int hif_config_ce(struct hif_softc *scn);
int hif_config_ce_pktlog(struct hif_opaque_softc *hif_ctx);
int hif_config_ce_by_id(struct hif_softc *scn, int pipe_num);
void hif_unconfig_ce(struct hif_softc *scn);
void hif_ce_prepare_config(struct hif_softc *scn);
QDF_STATUS hif_ce_open(struct hif_softc *scn);
void hif_ce_close(struct hif_softc *scn);
int athdiag_procfs_init(void *scn);
void athdiag_procfs_remove(void);
/* routine to modify the initial buffer count to be allocated on an os
 * platform basis. Platform owner will need to modify this as needed
 */
qdf_size_t init_buffer_count(qdf_size_t maxSize);

irqreturn_t hif_fw_interrupt_handler(int irq, void *arg);
int hif_get_device_type(uint32_t device_id,
			uint32_t revision_id,
			uint32_t *hif_type, uint32_t *target_type);
/*These functions are exposed to HDD*/
void hif_nointrs(struct hif_softc *scn);
void hif_bus_close(struct hif_softc *ol_sc);
QDF_STATUS hif_bus_open(struct hif_softc *ol_sc,
	enum qdf_bus_type bus_type);
QDF_STATUS hif_enable_bus(struct hif_softc *ol_sc, struct device *dev,
	void *bdev, const struct hif_bus_id *bid, enum hif_enable_type type);
void hif_disable_bus(struct hif_softc *scn);
void hif_bus_prevent_linkdown(struct hif_softc *scn, bool flag);
int hif_bus_get_context_size(enum qdf_bus_type bus_type);
void hif_read_phy_mem_base(struct hif_softc *scn, qdf_dma_addr_t *bar_value);
uint32_t hif_get_conparam(struct hif_softc *scn);
struct hif_driver_state_callbacks *hif_get_callbacks_handle(
							struct hif_softc *scn);
bool hif_is_driver_unloading(struct hif_softc *scn);
bool hif_is_load_or_unload_in_progress(struct hif_softc *scn);
bool hif_is_recovery_in_progress(struct hif_softc *scn);
bool hif_is_target_ready(struct hif_softc *scn);

/**
 * hif_get_bandwidth_level() - API to get the current bandwidth level
 * @hif_handle: HIF Context
 *
 * Return: PLD bandwidth level
 */
int hif_get_bandwidth_level(struct hif_opaque_softc *hif_handle);

void hif_wlan_disable(struct hif_softc *scn);
int hif_target_sleep_state_adjust(struct hif_softc *scn,
					 bool sleep_ok,
					 bool wait_for_it);

#ifdef DP_MEM_PRE_ALLOC
void *hif_mem_alloc_consistent_unaligned(struct hif_softc *scn,
					 qdf_size_t size,
					 qdf_dma_addr_t *paddr,
					 uint32_t ring_type,
					 uint8_t *is_mem_prealloc);

void hif_mem_free_consistent_unaligned(struct hif_softc *scn,
				       qdf_size_t size,
				       void *vaddr,
				       qdf_dma_addr_t paddr,
				       qdf_dma_context_t memctx,
				       uint8_t is_mem_prealloc);

/**
 * hif_prealloc_get_multi_pages() - gets pre-alloc DP multi-pages memory
 * @scn: HIF context
 * @desc_type: descriptor type
 * @elem_size: single element size
 * @elem_num: total number of elements should be allocated
 * @pages: multi page information storage
 * @cacheable: coherent memory or cacheable memory
 *
 * Return: None
 */
void hif_prealloc_get_multi_pages(struct hif_softc *scn, uint32_t desc_type,
				  qdf_size_t elem_size, uint16_t elem_num,
				  struct qdf_mem_multi_page_t *pages,
				  bool cacheable);

/**
 * hif_prealloc_put_multi_pages() - puts back pre-alloc DP multi-pages memory
 * @scn: HIF context
 * @desc_type: descriptor type
 * @pages: multi page information storage
 * @cacheable: coherent memory or cacheable memory
 *
 * Return: None
 */
void hif_prealloc_put_multi_pages(struct hif_softc *scn, uint32_t desc_type,
				  struct qdf_mem_multi_page_t *pages,
				  bool cacheable);
#else
static inline
void *hif_mem_alloc_consistent_unaligned(struct hif_softc *scn,
					 qdf_size_t size,
					 qdf_dma_addr_t *paddr,
					 uint32_t ring_type,
					 uint8_t *is_mem_prealloc)
{
	return qdf_mem_alloc_consistent(scn->qdf_dev,
					scn->qdf_dev->dev,
					size,
					paddr);
}

static inline
void hif_mem_free_consistent_unaligned(struct hif_softc *scn,
				       qdf_size_t size,
				       void *vaddr,
				       qdf_dma_addr_t paddr,
				       qdf_dma_context_t memctx,
				       uint8_t is_mem_prealloc)
{
	return qdf_mem_free_consistent(scn->qdf_dev, scn->qdf_dev->dev,
				       size, vaddr, paddr, memctx);
}

static inline
void hif_prealloc_get_multi_pages(struct hif_softc *scn, uint32_t desc_type,
				  qdf_size_t elem_size, uint16_t elem_num,
				  struct qdf_mem_multi_page_t *pages,
				  bool cacheable)
{
	qdf_mem_multi_pages_alloc(scn->qdf_dev, pages,
				  elem_size, elem_num, 0, cacheable);
}

static inline
void hif_prealloc_put_multi_pages(struct hif_softc *scn, uint32_t desc_type,
				  struct qdf_mem_multi_page_t *pages,
				  bool cacheable)
{
	qdf_mem_multi_pages_free(scn->qdf_dev, pages, 0,
				 cacheable);
}
#endif

/**
 * hif_get_rx_ctx_id() - Returns NAPI instance ID based on CE ID
 * @ctx_id: Rx CE context ID
 * @hif_hdl: HIF Context
 *
 * Return: Rx instance ID
 */
int hif_get_rx_ctx_id(int ctx_id, struct hif_opaque_softc *hif_hdl);
void hif_ramdump_handler(struct hif_opaque_softc *scn);
#ifdef HIF_USB
void hif_usb_get_hw_info(struct hif_softc *scn);
void hif_usb_ramdump_handler(struct hif_opaque_softc *scn);
#else
static inline void hif_usb_get_hw_info(struct hif_softc *scn) {}
static inline void hif_usb_ramdump_handler(struct hif_opaque_softc *scn) {}
#endif

/**
 * hif_wake_interrupt_handler() - interrupt handler for standalone wake irq
 * @irq: the irq number that fired
 * @context: the opaque pointer passed to request_irq()
 *
 * Return: an irq return type
 */
irqreturn_t hif_wake_interrupt_handler(int irq, void *context);

#if defined(HIF_SNOC)
bool hif_is_target_register_access_allowed(struct hif_softc *hif_sc);
#elif defined(HIF_IPCI)
static inline bool
hif_is_target_register_access_allowed(struct hif_softc *hif_sc)
{
	return !(hif_sc->recovery);
}
#else
static inline
bool hif_is_target_register_access_allowed(struct hif_softc *hif_sc)
{
	return true;
}
#endif

#ifdef ADRASTEA_RRI_ON_DDR
void hif_uninit_rri_on_ddr(struct hif_softc *scn);
#else
static inline
void hif_uninit_rri_on_ddr(struct hif_softc *scn) {}
#endif

#ifdef FEATURE_RUNTIME_PM
/**
 * hif_runtime_prevent_linkdown() - prevent or allow a runtime pm from occurring
 * @scn: hif context
 * @is_get: prevent linkdown if true otherwise allow
 *
 * this api should only be called as part of bus prevent linkdown
 */
void hif_runtime_prevent_linkdown(struct hif_softc *scn, bool is_get);
#else
static inline
void hif_runtime_prevent_linkdown(struct hif_softc *scn, bool is_get)
{
}
#endif

#ifdef HIF_HAL_REG_ACCESS_SUPPORT
void hif_reg_window_write(struct hif_softc *scn,
			  uint32_t offset, uint32_t value);
uint32_t hif_reg_window_read(struct hif_softc *scn, uint32_t offset);
#endif

#ifdef FEATURE_HIF_DELAYED_REG_WRITE
void hif_delayed_reg_write(struct hif_softc *scn, uint32_t ctrl_addr,
			   uint32_t val);
#endif

#if defined(HIF_IPCI) && defined(FEATURE_HAL_DELAYED_REG_WRITE)
static inline bool hif_is_ep_vote_access_disabled(struct hif_softc *scn)
{
	if ((qdf_atomic_read(&scn->dp_ep_vote_access) ==
	     HIF_EP_VOTE_ACCESS_DISABLE) &&
	    (qdf_atomic_read(&scn->ep_vote_access) ==
	     HIF_EP_VOTE_ACCESS_DISABLE))
		return true;

	return false;
}
#else
static inline bool hif_is_ep_vote_access_disabled(struct hif_softc *scn)
{
	return false;
}
#endif
#endif /* __HIF_MAIN_H__ */
