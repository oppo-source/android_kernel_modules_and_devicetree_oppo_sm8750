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

/**
 * DOC: Declare various struct, macros which are used privately in IPA
 * component.
 */

#ifndef _WLAN_IPA_PRIV_STRUCT_H_
#define _WLAN_IPA_PRIV_STRUCT_H_

#ifdef IPA_OFFLOAD

#include <linux/version.h>
#include <linux/kernel.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)) || \
	defined(CONFIG_IPA_WDI_UNIFIED_API)
#include <qdf_ipa_wdi3.h>
#else
#include <qdf_ipa.h>
#endif

#include <qdf_net_types.h>
#include <qdf_mc_timer.h>
#include <qdf_list.h>
#include <qdf_defer.h>
#include "qdf_delayed_work.h"
#include <qdf_event.h>
#include "wlan_ipa_public_struct.h"
#ifdef IPA_OPT_WIFI_DP
#include "cdp_txrx_ipa.h"
#endif

#define WLAN_IPA_RX_INACTIVITY_MSEC_DELAY   1000
#define WLAN_IPA_UC_WLAN_8023_HDR_SIZE      14

#define WLAN_IPA_UC_NUM_WDI_PIPE            2
#define WLAN_IPA_UC_MAX_PENDING_EVENT       33

#define WLAN_IPA_UC_DEBUG_DUMMY_MEM_SIZE    32000
#define WLAN_IPA_UC_RT_DEBUG_PERIOD         300
#define WLAN_IPA_UC_RT_DEBUG_BUF_COUNT      30
#define WLAN_IPA_UC_RT_DEBUG_FILL_INTERVAL  10000

#define WLAN_IPA_WLAN_HDR_DES_MAC_OFFSET    0
#define WLAN_IPA_MAX_IFACE                  MAX_IPA_IFACE
#define WLAN_IPA_CLIENT_MAX_IFACE           MAX_IPA_IFACE
#define WLAN_IPA_MAX_SYSBAM_PIPE            4

#if defined(IPA_WDS_EASYMESH_FEATURE) || defined(QCA_WIFI_QCN9224)
#define WLAN_IPA_MAX_SESSION                MAX_IPA_IFACE //7
#else
#define WLAN_IPA_MAX_SESSION                5
#endif

#ifdef WLAN_MAX_CLIENTS_ALLOWED
#define WLAN_IPA_MAX_STA_COUNT              WLAN_MAX_CLIENTS_ALLOWED
#else
#define WLAN_IPA_MAX_STA_COUNT              41
#endif

#define WLAN_IPA_RX_PIPE                    (WLAN_IPA_MAX_SYSBAM_PIPE - 1)
#define WLAN_IPA_ENABLE_MASK                BIT(0)
#define WLAN_IPA_PRE_FILTER_ENABLE_MASK     BIT(1)
#define WLAN_IPA_IPV6_ENABLE_MASK           BIT(2)
#define WLAN_IPA_RM_ENABLE_MASK             BIT(3)
#define WLAN_IPA_CLK_SCALING_ENABLE_MASK    BIT(4)
#define WLAN_IPA_UC_ENABLE_MASK             BIT(5)
#define WLAN_IPA_UC_STA_ENABLE_MASK         BIT(6)
#define WLAN_IPA_REAL_TIME_DEBUGGING        BIT(8)
#define WLAN_IPA_OPT_WIFI_DP                BIT(9)
/* With CONFIG_IPA_WDI3_TX_TWO_PIPES=y, this bitmask is added to support
 * runtime IPA two tx pipes feature enablement.
 */
#define WLAN_IPA_TWO_TX_PIPES_ENABLE_MASK    BIT(10)
#define WLAN_IPA_SET_PORT_IN_CCE_CONFIG_MASK BIT(11)
#define WLAN_IPA_LOW_POWER_MODE_ENABLE_MASK  BIT(12)

#ifdef QCA_IPA_LL_TX_FLOW_CONTROL
#define WLAN_IPA_MAX_BANDWIDTH              4800
#define WLAN_IPA_MAX_BANDWIDTH_2G           1400
#else /* !QCA_IPA_LL_TX_FLOW_CONTROL */
#define WLAN_IPA_MAX_BANDWIDTH              800
#endif /* QCA_IPA_LL_TX_FLOW_CONTROL */

#define WLAN_IPA_MAX_PENDING_EVENT_COUNT    20

#define IPA_WLAN_RX_SOFTIRQ_THRESH 32

#define WLAN_IPA_UC_BW_MONITOR_LEVEL        3

/**
 * enum wlan_ipa_uc_op_code - IPA UC operation message
 *
 * @WLAN_IPA_UC_OPCODE_TX_SUSPEND: IPA WDI TX pipe suspend
 * @WLAN_IPA_UC_OPCODE_TX_RESUME: IPA WDI TX pipe resume
 * @WLAN_IPA_UC_OPCODE_RX_SUSPEND: IPA WDI RX pipe suspend
 * @WLAN_IPA_UC_OPCODE_RX_RESUME: IPA WDI RX pipe resume
 * @WLAN_IPA_UC_OPCODE_STATS: IPA UC stats
 * @WLAN_IPA_UC_OPCODE_SHARING_STATS: IPA UC sharing stats
 * @WLAN_IPA_UC_OPCODE_QUOTA_RSP: IPA UC quota response
 * @WLAN_IPA_UC_OPCODE_QUOTA_IND: IPA UC quota indication
 * @WLAN_IPA_UC_OPCODE_UC_READY: IPA UC ready indication
 * @WLAN_IPA_FILTER_RSV_NOTIFY: OPT WIFI DP filter reserve notification
 * @WLAN_IPA_FILTER_REL_NOTIFY: OPT WIFI DP filter release notification
 * @WLAN_IPA_SMMU_MAP: IPA SMMU map call
 * @WLAN_IPA_SMMU_UNMAP: IPA SMMU unmap call
 * @WLAN_IPA_CTRL_TX_REINJECT: REINJECT TO TX
 * @WLAN_IPA_CTRL_FILTER_DEL_NOTIFY: OPT WIFI DP CTRL filter delete notification
 * @WLAN_IPA_CTRL_FILTER_HIGH_TPUT_NOTIFY: OPT WIFI DP CTRL filter
 * delete notification in high TPUT
 * @WLAN_IPA_UC_OPCODE_MAX: IPA UC max operation code
 */
enum wlan_ipa_uc_op_code {
	WLAN_IPA_UC_OPCODE_TX_SUSPEND = 0,
	WLAN_IPA_UC_OPCODE_TX_RESUME = 1,
	WLAN_IPA_UC_OPCODE_RX_SUSPEND = 2,
	WLAN_IPA_UC_OPCODE_RX_RESUME = 3,
	WLAN_IPA_UC_OPCODE_STATS = 4,
#ifdef FEATURE_METERING
	WLAN_IPA_UC_OPCODE_SHARING_STATS = 5,
	WLAN_IPA_UC_OPCODE_QUOTA_RSP = 6,
	WLAN_IPA_UC_OPCODE_QUOTA_IND = 7,
#endif
	WLAN_IPA_UC_OPCODE_UC_READY = 8,
	WLAN_IPA_FILTER_RSV_NOTIFY = 9,
	WLAN_IPA_FILTER_REL_NOTIFY = 10,
	WLAN_IPA_SMMU_MAP = 11,
	WLAN_IPA_SMMU_UNMAP = 12,
	WLAN_IPA_CTRL_TX_REINJECT = 13,
	WLAN_IPA_CTRL_FILTER_DEL_NOTIFY = 14,
	WLAN_IPA_CTRL_FILTER_HIGH_TPUT_NOTIFY = 15,
	/* keep this last */
	WLAN_IPA_UC_OPCODE_MAX
};

/**
 * enum - Reason codes for stat query
 *
 * @WLAN_IPA_UC_STAT_REASON_NONE: Initial value
 * @WLAN_IPA_UC_STAT_REASON_DEBUG: For debug/info
 * @WLAN_IPA_UC_STAT_REASON_BW_CAL: For bandwidth calibration
 */
enum {
	WLAN_IPA_UC_STAT_REASON_NONE,
	WLAN_IPA_UC_STAT_REASON_DEBUG,
	WLAN_IPA_UC_STAT_REASON_BW_CAL
};

/**
 * enum wlan_ipa_rm_state - IPA resource manager state
 * @WLAN_IPA_RM_RELEASED:      PROD pipe resource released
 * @WLAN_IPA_RM_GRANT_PENDING: PROD pipe resource requested but not granted yet
 * @WLAN_IPA_RM_GRANTED:       PROD pipe resource granted
 */
enum wlan_ipa_rm_state {
	WLAN_IPA_RM_RELEASED,
	WLAN_IPA_RM_GRANT_PENDING,
	WLAN_IPA_RM_GRANTED,
};

/**
 * enum wlan_ipa_forward_type: Type of forward packet received from IPA
 * @WLAN_IPA_FORWARD_PKT_NONE: No forward packet
 * @WLAN_IPA_FORWARD_PKT_LOCAL_STACK: Packet forwarded to kernel network stack
 * @WLAN_IPA_FORWARD_PKT_DISCARD: Discarded packet before sending to kernel
 */
enum wlan_ipa_forward_type {
	WLAN_IPA_FORWARD_PKT_NONE = 0,
	WLAN_IPA_FORWARD_PKT_LOCAL_STACK = 1,
	WLAN_IPA_FORWARD_PKT_DISCARD = 2
};

/**
 * enum wlan_ipa_ctrl_flt_del_src: OPT_DP_CTRL flt del request src
 * @WLAN_IPA_CTRL_FLT_DEL_SRC_IPA: flt del requested from ipa
 * @WLAN_IPA_CTRL_FLT_DEL_SRC_SHUTDOWN: flt del triggered from shutdown
 */
enum wlan_ipa_ctrl_flt_del_src {
	WLAN_IPA_CTRL_FLT_DEL_SRC_IPA = 0,
	WLAN_IPA_CTRL_FLT_DEL_SRC_SHUTDOWN = 1
};

/**
 * enum wlan_ipa_wdi_opt_dpath_resp_code: flt deletion return code
 * in opt_dp_ctrl
 * @WLAN_IPA_WDI_OPT_DPATH_RESP_SUCCESS: flt del success
 * @WLAN_IPA_WDI_OPT_DPATH_RESP_ERR_FAILURE: flt del failure in FW
 * @WLAN_IPA_WDI_OPT_DPATH_RESP_ERR_INTERNAL: flt hdl invalid
 * @WLAN_IPA_WDI_OPT_DPATH_RESP_ERR_TIMEOUT: flt del timed out
 * @WLAN_IPA_WDI_OPT_DPATH_RESP_SUCCESS_HIGH_TPUT: high tput deletion
 * @WLAN_IPA_WDI_OPT_DPATH_RESP_SUCCESS_SHUTDOWN: flt del due to wlan shutdown
 * @WLAN_IPA_WDI_OPT_DPATH_RESP_SUCCESS_SSR: flt del due to wlan ssr
 * @WLAN_IPA_WDI_OPT_DPATH_RESP_MAX: Max return code for flt del
 *
 */
enum wlan_ipa_wdi_opt_dpath_resp_code {
	WLAN_IPA_WDI_OPT_DPATH_RESP_SUCCESS = 0,
	WLAN_IPA_WDI_OPT_DPATH_RESP_ERR_FAILURE = 200,
	WLAN_IPA_WDI_OPT_DPATH_RESP_ERR_INTERNAL = 201,
	WLAN_IPA_WDI_OPT_DPATH_RESP_ERR_TIMEOUT = 202,
	WLAN_IPA_WDI_OPT_DPATH_RESP_SUCCESS_HIGH_TPUT = 203,
	WLAN_IPA_WDI_OPT_DPATH_RESP_SUCCESS_SHUTDOWN = 204,
	WLAN_IPA_WDI_OPT_DPATH_RESP_SUCCESS_SSR = 205,
	WLAN_IPA_WDI_OPT_DPATH_RESP_MAX
};

/**
 * enum wlan_ipa_opt_dp_ctrl_add_resp: filter add response
 * in opt_dp_ctrl
 * @WLAN_IPA_CTRL_FLT_ADD_INPROGRESS: flt add inprogress
 * @WLAN_IPA_CTRL_FLT_ADD_SUCCESS: flt add success
 * @WLAN_IPA_CTRL_FLT_ADD_FAILURE: flt add failure
 */
enum wlan_ipa_opt_dp_ctrl_add_resp {
	WLAN_IPA_CTRL_FLT_ADD_INPROGRESS = 0,
	WLAN_IPA_CTRL_FLT_ADD_SUCCESS = 1,
	WLAN_IPA_CTRL_FLT_ADD_FAILURE = 2
};

/**
 * struct llc_snap_hdr - LLC snap header
 * @dsap: Destination service access point
 * @ssap: Source service access point
 * @resv: Reserved for future use
 * @eth_type: Ether type
 */
struct llc_snap_hdr {
	uint8_t dsap;
	uint8_t ssap;
	uint8_t resv[4];
	qdf_be16_t eth_type;
} qdf_packed;

/**
 * struct wlan_ipa_tx_hdr - header type which IPA should handle to TX packet
 * @eth:      ether II header
 * @llc_snapp: LLC snap header
 */
struct wlan_ipa_tx_hdr {
	qdf_ether_header_t eth;
	struct llc_snap_hdr llc_snapp;
} qdf_packed;

#if defined(QCA_WIFI_QCA6290) || defined(QCA_WIFI_QCA6390) || \
    defined(QCA_WIFI_QCA6490) || defined(QCA_WIFI_QCA6750) || \
    defined(QCA_WIFI_WCN7850) || defined(QCA_WIFI_QCN9000) || \
    defined(QCA_WIFI_KIWI) || defined(QCA_WIFI_KIWI_V2) || \
    defined(QCA_WIFI_WCN7750) || defined(QCA_WIFI_QCC2072)
/**
 * struct frag_header - fragment header type registered to IPA hardware
 * @length:    fragment length
 * @reserved: Reserved not used
 */
struct frag_header {
	__QDF_DECLARE_FLEX_ARRAY(uint8_t, reserved);
};
#elif defined(QCA_WIFI_3_0)
/**
 * struct frag_header - fragment header type registered to IPA hardware
 * @length:    fragment length
 * @reserved1: Reserved not used
 * @reserved2: Reserved not used
 */
struct frag_header {
	uint16_t length;
	uint32_t reserved1;
	uint32_t reserved2;
} qdf_packed;
#else
/**
 * struct frag_header - fragment header type registered to IPA hardware
 * @length:    fragment length
 * @reserved16: Reserved not used
 * @reserved2: Reserved not used
 */
struct frag_header {
	uint32_t
		length:16,
		reserved16:16;
	uint32_t reserved2;
} qdf_packed;
#endif

#if defined(QCA_WIFI_QCA6290) || defined(QCA_WIFI_QCA6390) || \
    defined(QCA_WIFI_QCA6490) || defined(QCA_WIFI_QCA6750) || \
    defined(QCA_WIFI_WCN7850) || defined(QCA_WIFI_QCN9000) || \
    defined(QCA_WIFI_KIWI) || defined(QCA_WIFI_KIWI_V2) || \
    defined(QCA_WIFI_WCN7750) || defined(QCA_WIFI_QCC2072)
/**
 * struct ipa_header - ipa header type registered to IPA hardware
 * @reserved: Reserved not used
 */
struct ipa_header {
	__QDF_DECLARE_FLEX_ARRAY(uint8_t, reserved);
};
#else
/**
 * struct ipa_header - ipa header type registered to IPA hardware
 * @vdev_id:  vdev id
 * @reserved: Reserved not used
 */
struct ipa_header {
	uint32_t
		vdev_id:8,	/* vdev_id field is LSB of IPA DESC */
		reserved:24;
} qdf_packed;
#endif

/**
 * struct wlan_ipa_uc_tx_hdr - full tx header registered to IPA hardware
 * @frag_hd: fragment header
 * @ipa_hd:  ipa header
 * @eth:     ether II header
 */
struct wlan_ipa_uc_tx_hdr {
	struct frag_header frag_hd;
	struct ipa_header ipa_hd;
	qdf_ether_header_t eth;
} qdf_packed;

/**
 * struct wlan_ipa_cld_hdr - IPA CLD Header
 * @reserved: reserved fields
 * @iface_id: interface ID
 * @sta_id: Station ID
 *
 * Packed 32-bit structure
 * +----------+----------+--------------+--------+
 * | Reserved | QCMAP ID | interface id | STA ID |
 * +----------+----------+--------------+--------+
 */
struct wlan_ipa_cld_hdr {
	uint8_t reserved[2];
	uint8_t iface_id;
	uint8_t sta_id;
} qdf_packed;

/**
 * struct wlan_ipa_rx_hdr - IPA RX header
 * @cld_hdr: IPA CLD header
 * @eth:     ether II header
 */
struct wlan_ipa_rx_hdr {
	struct wlan_ipa_cld_hdr cld_hdr;
	qdf_ether_header_t eth;
} qdf_packed;

/**
 * struct wlan_ipa_pm_tx_cb - PM resume TX callback
 * @exception: Exception packet
 * @iface_context: Interface context
 * @ipa_tx_desc: IPA TX descriptor
 * @send_to_nw: RX exception packet that needs to be passed up to stack
 */
struct wlan_ipa_pm_tx_cb {
	bool exception;
	struct wlan_ipa_iface_context *iface_context;
	qdf_ipa_rx_data_t *ipa_tx_desc;
	bool send_to_nw;
};

/**
 * struct wlan_ipa_sys_pipe - IPA system pipe
 * @conn_hdl: IPA system pipe connection handle
 * @conn_hdl_valid: IPA system pipe valid flag
 * @ipa_sys_params: IPA system pipe params
 */
struct wlan_ipa_sys_pipe {
	uint32_t conn_hdl;
	uint8_t conn_hdl_valid;
	qdf_ipa_sys_connect_params_t ipa_sys_params;
};

/**
 * struct wlan_ipa_iface_stats - IPA system pipe
 * @num_tx: Number of TX packets
 * @num_tx_drop: Number of TX packet drops
 * @num_tx_err: Number of TX packet errors
 * @num_tx_cac_drop: Number of TX packet drop due to CAC
 * @num_rx_ipa_excep: Number of RX IPA exception packets
 */
struct wlan_ipa_iface_stats {
	uint64_t num_tx;
	uint64_t num_tx_drop;
	uint64_t num_tx_err;
	uint64_t num_tx_cac_drop;
	uint64_t num_rx_ipa_excep;
};

/* IPA private context structure */
struct wlan_ipa_priv;

/**
 * struct wlan_ipa_iface_context - IPA interface context
 * @ipa_ctx: IPA private context
 * @cons_client: IPA consumer pipe
 * @prod_client: IPA producer pipe
 * @iface_id: IPA interface ID
 * @dev: Net device structure
 * @device_mode: Interface device mode
 * @mac_addr: MAC address
 * @conn_count: Connect count
 * @disconn_count: Disconnect count
 * @session_id: Session ID
 * @interface_lock: Interface lock
 * @ifa_address: Interface address
 * @stats: Interface stats
 * @bssid: BSSID. valid only for sta iface ctx
 * @is_authenticated: is peer authenticated
 * @alt_pipe: Indicate whether the interface uses alternate TX pipe
 */
struct wlan_ipa_iface_context {
	struct wlan_ipa_priv *ipa_ctx;

	qdf_ipa_client_type_t cons_client;
	qdf_ipa_client_type_t prod_client;

	uint8_t iface_id;       /* This iface ID */
	qdf_netdev_t dev;
	enum QDF_OPMODE device_mode;
	uint8_t mac_addr[QDF_MAC_ADDR_SIZE];
	qdf_atomic_t conn_count;
	qdf_atomic_t disconn_count;
	uint8_t session_id;
	qdf_spinlock_t interface_lock;
	uint32_t ifa_address;
	struct wlan_ipa_iface_stats stats;
	struct qdf_mac_addr bssid;
	uint8_t is_authenticated;
#ifdef IPA_WDI3_TX_TWO_PIPES
	bool alt_pipe;
#endif
};

/**
 * struct wlan_ipa_stats - IPA system stats
 * @event: WLAN IPA event record
 * @num_send_msg: Number of sent IPA messages
 * @num_free_msg: Number of freed IPA messages
 * @num_rm_grant: Number of times IPA RM resource granted
 * @num_rm_release: Number of times IPA RM resource released
 * @num_rm_grant_imm: Number of immediate IPA RM granted
 * @num_cons_perf_req: Number of CONS pipe perf request
 * @num_prod_perf_req: Number of PROD pipe perf request
 * @num_rx_drop: Number of RX packet drops
 * @num_tx_desc_q_cnt: Number of TX descriptor queue count
 * @num_tx_desc_error: Number of TX descriptor error
 * @num_tx_comp_cnt: Number of TX qdf_event_t count
 * @num_tx_queued: Number of TX queued
 * @num_tx_dequeued: Number of TX dequeued
 * @num_max_pm_queue: Number of packets in PM queue
 * @num_rx_excep: Number of RX IPA exception packets
 * @num_rx_no_iface_eapol: No of EAPOL pkts before iface setup
 * @num_tx_fwd_ok: Number of TX forward packet success
 * @num_tx_fwd_err: Number of TX forward packet failures
 */
struct wlan_ipa_stats {
	uint32_t event[QDF_IPA_WLAN_EVENT_MAX];
	uint64_t num_send_msg;
	uint64_t num_free_msg;
	uint64_t num_rm_grant;
	uint64_t num_rm_release;
	uint64_t num_rm_grant_imm;
	uint64_t num_cons_perf_req;
	uint64_t num_prod_perf_req;
	uint64_t num_rx_drop;
	uint64_t num_tx_desc_q_cnt;
	uint64_t num_tx_desc_error;
	uint64_t num_tx_comp_cnt;
	uint64_t num_tx_queued;
	uint64_t num_tx_dequeued;
	uint64_t num_max_pm_queue;
	uint64_t num_rx_excep;
	uint64_t num_rx_no_iface_eapol;
	uint64_t num_tx_fwd_ok;
	uint64_t num_tx_fwd_err;
};

/**
 * struct ipa_uc_stas_map - IPA UC assoc station map
 * @is_reserved: STA reserved flag
 * @is_authenticated: is peer authenticated
 * @mac_addr: Station mac address
 */
struct ipa_uc_stas_map {
	bool is_reserved;
	struct qdf_mac_addr mac_addr;
	uint8_t is_authenticated;
};

/**
 * struct op_msg_type - IPA operation message type
 * @msg_t: Message type
 * @rsvd: Reserved
 * @op_code: IPA Operation type
 * @len: IPA message length
 * @rsvd_snd: Reserved
 * @vdev_id: vdev id
 * @nbuf: tx nbuf
 * @ctrl_del_hdl: flt handle deleted in opt_dp_ctrl
 */
struct op_msg_type {
	uint8_t msg_t;
	uint8_t rsvd;
	uint16_t op_code;
	uint16_t len;
	uint16_t rsvd_snd;
	uint8_t vdev_id;
	qdf_nbuf_t nbuf;
	uint32_t ctrl_del_hdl;
};

/**
 * struct ipa_uc_fw_stats - IPA FW stats
 * @tx_comp_ring_base: TX completion ring base address
 * @tx_comp_ring_size: TX completion ring size
 * @tx_comp_ring_dbell_addr: TX completion ring door bell address
 * @tx_comp_ring_dbell_ind_val: TX completion ring door bell indication
 * @tx_comp_ring_dbell_cached_val: TX completion ring cached value
 * @tx_pkts_enqueued: TX packets enqueued
 * @tx_pkts_completed: TX packets completed
 * @tx_is_suspend: TX suspend flag
 * @tx_reserved: Reserved for TX stat
 * @rx_ind_ring_base: RX indication ring base address
 * @rx_ind_ring_size: RX indication ring size
 * @rx_ind_ring_dbell_addr: RX indication ring doorbell address
 * @rx_ind_ring_dbell_ind_val: RX indication ring doorbell indication
 * @rx_ind_ring_dbell_ind_cached_val: RX indication ring doorbell cached value
 * @rx_ind_ring_rdidx_addr: RX indication ring read index address
 * @rx_ind_ring_rd_idx_cached_val: RX indication ring read index cached value
 * @rx_refill_idx: RX ring refill index
 * @rx_num_pkts_indicated: Number of RX packets indicated
 * @rx_buf_refilled: Number of RX buffer refilled
 * @rx_num_ind_drop_no_space: Number of RX indication drops due to no space
 * @rx_num_ind_drop_no_buf: Number of RX indication drops due to no buffer
 * @rx_is_suspend: RX suspend flag
 * @rx_reserved: Reserved for RX stat
 */
struct ipa_uc_fw_stats {
	uint32_t tx_comp_ring_base;
	uint32_t tx_comp_ring_size;
	uint32_t tx_comp_ring_dbell_addr;
	uint32_t tx_comp_ring_dbell_ind_val;
	uint32_t tx_comp_ring_dbell_cached_val;
	uint32_t tx_pkts_enqueued;
	uint32_t tx_pkts_completed;
	uint32_t tx_is_suspend;
	uint32_t tx_reserved;
	uint32_t rx_ind_ring_base;
	uint32_t rx_ind_ring_size;
	uint32_t rx_ind_ring_dbell_addr;
	uint32_t rx_ind_ring_dbell_ind_val;
	uint32_t rx_ind_ring_dbell_ind_cached_val;
	uint32_t rx_ind_ring_rdidx_addr;
	uint32_t rx_ind_ring_rd_idx_cached_val;
	uint32_t rx_refill_idx;
	uint32_t rx_num_pkts_indicated;
	uint32_t rx_buf_refilled;
	uint32_t rx_num_ind_drop_no_space;
	uint32_t rx_num_ind_drop_no_buf;
	uint32_t rx_is_suspend;
	uint32_t rx_reserved;
};

/**
 * struct wlan_ipa_uc_pending_event - WLAN IPA UC pending event
 * @node: Pending event list node
 * @type: WLAN IPA event type
 * @net_dev: network device
 * @device_mode: Device mode
 * @session_id: Session ID
 * @mac_addr: Mac address
 * @is_loading: Driver loading flag
 * @is_2g_iface: true if interface is operating on 2G band, otherwise false
 */
struct wlan_ipa_uc_pending_event {
	qdf_list_node_t node;
	qdf_ipa_wlan_event type;
	qdf_netdev_t net_dev;
	uint8_t device_mode;
	uint8_t session_id;
	uint8_t mac_addr[QDF_MAC_ADDR_SIZE];
	bool is_loading;
	bool is_2g_iface;
};

/**
 * struct uc_rm_work_struct
 * @work: uC RM work
 * @event: IPA RM event
 */
struct uc_rm_work_struct {
	qdf_work_t work;
	qdf_ipa_rm_event_t event;
};

/**
 * struct msg_elem
 * @vdev_id: vdev id
 * @nbuf: nbuf
 * @op_code: IPA Operation type
 * @hdl: handle of filter deleted
 * @result: result of deletion
 */
struct msg_elem {
	uint8_t vdev_id;
	qdf_nbuf_t nbuf;
	uint16_t op_code;
	uint32_t hdl;
	uint16_t result;
};

/**
 * struct op_msg_list
 * @hp: hp of list
 * @tp: tp of list
 * @entries: list of messages
 * @list_size: max list size
 * @lock: spin lock for list
 */
struct op_msg_list {
	uint16_t hp;
	uint16_t tp;
	struct msg_elem *entries;
	uint16_t list_size;
	qdf_spinlock_t lock;
};

/**
 * struct uc_op_work_struct
 * @work: uC OP work
 * @msg: OP message
 * @osdev: pointer to qdf net device, used by osif_psoc_sync_trans_start_wait
 * @ipa_priv_bp: back pointer to ipa_obj
 * @msg_list: list of messages, to be used in case of parallel msgs
 * @flag: flag to be set when msg list is required
 */
struct uc_op_work_struct {
	qdf_work_t work;
	struct op_msg_type *msg;
	qdf_device_t osdev;
	struct wlan_ipa_priv *ipa_priv_bp;
	struct op_msg_list *msg_list;
	uint16_t flag;
};

/**
 * struct uc_rt_debug_info
 * @time: system time
 * @ipa_excep_count: IPA exception packet count
 * @rx_drop_count: IPA Rx drop packet count
 * @net_sent_count: IPA Rx packet sent to network stack count
 * @rx_discard_count: IPA Rx discard packet count
 * @tx_fwd_ok_count: IPA Tx forward success packet count
 * @tx_fwd_count: IPA Tx forward packet count
 * @rx_destructor_call: IPA Rx packet destructor count
 */
struct uc_rt_debug_info {
	uint64_t time;
	uint64_t ipa_excep_count;
	uint64_t rx_drop_count;
	uint64_t net_sent_count;
	uint64_t rx_discard_count;
	uint64_t tx_fwd_ok_count;
	uint64_t tx_fwd_count;
	uint64_t rx_destructor_call;
};

#ifdef FEATURE_METERING
/**
 * struct ipa_uc_sharing_stats - IPA UC sharing stats
 * @ipv4_rx_packets: IPv4 RX packets
 * @ipv4_rx_bytes: IPv4 RX bytes
 * @ipv6_rx_packets: IPv6 RX packets
 * @ipv6_rx_bytes: IPv6 RX bytes
 * @ipv4_tx_packets: IPv4 TX packets
 * @ipv4_tx_bytes: IPv4 TX bytes
 * @ipv6_tx_packets: IPv4 TX packets
 * @ipv6_tx_bytes: IPv6 TX bytes
 */
struct ipa_uc_sharing_stats {
	uint64_t ipv4_rx_packets;
	uint64_t ipv4_rx_bytes;
	uint64_t ipv6_rx_packets;
	uint64_t ipv6_rx_bytes;
	uint64_t ipv4_tx_packets;
	uint64_t ipv4_tx_bytes;
	uint64_t ipv6_tx_packets;
	uint64_t ipv6_tx_bytes;
};

/**
 * struct ipa_uc_quota_rsp - IPA UC quota response
 * @success: Success or fail flag
 * @reserved: Reserved
 * @quota_lo: Quota limit low bytes
 * @quota_hi: Quota limit high bytes
 */
struct ipa_uc_quota_rsp {
	uint8_t success;
	uint8_t reserved[3];
	uint32_t quota_lo;
	uint32_t quota_hi;
};

/**
 * struct ipa_uc_quota_ind
 * @quota_bytes: Quota bytes to set
 */
struct ipa_uc_quota_ind {
	uint64_t quota_bytes;
};
#endif

/**
 * struct wlan_ipa_tx_desc
 * @node: TX descriptor node
 * @priv: pointer to priv list entry
 * @id: Tx desc idex
 * @ipa_tx_desc_ptr: pointer to IPA Tx descriptor
 */
struct wlan_ipa_tx_desc {
	qdf_list_node_t node;
	void *priv;
	uint32_t id;
	qdf_ipa_rx_data_t *ipa_tx_desc_ptr;
};

typedef QDF_STATUS (*wlan_ipa_softap_xmit)(qdf_nbuf_t nbuf, qdf_netdev_t dev);
typedef void (*wlan_ipa_send_to_nw)(qdf_nbuf_t nbuf, qdf_netdev_t dev);
typedef bool (*wlan_ipa_driver_unloading)(void);

/**
 * typedef wlan_ipa_rps_enable - Enable/disable RPS for adapter using vdev id
 * @vdev_id: vdev_id of adapter
 * @enable: Set true to enable RPS
 */
typedef void (*wlan_ipa_rps_enable)(uint8_t vdev_id, bool enable);

#if defined(IPA_OFFLOAD) && defined(QCA_IPA_LL_TX_FLOW_CONTROL)
/**
 * struct wlan_ipa_evt_wq_args - IPA Workqueue arguments
 * @pdev_obj:           Pdev object
 * @net_dev:            Network Device
 * @vdev:               Vdev object
 * @device_mode:        Device mode type
 * @ch_freq:            Channel frequency
 * @vdev_id:            Vdev Id
 * @mac_addr:           Peer Mac Address
 * @event:              IPA wlan event
 * @list_elem:          WQ list elem
 */
struct wlan_ipa_evt_wq_args {
	struct wlan_objmgr_pdev *pdev_obj;
	struct net_device *net_dev;
	struct wlan_objmgr_vdev *vdev;
	enum QDF_OPMODE device_mode;
	uint16_t ch_freq;
	uint8_t vdev_id;
	u_int8_t mac_addr[QDF_MAC_ADDR_SIZE]; /* MAC address */
	enum wlan_ipa_wlan_event event;

	TAILQ_ENTRY(wlan_ipa_evt_wq_args) list_elem;
};

/*
 * NB: not using kernel-doc format since the kernel-doc script doesn't
 *     handle the TAILQ_HEAD() macro
 *
 * struct wlan_ipa_evt_wq - IPA Workqueue structure
 * @work:               Instance of work
 * @work_queue:         Wrapper around the real task func
 * @list_lock:          Lock on WQ
 * @list:               Queue of IPA event
 */
struct wlan_ipa_evt_wq {
	qdf_work_t work;
	qdf_workqueue_t *work_queue;
	qdf_spinlock_t list_lock;

	TAILQ_HEAD(, wlan_ipa_evt_wq_args) list;
};
#endif

/**
 * struct opt_dp_ctrl_stats - stats for opt_dp_ctrl
 * @flt_add_req_cnt: cnt of filter add requested by ipa
 * @flt_rm_req_cnt: cnt of filters rm requested by ipa
 * @active_filter: total active filters
 * @add_fail_cnt: cnt of filter add failed
 * @rm_fail_cnt: cnt of filter rm failed
 * @clk_resp_cnt: cnt of clock vote response received
 * @clk_vote_cnt: cnt of clock vote
 * @clk_unvote_req_cnt: cnt of clock unvote
 * @tput_del_cnt: cnt of filters deleted due to high tput
 * @reinject_pkt_enq_fail_cnt: cnt of pkts failed to enqueue
 * in WQ after reinjection
 */
struct opt_dp_ctrl_stats {
	int flt_add_req_cnt;
	int flt_rm_req_cnt;
	int active_filter;
	int add_fail_cnt;
	int rm_fail_cnt;
	int clk_resp_cnt;
	int clk_vote_cnt;
	int clk_unvote_req_cnt;
	int tput_del_cnt;
	int reinject_pkt_enq_fail_cnt;
};

/* IPA private context structure definition */
struct wlan_ipa_priv {
	struct wlan_objmgr_psoc *psoc;
	struct wlan_ipa_sys_pipe sys_pipe[WLAN_IPA_MAX_SYSBAM_PIPE];
	struct wlan_ipa_iface_context iface_context[WLAN_IPA_MAX_IFACE];
	uint8_t num_iface;
	void *dp_soc;
	struct wlan_ipa_config *config;
	enum wlan_ipa_rm_state rm_state;
	/*
	 * IPA driver can send RM notifications with IRQ disabled so using qdf
	 * APIs as it is taken care gracefully. Without this, kernel would throw
	 * an warning if spin_lock_bh is used while IRQ is disabled
	 */
	qdf_spinlock_t rm_lock;
	struct uc_rm_work_struct uc_rm_work;
	struct uc_op_work_struct uc_op_work[WLAN_IPA_UC_OPCODE_MAX];
	qdf_wake_lock_t wake_lock;
	struct qdf_delayed_work wake_lock_work;
	bool wake_lock_released;

	qdf_atomic_t tx_ref_cnt;
	qdf_nbuf_queue_t pm_queue_head;
	qdf_work_t pm_work;
	qdf_spinlock_t pm_lock;
	bool suspended;
	qdf_spinlock_t q_lock;
	qdf_spinlock_t enable_disable_lock;
	/* Flag to indicate wait on pending TX completions */
	qdf_atomic_t waiting_on_pending_tx;
	/* Timer ticks to keep track of time after which pipes are disabled */
	uint64_t pending_tx_start_ticks;
	/* Indicates if cdp_ipa_disable_autonomy is called for IPA pipes */
	qdf_atomic_t autonomy_disabled;
	/* Indicates if cdp_disable_ipa_pipes has been called for IPA pipes */
	qdf_atomic_t pipes_disabled;
	/*
	 * IPA pipes are considered "down" when both autonomy_disabled and
	 * ipa_pipes_disabled are set
	 */
	bool ipa_pipes_down;
	/* Flag for mutual exclusion during IPA disable pipes */
	bool pipes_down_in_progress;
	/* Flag for mutual exclusion during IPA enable pipes */
	bool pipes_enable_in_progress;
	qdf_list_node_t pend_desc_head;
	struct wlan_ipa_tx_desc *tx_desc_pool;
	qdf_list_t tx_desc_free_list;

	struct wlan_ipa_stats stats;

	uint32_t curr_prod_bw;
	uint32_t curr_cons_bw;

	uint8_t activated_fw_pipe;
	uint8_t num_sap_connected;
	uint16_t sap_num_connected_sta;
	uint16_t sap_num_mlo_connected_sta;
	uint8_t sta_connected;
	uint32_t tx_pipe_handle;
	uint32_t rx_pipe_handle;
	bool resource_loading;
	bool resource_unloading;
	bool pending_cons_req;
	struct ipa_uc_stas_map assoc_stas_map[WLAN_IPA_MAX_STA_COUNT];
	qdf_list_t pending_event;
	qdf_mutex_t event_lock;
	uint32_t ipa_tx_packets_diff;
	uint32_t ipa_rx_packets_diff;
	uint32_t ipa_p_tx_packets;
	uint32_t ipa_p_rx_packets;
	uint32_t stat_req_reason;
	uint64_t ipa_tx_forward;
	uint64_t ipa_rx_discard;
	uint64_t ipa_rx_net_send_count;
	uint64_t ipa_rx_internal_drop_count;
	uint64_t ipa_rx_destructor_count;
	qdf_mc_timer_t rt_debug_timer;
	struct uc_rt_debug_info rt_bug_buffer[WLAN_IPA_UC_RT_DEBUG_BUF_COUNT];
	unsigned int rt_buf_fill_index;
	qdf_ipa_wdi_in_params_t cons_pipe_in;
	qdf_ipa_wdi_in_params_t prod_pipe_in;
	bool uc_loaded;
	bool wdi_enabled;
	bool over_gsi;
	qdf_mc_timer_t rt_debug_fill_timer;
	qdf_mutex_t rt_debug_lock;
	qdf_mutex_t ipa_lock;

	uint8_t vdev_to_iface[WLAN_IPA_MAX_SESSION];
	bool vdev_offload_enabled[WLAN_IPA_MAX_SESSION];
	bool mcc_mode;
	qdf_work_t mcc_work;
	bool disable_intrabss_fwd[WLAN_IPA_MAX_SESSION];
	bool dfs_cac_block_tx;
#ifdef FEATURE_METERING
	struct ipa_uc_sharing_stats ipa_sharing_stats;
	struct ipa_uc_quota_rsp ipa_quota_rsp;
	struct ipa_uc_quota_ind ipa_quota_ind;
	qdf_event_t ipa_uc_sharing_stats_comp;
	qdf_event_t ipa_uc_set_quota_comp;
#endif

	wlan_ipa_softap_xmit softap_xmit;
	wlan_ipa_send_to_nw send_to_nw;

#if defined(QCA_CONFIG_RPS) && !defined(MDM_PLATFORM)
	/*Callback to enable RPS for STA in STA+SAP scenario*/
	wlan_ipa_rps_enable rps_enable;
#endif
	wlan_ipa_driver_unloading driver_is_unloading;
	qdf_event_t ipa_resource_comp;

	uint32_t wdi_version;
	bool is_smmu_enabled;	/* IPA caps returned from ipa_wdi_init */
	/* Flag to notify whether optional wifi dp feature is enabled or not */
	bool opt_wifi_datapath;
	bool opt_dp_active;
	bool opt_wifi_datapath_ctrl;
	bool fw_cap_opt_dp_ctrl;
	qdf_atomic_t stats_quota;
	uint8_t curr_bw_level;
	qdf_atomic_t deinit_in_prog;
	uint8_t instance_id;
	bool handle_initialized;
	qdf_ipa_wdi_hdl_t hdl;
	bool opt_dp_ctrl_wlan_shutdown;
	bool opt_dp_ctrl_ssr;
	bool opt_dp_ctrl_flt_cleaned;
	qdf_event_t ipa_ctrl_flt_rm_shutdown_evt;
	bool ipa_opt_dp_ctrl_debug;
#ifdef IPA_OPT_WIFI_DP
	struct wifi_dp_flt_setup dp_cce_super_rule_flt_param;
	struct wifi_dp_tx_flt_setup dp_tx_super_rule_flt_param;
	qdf_event_t ipa_flt_evnt;
	qdf_event_t ipa_ctrl_flt_evnt;
	qdf_event_t ipa_opt_dp_ctrl_clk_evt;
	qdf_wake_lock_t opt_dp_wake_lock;
	struct opt_dp_ctrl_stats ctrl_stats;
	qdf_runtime_lock_t opt_dp_runtime_lock;
#endif
#if defined(QCA_IPA_LL_TX_FLOW_CONTROL)
	struct wlan_ipa_evt_wq *ipa_evt_wq;
#endif
};

#define WLAN_IPA_WLAN_FRAG_HEADER        sizeof(struct frag_header)
#define WLAN_IPA_WLAN_IPA_HEADER         sizeof(struct ipa_header)
#define WLAN_IPA_WLAN_CLD_HDR_LEN        sizeof(struct wlan_ipa_cld_hdr)
#define WLAN_IPA_UC_WLAN_CLD_HDR_LEN     0
#define WLAN_IPA_WLAN_TX_HDR_LEN         sizeof(struct wlan_ipa_tx_hdr)
#define WLAN_IPA_UC_WLAN_TX_HDR_LEN      sizeof(struct wlan_ipa_uc_tx_hdr)
#define WLAN_IPA_WLAN_RX_HDR_LEN         sizeof(struct wlan_ipa_rx_hdr)
#define WLAN_IPA_UC_WLAN_HDR_DES_MAC_OFFSET \
	(WLAN_IPA_WLAN_FRAG_HEADER + WLAN_IPA_WLAN_IPA_HEADER)

#define WLAN_IPA_GET_IFACE_ID(_data) \
	(((struct wlan_ipa_cld_hdr *) (_data))->iface_id)

#define WLAN_IPA_LOG(LVL, fmt, args ...) \
	QDF_TRACE(QDF_MODULE_ID_IPA, LVL, \
		  "%s:%d: "fmt, __func__, __LINE__, ## args)

#define WLAN_IPA_IS_CONFIG_ENABLED(_ipa_cfg, _mask) \
	(((_ipa_cfg)->ipa_config & (_mask)) == (_mask))

#define BW_GET_DIFF(_x, _y) (unsigned long)((ULONG_MAX - (_y)) + (_x) + 1)

#define IPA_RESOURCE_COMP_WAIT_TIME	500

#ifdef FEATURE_METERING
#define IPA_UC_SHARING_STATES_WAIT_TIME	500
#define IPA_UC_SET_QUOTA_WAIT_TIME	500
#endif

/**
 * wlan_ipa_wlan_event_to_str() - convert IPA WLAN event to string
 * @event: IPA WLAN event to be converted to a string
 *
 * Return: ASCII string representing the IPA WLAN event
 */
static inline char *wlan_ipa_wlan_event_to_str(qdf_ipa_wlan_event event)
{
	switch (event) {
	CASE_RETURN_STRING(QDF_IPA_CLIENT_CONNECT);
	CASE_RETURN_STRING(QDF_IPA_CLIENT_DISCONNECT);
	CASE_RETURN_STRING(QDF_IPA_AP_CONNECT);
	CASE_RETURN_STRING(QDF_IPA_AP_DISCONNECT);
	CASE_RETURN_STRING(QDF_IPA_STA_CONNECT);
	CASE_RETURN_STRING(QDF_IPA_STA_DISCONNECT);
	CASE_RETURN_STRING(QDF_IPA_CLIENT_CONNECT_EX);
	CASE_RETURN_STRING(QDF_IPA_MLO_CLIENT_CONNECT_EX);
	CASE_RETURN_STRING(QDF_IPA_MLO_CLIENT_DISCONNECT);
	CASE_RETURN_STRING(QDF_SWITCH_TO_SCC);
	CASE_RETURN_STRING(QDF_SWITCH_TO_MCC);
	CASE_RETURN_STRING(QDF_WDI_ENABLE);
	CASE_RETURN_STRING(QDF_WDI_DISABLE);
	default:
		return "UNKNOWN";
	}
}

/**
 * wlan_ipa_get_iface() - Get IPA interface
 * @ipa_ctx: IPA context
 * @mode: Interface device mode
 *
 * Return: IPA interface address
 */
struct wlan_ipa_iface_context
*wlan_ipa_get_iface(struct wlan_ipa_priv *ipa_ctx, uint8_t mode);

#endif /* IPA_OFFLOAD */
#endif /* _WLAN_IPA_PRIV_STRUCT_H_ */
