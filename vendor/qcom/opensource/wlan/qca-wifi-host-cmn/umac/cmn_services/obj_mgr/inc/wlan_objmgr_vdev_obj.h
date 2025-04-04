/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
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
 * DOC: Define the vdev data structure of UMAC
 */

#ifndef _WLAN_OBJMGR_VDEV_OBJ_H_
#define _WLAN_OBJMGR_VDEV_OBJ_H_

#include "qdf_atomic.h"
#include "qdf_list.h"
#include "qdf_lock.h"
#include "qdf_types.h"
#include "wlan_cmn.h"
#include "wlan_objmgr_cmn.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_vdev_mlme_main.h"
#include "include/wlan_vdev_mlme.h"
#include "wlan_vdev_mlme_api.h"
#include "wlan_mlme_dbg.h"
#ifdef WLAN_FEATURE_11BE_MLO
#include "wlan_mlo_mgr_public_structs.h"
#endif

	/* CONF: privacy enabled */
#define WLAN_VDEV_F_PRIVACY              0x00000001
	/* CONF: 11g w/o 11b sta's */
#define WLAN_VDEV_F_PUREG                0x00000002
	/* CONF: des_bssid is set */
#define WLAN_VDEV_F_DESBSSID             0x00000004
	/* CONF: bg scan enabled */
#define WLAN_VDEV_F_BGSCAN               0x00000008
	/* CONF: sw tx retry enabled */
#define WLAN_VDEV_F_SWRETRY              0x00000010
	/* STATUS: update beacon tim */
#define WLAN_VDEV_F_TIMUPDATE            0x00000020
	/* CONF: WPA enabled */
#define WLAN_VDEV_F_WPA1                 0x00000040
	/* CONF: WPA2 enabled */
#define WLAN_VDEV_F_WPA2                 0x00000080
	/* CONF: WPA/WPA2 enabled */
#define WLAN_VDEV_F_WPA                  0x000000c0
	/* CONF: drop unencrypted */
#define WLAN_VDEV_F_DROPUNENC            0x00000100
	/* CONF: TKIP countermeasures */
#define WLAN_VDEV_F_COUNTERM             0x00000200
	/* CONF: hide SSID in beacon */  /*TODO PDEV/PSOC */
#define WLAN_VDEV_F_HIDESSID             0x00000400
	/* CONF: disable internal bridge */ /*TODO PDEV/PSOC */
#define WLAN_VDEV_F_NOBRIDGE             0x00000800
	/* STATUS: update beacon wme */
#define WLAN_VDEV_F_WMEUPDATE            0x00001000
	/* CONF: enable U-APSD */
#define WLAN_VDEV_F_UAPSD                0x00004000
	/* STATUS: sleeping */
#define WLAN_VDEV_F_SLEEP                0x00008000
	/* drop uapsd EOSP frames for test */
#define WLAN_VDEV_F_EOSPDROP             0x00010000
	/* CONF: A-MPDU supported */
#define WLAN_VDEV_F_AMPDU                0x00020000
	/* STATE: beacon APP IE updated */
#define WLAN_VDEV_F_APPIE_UPDATE         0x00040000
	/* CONF: WDS auto Detect/DELBA */
#define WLAN_VDEV_F_WDS_AUTODETECT       0x00080000
	/* 11b only without 11g stations */
#define WLAN_VDEV_F_PUREB                0x00100000
	/* disable HT rates */
#define WLAN_VDEV_F_HTRATES              0x00200000
	/* CONF: deliver rx frames with 802.11 header */
#define WLAN_VDEV_F_DELIVER_80211        0x00800000
	/* CONF: os sends down tx frames with 802.11 header */
#define WLAN_VDEV_F_SEND_80211           0x01000000
	/* CONF: statically configured WDS */
#define WLAN_VDEV_F_WDS_STATIC           0x02000000
	/* CONF: pure 11n mode */
#define WLAN_VDEV_F_PURE11N              0x04000000
	/* CONF: pure 11ac mode */
#define WLAN_VDEV_F_PURE11AC             0x08000000
	/* Basic Rates Update */
#define WLAN_VDEV_F_BR_UPDATE            0x10000000
	/* CONF: restrict bw ont top of per 11ac/n */
#define WLAN_VDEV_F_STRICT_BW            0x20000000
	/* Wi-Fi SON mode (with APS) */
#define WLAN_VDEV_F_SON                  0x40000000
	/* Wi-Fi SON mode (with APS) */
#define WLAN_VDEV_F_MBO                  0x80000000

/* Feature extension flags */
		/* CONF: MSFT safe mode         */
#define WLAN_VDEV_FEXT_SAFEMODE             0x00000001
		/* if the vap can sleep*/
#define WLAN_VDEV_FEXT_CANSLEEP             0x00000002
		/* use sw bmiss timer */
#define WLAN_VDEV_FEXT_SWBMISS              0x00000004
		/* enable beacon copy */
#define WLAN_VDEV_FEXT_COPY_BEACON          0x00000008
#define WLAN_VDEV_FEXT_WAPI                 0x00000010
		/* 802.11h enabled */
#define WLAN_VDEV_FEXT_DOTH                 0x00000020
	/* if the vap has wds independence set */
#define WLAN_VDEV_FEXT_VAPIND               0x00000040
	/* QBSS load IE enabled */
#define WLAN_VDEV_FEXT_BSSLOAD              0x00000080
	/* Short Guard Interval Enable:1 Disable:0 */
#define WLAN_VDEV_FEXT_SGI                  0x00000100
	/* Short Guard Interval Enable:1 Disable:0 for VHT fixed rates */
#define WLAN_VDEV_FEXT_DATASGI              0x00000200
	/* LDPC Enable Rx:1 TX: 2 ; Disable:0 */
#define WLAN_VDEV_FEXT_LDPC_TX              0x00000400
#define WLAN_VDEV_FEXT_LDPC_RX              0x00000800
#define WLAN_VDEV_FEXT_LDPC                 0x00000c00
	/* wme enabled */
#define WLAN_VDEV_FEXT_WME                  0x00001000
	/* WNM Capabilities */
#define WLAN_VDEV_FEXT_WNM                  0x00002000
	/* RRM Capabilities */
#define WLAN_VDEV_FEXT_RRM                  0x00004000
	/* WNM Proxy ARP Capabilities */
#define WLAN_VDEV_FEXT_PROXYARP             0x00008000
	/* 256 QAM support in 2.4GHz mode Enable:1 Disable:0 */
#define WLAN_VDEV_FEXT_256QAM               0x00010000
	/* 2.4NG 256 QAM Interop mode Enable:1 Disable:0 */
#define WLAN_VDEV_FEXT_256QAM_INTEROP       0x00020000
	/* static mimo ps enabled */
#define WLAN_VDEV_FEXT_STATIC_MIMOPS        0x00040000
	/* dynamic mimo ps enabled */
#define WLAN_VDEV_FEXT_DYN_MIMOPS           0x00080000
	/* Country IE enabled */
#define WLAN_VDEV_FEXT_CNTRY_IE             0x00100000
	/*does not want to trigger multi channel operation
	instead follow master vaps channel (for AP/GO Vaps) */
#define WLAN_VDEV_FEXT_NO_MULCHAN           0x00200000
	/*non-beaconing AP VAP*/
#define WLAN_VDEV_FEXT_NON_BEACON           0x00400000
	/* SPL repeater enabled for SON*/
#define WLAN_VDEV_FEXT_SON_SPL_RPT          0x00800000
	/* SON IE update in MGMT frame */
#define WLAN_VDEV_FEXT_SON_INFO_UPDATE      0x01000000
	/* CONF: A-MSDU supported */
#define WLAN_VDEV_FEXT_AMSDU                0x02000000
	/* Fils discovery on 6G SAP*/
#define WLAN_VDEV_FEXT_FILS_DISC_6G_SAP     0x80000000

/* Feature more extension flags */
	/* VDEV is MLO*/
#define WLAN_VDEV_FEXT2_MLO                 0x00000001
	/* STA VDEV is link type */
#define WLAN_VDEV_FEXT2_MLO_STA_LINK        0x00000002
	/* VDEV is MLO mcast primary*/
#define WLAN_VDEV_FEXT2_MLO_MCAST           0x00000004
	/* 20TU BCAST PROBE RESP on 6G SAP*/
#define WLAN_VDEV_FEXT2_20TU_PRB_RESP       0x00000008
	/* STA VDEV is TDLS link type */
#define WLAN_VDEV_FEXT2_MLO_STA_TDLS        0x00000010

/* VDEV OP flags  */
  /* if the vap destroyed by user */
#define WLAN_VDEV_OP_DELETE_PROGRESS        0x00000001
 /* set to enable sta-fws fweature */
#define WLAN_VDEV_OP_STAFWD                 0x00000002
   /* Off-channel support enabled */
#define WLAN_VDEV_OP_OFFCHAN                0x00000004
  /* if the vap has erp update set */
#define WLAN_VDEV_OP_ERPUPDATE              0x00000008
  /* this vap needs scheduler for off channel operation */
#define WLAN_VDEV_OP_NEEDS_SCHED            0x00000010
  /*STA in forced sleep set PS bit for all outgoing frames */
#define WLAN_VDEV_OP_FORCED_SLEEP           0x00000020
  /* update bssload IE in beacon */
#define WLAN_VDEV_OP_BSSLOAD_UPDATE         0x00000040
  /* Hotspot 2.0 DGAF Disable bit */
#define WLAN_VDEV_OP_DGAF_DISABLE           0x00000080
  /* STA SmartNet enabled */
#define WLAN_VDEV_OP_SMARTNET_EN            0x00000100
  /* SoftAP to reject resuming in DFS channels */
#define WLAN_VDEV_OP_REJ_DFS_CHAN           0x00000200
  /* Trigger mlme response */
#define WLAN_VDEV_OP_TRIGGER_MLME_RESP      0x00000400
  /* test flag for MFP */
#define WLAN_VDEV_OP_MFP_TEST               0x00000800
  /* flag to indicate using default ratemask */
#define WLAN_VDEV_OP_DEF_RATEMASK           0x00001000
/*For wakeup AP VAP when wds-sta connect to the AP only use when
	export (UMAC_REPEATER_DELAYED_BRINGUP || DBDC_REPEATER_SUPPORT)=1*/
#define WLAN_VDEV_OP_KEYFLAG                0x00002000
  /* if performed the iwlist scanning */
#define WLAN_VDEV_OP_LIST_SCANNING          0x00004000
   /*Set when VAP down*/
#define WLAN_VDEV_OP_IS_DOWN                0x00008000
  /* if vap may require acs when another vap is brought down */
#define WLAN_VDEV_OP_NEEDS_UP_ACS           0x00010000
  /* Block data traffic tx for this vap */
#define WLAN_VDEV_OP_BLOCK_TX_TRAFFIC       0x00020000
  /* for mbo functionality */
#define WLAN_VDEV_OP_MBO                    0x00040000
/* VDEV Critical update category-1
 * Inclusion of Critical Update IES flag
 * This includes: CSA, ECSA, Quiet
 * Quiet channel, Max Ch Switch Time IEs.
 */
#define WLAN_VDEV_OP_CU_CAT1                0x00080000
/* VDEV Critical update category-2
 * Modification of Critical Update IES flag
 * This includes: DSSS Param, HT Operation element
 * VHT Operation element, HE Operation element
 * EHT Operation element, MU EDCA Param, EDCA param
 * UORA Param, BSS Color Change Announcement element
 * Spatial Reuse Param Set element
 * Operating Mode Notification element
 * Wide Bandwidth Channel Switch element
 * Broadcast TWT element
 */
#define WLAN_VDEV_OP_CU_CAT2                0x00100000
  /* for mlo reconfig link removal functionality */
#define WLAN_VDEV_OP_MLO_STOP_LINK_DEL      0x00200000
  /* for mlo reconfig link add functionality */
#define WLAN_VDEV_OP_MLO_LINK_ADD           0x00400000
  /* for mlo reconfig link removal TBTT complete */
#define WLAN_VDEV_OP_MLO_LINK_TBTT_COMPLETE 0x00800000

/* MLO link removal is in progress on this VDEV */
#define WLAN_VDEV_OP_MLO_LINK_REMOVAL_IN_PROGRESS 0x01000000
/* MLO link switch is in progress on this VDEV */
#define WLAN_VDEV_OP_MLO_LINK_SWITCH_IN_PROGRESS 0x02000000
 /* MLO link rejection handling is in progress on this VDEV */
#define WLAN_VDEV_OP_MLO_LINK_REJECTION_IN_PROGRESS 0x04000000

 /* flag to indicate disconnect only legacy peers due to moving to DFS channel
  * from non-DFS channel
  */
#define WLAN_VDEV_OP_MLME_LEGACY_PEER_DISCON_TRIG 0x02000000
  /* for mlo reconfig link removal functionality */
#define WLAN_VDEV_OP_MLO_REMOVE_LINK_VDEV      0x04000000

 /* CAPABILITY: IBSS available */
#define WLAN_VDEV_C_IBSS                    0x00000001
/* CAPABILITY: HOSTAP avail */
#define WLAN_VDEV_C_HOSTAP               0x00000002
   /* CAPABILITY: Old Adhoc Demo */
#define WLAN_VDEV_C_AHDEMO               0x00000004
  /* CAPABILITY: sw tx retry */
#define WLAN_VDEV_C_SWRETRY              0x00000008
  /* CAPABILITY: monitor mode */
#define WLAN_VDEV_C_MONITOR              0x00000010
  /* CAPABILITY: TKIP MIC avail */
#define WLAN_VDEV_C_TKIPMIC              0x00000020
  /* CAPABILITY: 4-addr support */
#define WLAN_VDEV_C_WDS                  0x00000040
  /* CAPABILITY: TKIP MIC for QoS frame */
#define WLAN_VDEV_C_WME_TKIPMIC          0x00000080
  /* CAPABILITY: bg scanning */
#define WLAN_VDEV_C_BGSCAN               0x00000100
  /* CAPABILITY: Restrict offchannel */
#define WLAN_VDEV_C_RESTRICT_OFFCHAN     0x00000200
  /* CAPABILITY: eMLSR capability */
#define WLAN_VDEV_C_EMLSR_CAP            0x00000400
  /* CAPABILITY: Exclude per sta profile in unicast Probe req */
#define WLAN_VDEV_C_EXCL_STA_PROF_PRB_REQ   0x00000800

/* Invalid VDEV identifier */
#define WLAN_INVALID_VDEV_ID 255

/* Invalid VDEV link id*/
#define WLAN_INVALID_LINK_ID 255

/**
 * struct wlan_vdev_create_params - Create params, HDD/OSIF passes this
 *				    structure While creating VDEV
 * @opmode:         Opmode of VDEV
 * @flags:          create flags
 * @size_vdev_priv: Size of vdev private
 * @legacy_osif:    Legacy os_if private member
 * @macaddr:        MAC address
 * @mataddr:        MAT address
 * @mldaddr:        MLD address
 * @mlo_sap_sync_disable:  flag for disable mlo sap sync between vdevs
 */
struct wlan_vdev_create_params {
	enum QDF_OPMODE opmode;
	uint32_t flags;
	size_t size_vdev_priv;
	void *legacy_osif;
	uint8_t macaddr[QDF_MAC_ADDR_SIZE];
	uint8_t mataddr[QDF_MAC_ADDR_SIZE];
	uint8_t mldaddr[QDF_MAC_ADDR_SIZE];
	bool mlo_sap_sync_disable;
};

/**
 * struct wlan_channel - channel structure
 * @ch_freq:      Channel in Mhz.
 * @ch_ieee:      IEEE channel number.
 * @ch_freq_seg1: Channel Center frequency for VHT80/160 and HE80/160.
 * @ch_freq_seg2: Second channel Center frequency applicable for 80+80MHz mode.
 * @ch_maxpower:  Maximum tx power in dBm.
 * @ch_flagext:   Channel extension flags.
 * @ch_flags:     Channel flags.
 * @ch_cfreq1:    channel center frequency for primary
 * @ch_cfreq2:    channel center frequency for secondary
 * @ch_width:     Channel width.
 * @ch_phymode:   Channel phymode.
 * @puncture_bitmap:   Puncture bitmap per 20MHz.
 */
struct wlan_channel {
	uint16_t     ch_freq;
	uint8_t      ch_ieee;
	uint8_t      ch_freq_seg1;
	uint8_t      ch_freq_seg2;
	int8_t       ch_maxpower;
	uint16_t     ch_flagext;
	uint64_t     ch_flags;
	uint32_t     ch_cfreq1;
	uint32_t     ch_cfreq2;
	enum phy_ch_width ch_width;
	enum wlan_phymode ch_phymode;
#ifdef WLAN_FEATURE_11BE
	uint16_t     puncture_bitmap;
#endif
};

/**
 * struct wlan_objmgr_vdev_mlme - VDEV MLME specific sub structure
 * @vdev_opmode:        Opmode of VDEV
 * @mlme_state:         VDEV MLME SM state
 * @mlme_substate:      VDEV MLME SM substate
 * @bss_chan:           BSS channel
 * @des_chan:           Desired channel, for STA Desired may not be used
 * @vdev_caps:          VDEV capabilities
 * @vdev_feat_caps:     VDEV feature caps
 * @vdev_feat_ext_caps: VDEV Extended feature caps
 * @vdev_feat_ext2_caps: More VDEV Extended feature caps
 * @vdev_op_flags:      Operation flags
 * @mataddr:            MAT address
 * @macaddr:            Contains link MAC address for ML connection and
 *                      net dev address for non-ML connection
 * @mldaddr:            MLD address
 * @linkaddr:           Link MAC address
 * @epcs_enable:        EPCS enable flag
 * @mlo_link_id: link id for mlo connection
 * @mlo_external_sae_auth: MLO external SAE auth
 * @user_disable_eht: user disable eht for IOT issues
 * @wlan_vdev_mlo_lock: lock to protect the set/clear of
 * @skip_pumac_cnt: Counter to skip vdev to be selected as pumac
 * WLAN_VDEV_FEXT2_MLO feature flag in vdev MLME
 * @mlo_sap_sync_disable: flag to disable mlo sap vdev sync
 */
struct wlan_objmgr_vdev_mlme {
	enum QDF_OPMODE vdev_opmode;
	enum wlan_vdev_state mlme_state;
	enum wlan_vdev_state mlme_substate;
	struct wlan_channel *bss_chan;
	struct wlan_channel *des_chan;
	uint32_t vdev_caps;
	uint32_t vdev_feat_caps;
	uint32_t vdev_feat_ext_caps;
	uint32_t vdev_feat_ext2_caps;
	uint32_t vdev_op_flags;
	uint8_t  mataddr[QDF_MAC_ADDR_SIZE];
	uint8_t  macaddr[QDF_MAC_ADDR_SIZE];
	uint8_t  mldaddr[QDF_MAC_ADDR_SIZE];
	uint8_t  linkaddr[QDF_MAC_ADDR_SIZE];
#ifdef WLAN_FEATURE_11BE_MLO
	bool epcs_enable;
	uint8_t  mlo_link_id;
	bool mlo_external_sae_auth;
	bool user_disable_eht;
#ifdef WLAN_MLO_USE_SPINLOCK
	qdf_spinlock_t wlan_vdev_mlo_lock;
#else
	qdf_mutex_t wlan_vdev_mlo_lock;
#endif
#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
	qdf_atomic_t skip_pumac_cnt;
#endif
#ifdef WLAN_FEATURE_MULTI_LINK_SAP
	bool mlo_sap_sync_disable;
#endif
#endif

};

/**
 *  struct wlan_objmgr_vdev_nif - VDEV HDD specific sub structure
 *  @osdev:  OS specific pointer
 */
struct wlan_objmgr_vdev_nif {
	struct vdev_osif_priv *osdev;
};

/**
 *  struct wlan_objmgr_vdev_objmgr - vdev object manager sub structure
 *  @vdev_id:            VDEV id
 *  @print_cnt:          Count to throttle Logical delete prints
 *  @self_peer:          Self PEER
 *  @bss_peer:           BSS PEER
 *  @wlan_peer_list:     PEER list
 *  @wlan_pdev:          PDEV pointer
 *  @wlan_peer_count:    Peer count
 *  @wlan_ml_peer_count: Multilink Peer count
 *  @mlo_bridge_vdev:    Indicates it is bridge VDEV
 *  @mlo_central_vdev:   Indicates it is central VDEV
 *  @max_peer_count:     Max Peer count
 *  @c_flags:            creation specific flags
 *  @ref_cnt:            Ref count
 *  @ref_id_dbg:         Array to track Ref count
 *  @trace:              Trace ref and deref
 */
struct wlan_objmgr_vdev_objmgr {
	uint8_t vdev_id;
	uint8_t print_cnt;
	struct wlan_objmgr_peer *self_peer;
	struct wlan_objmgr_peer *bss_peer;
	qdf_list_t wlan_peer_list;
	struct wlan_objmgr_pdev *wlan_pdev;
	uint16_t wlan_peer_count;
#ifdef WLAN_FEATURE_11BE_MLO
	qdf_atomic_t wlan_ml_peer_count;
#ifdef WLAN_MLO_MULTI_CHIP
	bool mlo_bridge_vdev;
	bool mlo_central_vdev;
#endif
#endif
	uint16_t max_peer_count;
	uint32_t c_flags;
	qdf_atomic_t ref_cnt;
	qdf_atomic_t ref_id_dbg[WLAN_REF_ID_MAX];
#ifdef WLAN_OBJMGR_REF_ID_TRACE
	struct wlan_objmgr_trace trace;
#endif
};

/**
 * struct wlan_objmgr_vdev - VDEV common object
 * @vdev_node:      qdf list of pdev's vdev list
 * @vdev_mlme:      VDEV MLME substructure
 * @vdev_objmgr:    VDEV Object Mgr substructure
 * @vdev_nif:       VDEV HDD substructure
 * @vdev_comp_priv_obj:Component's private objects list
 * @obj_status:     Component object status
 * @obj_state:      VDEV object state
 * @vdev_lock:      VDEV lock
 * @mlo_dev_ctx:    MLO device context
 * @twt_work:	    TWT work
 * @is_ap_suspend:	AP suspend state
 */
struct wlan_objmgr_vdev {
	qdf_list_node_t vdev_node;
	struct wlan_objmgr_vdev_mlme vdev_mlme;
	struct wlan_objmgr_vdev_objmgr vdev_objmgr;
	struct wlan_objmgr_vdev_nif vdev_nif;
	void *vdev_comp_priv_obj[WLAN_UMAC_MAX_COMPONENTS];
	QDF_STATUS obj_status[WLAN_UMAC_MAX_COMPONENTS];
	WLAN_OBJ_STATE obj_state;
	qdf_spinlock_t vdev_lock;
#ifdef WLAN_FEATURE_11BE_MLO
	struct wlan_mlo_dev_context *mlo_dev_ctx;
#endif
#ifdef WLAN_SUPPORT_TWT
	qdf_work_t twt_work;
#endif
	qdf_atomic_t is_ap_suspend;
};

/*
 * APIs to Create/Delete Global object APIs
 */
/**
 * wlan_objmgr_vdev_obj_create() - vdev object create
 * @pdev: PDEV object on which this vdev gets created
 * @params: VDEV create params from HDD
 *
 * Creates vdev object, initializes with default values
 * Attaches to psoc and pdev objects
 * Invokes the registered notifiers to create component object
 *
 * Return: Handle to struct wlan_objmgr_vdev on successful creation,
 *         NULL on Failure (on Mem alloc failure and Component objects
 *         Failure)
 */
struct wlan_objmgr_vdev *wlan_objmgr_vdev_obj_create(
			struct wlan_objmgr_pdev *pdev,
			struct wlan_vdev_create_params *params);

/**
 * wlan_objmgr_vdev_obj_delete() - vdev object delete
 * @vdev: vdev object
 *
 * Logically deletes VDEV object,
 * Once all the references are released, object manager invokes the registered
 * notifiers to destroy component objects
 *
 * Return: SUCCESS/FAILURE
 */
QDF_STATUS wlan_objmgr_vdev_obj_delete(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_objmgr_vdev_mlo_dev_ctxt_attach() - vdev mlo ctxt object attach
 * @vdev: vdev object
 *
 * Attach vdev to mlo dev context object.
 *
 * Return: SUCCESS/FAILURE
 */
QDF_STATUS
wlan_objmgr_vdev_mlo_dev_ctxt_attach(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_objmgr_vdev_mlo_dev_ctxt_detach() - vdev mlo ctxt object delete
 * @vdev: vdev object
 *
 * Detach mlo dev context object from vdev.
 *
 * Return: SUCCESS/FAILURE
 */
QDF_STATUS
wlan_objmgr_vdev_mlo_dev_ctxt_detach(struct wlan_objmgr_vdev *vdev);

/*
 * APIs to attach/detach component objects
 */
/**
 * wlan_objmgr_vdev_component_obj_attach() - vdev comp object attach
 * @vdev: VDEV object
 * @id: Component id
 * @comp_priv_obj: component's private object pointer
 * @status: Component's private object creation status
 *
 * API to be used for attaching component object with VDEV common object
 *
 * Return: SUCCESS on successful storing of component's object in common object
 *         On FAILURE (appropriate failure codes are returned)
 */
QDF_STATUS wlan_objmgr_vdev_component_obj_attach(
		struct wlan_objmgr_vdev *vdev,
		enum wlan_umac_comp_id id,
		void *comp_priv_obj,
		QDF_STATUS status);

/**
 * wlan_objmgr_vdev_component_obj_detach() - vdev comp object detach
 * @vdev: VDEV object
 * @id: Component id
 * @comp_priv_obj: component's private object pointer
 *
 * API to be used for detaching component object with VDEV common object
 *
 * Return: SUCCESS on successful removal of component's object from common
 *         object
 *         On FAILURE (appropriate failure codes are returned)
 */
QDF_STATUS wlan_objmgr_vdev_component_obj_detach(
		struct wlan_objmgr_vdev *vdev,
		enum wlan_umac_comp_id id,
		void *comp_priv_obj);

/*
 * APIs to operations on vdev objects
 */

typedef void (*wlan_objmgr_vdev_op_handler)(struct wlan_objmgr_vdev *vdev,
					void *object,
					void *arg);

/**
 * wlan_objmgr_iterate_peerobj_list() - iterate vdev's peer list
 * @vdev: vdev object
 * @handler: the handler will be called for each object of requested type
 *            the handler should be implemented to perform required operation
 * @arg:     arguments passed by caller
 * @dbg_id: id of the caller
 *
 * API to be used for performing the operations on all PEER objects
 * of vdev
 *
 * Return: SUCCESS/FAILURE
 */
QDF_STATUS wlan_objmgr_iterate_peerobj_list(
		struct wlan_objmgr_vdev *vdev,
		wlan_objmgr_vdev_op_handler handler,
		void *arg, wlan_objmgr_ref_dbgid dbg_id);

/**
 * wlan_objmgr_vdev_get_log_del_peer_list() - vdev logically deleted peer list
 * @vdev: vdev object
 * @dbg_id: id of the caller
 *
 * API to be used for populating the list of logically deleted peers from the
 * vdev's peer list
 *
 * The caller of this function should free the memory allocated for the
 * peerlist and the peer member in the list
 * Also the peer ref release is handled by the caller
 *
 * Return: list of peer pointers
 *         NULL on FAILURE
 */
qdf_list_t *wlan_objmgr_vdev_get_log_del_peer_list(
		struct wlan_objmgr_vdev *vdev,
		wlan_objmgr_ref_dbgid dbg_id);

/**
 * wlan_objmgr_trigger_vdev_comp_priv_object_creation() - vdev
 * comp object creation
 * @vdev: VDEV object
 * @id: Component id
 *
 * API to create component private object in run time, this would
 * be used for features which gets enabled in run time
 *
 * Return: SUCCESS on successful creation
 *         On FAILURE (appropriate failure codes are returned)
 */
QDF_STATUS wlan_objmgr_trigger_vdev_comp_priv_object_creation(
		struct wlan_objmgr_vdev *vdev,
		enum wlan_umac_comp_id id);

/**
 * wlan_objmgr_trigger_vdev_comp_priv_object_deletion() - vdev comp
 *                                                        object deletion
 * @vdev: VDEV object
 * @id: Component id
 *
 * API to destroy component private object in run time, this would
 * be used for features which gets disabled in run time
 *
 * Return: SUCCESS on successful deletion
 *         On FAILURE (appropriate failure codes are returned)
 */
QDF_STATUS wlan_objmgr_trigger_vdev_comp_priv_object_deletion(
		struct wlan_objmgr_vdev *vdev,
		enum wlan_umac_comp_id id);

/**
 * wlan_objmgr_vdev_get_comp_private_obj() - get vdev component private object
 * @vdev: VDEV object
 * @id: Component id
 *
 * API to get component private object
 *
 * Return: void *ptr on SUCCESS
 *         NULL on Failure
 */
void *wlan_objmgr_vdev_get_comp_private_obj(
		struct wlan_objmgr_vdev *vdev,
		enum wlan_umac_comp_id id);

/* Util APIs */

/**
 * wlan_vdev_get_pdev() - get pdev
 * @vdev: VDEV object
 *
 * API to get pdev object pointer from vdev
 *
 * Return: pdev object pointer
 */
static inline struct wlan_objmgr_pdev *wlan_vdev_get_pdev(
				struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_objmgr.wlan_pdev;
}

/**
 * wlan_pdev_vdev_list_peek_head() - get first vdev from pdev list
 * @vdev_list: qdf_list_t
 *
 * API to get the head vdev of given vdev (of pdev's vdev list)
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: head peer
 */
static inline struct wlan_objmgr_vdev *wlan_pdev_vdev_list_peek_head(
					qdf_list_t *vdev_list)
{
	struct wlan_objmgr_vdev *vdev;
	qdf_list_node_t *vdev_node = NULL;

	/* This API is invoked with lock acquired, do not add log prints */
	if (qdf_list_peek_front(vdev_list, &vdev_node) != QDF_STATUS_SUCCESS)
		return NULL;

	vdev = qdf_container_of(vdev_node, struct wlan_objmgr_vdev, vdev_node);
	return vdev;
}


/**
 * wlan_vdev_get_next_vdev_of_pdev() - get next vdev
 * @vdev_list: qdf_list_t
 * @vdev: VDEV object
 *
 * API to get next vdev object pointer of vdev
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: VDEV object
 */
static inline struct wlan_objmgr_vdev *wlan_vdev_get_next_vdev_of_pdev(
					qdf_list_t *vdev_list,
					struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_vdev *vdev_next;
	qdf_list_node_t *node = &vdev->vdev_node;
	qdf_list_node_t *next_node = NULL;

	/* This API is invoked with lock acquired, do not add log prints */
	if (!node)
		return NULL;

	if (qdf_list_peek_next(vdev_list, node, &next_node) !=
						QDF_STATUS_SUCCESS)
		return NULL;

	vdev_next = qdf_container_of(next_node, struct wlan_objmgr_vdev,
				vdev_node);
	return vdev_next;
}



/**
 * wlan_vdev_set_pdev() - set pdev
 * @vdev: VDEV object
 * @pdev: PDEV object
 *
 * API to get pdev object pointer from vdev
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: void
 */
static inline void wlan_vdev_set_pdev(struct wlan_objmgr_vdev *vdev,
					struct wlan_objmgr_pdev *pdev)
{
	/* This API is invoked with lock acquired, do not add log prints */
	vdev->vdev_objmgr.wlan_pdev = pdev;
}

/**
 * wlan_vdev_get_psoc() - get psoc
 * @vdev: VDEV object
 *
 * API to get pdev object pointer from vdev
 *
 * Return: psoc object pointer
 */
static inline struct wlan_objmgr_psoc *wlan_vdev_get_psoc(
				struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc = NULL;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return NULL;

	psoc = wlan_pdev_get_psoc(pdev);

	return psoc;
}

/**
 * wlan_vdev_get_psoc_id() - get psoc id
 * @vdev: VDEV object
 *
 * API to get VDEV's psoc id
 *
 * Return: psoc id
 */
static inline uint8_t wlan_vdev_get_psoc_id(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_vdev_get_psoc(vdev);

	return wlan_psoc_get_id(psoc);
}

/**
 * wlan_vdev_skip_pumac() - get primary umac support
 * @vdev: VDEV object
 *
 * API to get Primary umac support for MLO
 *
 * Return: get primary umac support (bool)
 */
static inline bool wlan_vdev_skip_pumac(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_vdev_get_psoc(vdev);

	if (wlan_psoc_get_pumac_skip(psoc))
		return true;

	return false;
}

/**
 * wlan_vdev_mlme_set_opmode() - set vdev opmode
 * @vdev: VDEV object
 * @mode: VDEV op mode
 *
 * API to set opmode in vdev object
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_opmode(struct wlan_objmgr_vdev *vdev,
				enum QDF_OPMODE mode)
{
	vdev->vdev_mlme.vdev_opmode = mode;
}

/**
 * wlan_vdev_mlme_get_opmode() - get vdev opmode
 * @vdev: VDEV object
 *
 * API to set opmode of vdev object
 *
 * Return: VDEV op mode
 */
static inline enum QDF_OPMODE wlan_vdev_mlme_get_opmode(
					struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_mlme.vdev_opmode;
}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_FEATURE_MULTI_LINK_SAP)
/**
 * wlan_vdev_mlme_set_mlo_sap_sync_disable() - set mlo sap sync disable flag
 * @vdev: VDEV object
 * @value: mlo_ap_sync_disable
 *
 * API to disable vdev sync of multi link sap
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_mlo_sap_sync_disable(
				struct wlan_objmgr_vdev *vdev,
				bool value)
{
	vdev->vdev_mlme.mlo_sap_sync_disable = value;
}

/**
 * wlan_vdev_mlme_get_mlo_sap_sync_disable() - get mlo sap sync disable flag
 * @vdev: VDEV object
 *
 * API to get if vdev sync of multi link sap is disabled.
 *
 * Return: bool
 */
static inline bool
wlan_vdev_mlme_get_mlo_sap_sync_disable(struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_mlme.mlo_sap_sync_disable;
}
#else
static inline void wlan_vdev_mlme_set_mlo_sap_sync_disable(
				struct wlan_objmgr_vdev *vdev,
				bool value)
{}

static inline bool
wlan_vdev_mlme_get_mlo_sap_sync_disable(struct wlan_objmgr_vdev *vdev)
{
	return false;
}
#endif

/**
 * wlan_vdev_mlme_set_macaddr() - set vdev macaddr
 * @vdev: VDEV object
 * @macaddr: MAC address
 *
 * API to set macaddr in vdev object
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_macaddr(struct wlan_objmgr_vdev *vdev,
					 uint8_t *macaddr)
{
	/* This API is invoked with lock acquired, do not add log prints */
	WLAN_ADDR_COPY(vdev->vdev_mlme.macaddr, macaddr);
}

/**
 * wlan_vdev_mlme_get_macaddr() - get vdev macaddr
 * @vdev: VDEV object
 *
 * API to get MAC address from vdev object
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: MAC address
 */
static inline uint8_t *wlan_vdev_mlme_get_macaddr(struct wlan_objmgr_vdev *vdev)
{
	/* This API is invoked with lock acquired, do not add log prints */
	return vdev->vdev_mlme.macaddr;
}

/**
 * wlan_vdev_mlme_set_mataddr() - set vdev mataddr
 * @vdev: VDEV object
 * @mataddr: MAT address
 *
 * API to set mataddr in vdev object
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_mataddr(struct wlan_objmgr_vdev *vdev,
					uint8_t *mataddr)
{
	/* This API is invoked with lock acquired, do not add log prints */
	WLAN_ADDR_COPY(vdev->vdev_mlme.mataddr, mataddr);
}

/**
 * wlan_vdev_mlme_get_mldaddr() - get vdev mldaddr
 * @vdev: VDEV object
 *
 * API to get MLD address from vdev object
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: MAC address
 */
static inline uint8_t *wlan_vdev_mlme_get_mldaddr(struct wlan_objmgr_vdev *vdev)
{
	/* This API is invoked with lock acquired, do not add log prints */
	return vdev->vdev_mlme.mldaddr;
}

/**
 * wlan_vdev_mlme_set_mldaddr() - set vdev mldaddr
 * @vdev: VDEV object
 * @mldaddr: MLD address
 *
 * API to set MLD addr in vdev object
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_mldaddr(struct wlan_objmgr_vdev *vdev,
					uint8_t *mldaddr)
{
	/* This API is invoked with lock acquired, do not add log prints */
	WLAN_ADDR_COPY(vdev->vdev_mlme.mldaddr, mldaddr);
}

/**
 * wlan_vdev_mlme_reset_mldaddr() - clear vdev mldaddr
 * @vdev: VDEV object
 *
 * API to clear MLD addr in vdev object
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_reset_mldaddr(struct wlan_objmgr_vdev *vdev)
{
	/* This API is invoked with lock acquired, do not add log prints */
	qdf_zero_macaddr((struct qdf_mac_addr *)vdev->vdev_mlme.mldaddr);
}

/**
 * wlan_vdev_mlme_get_linkaddr() - get vdev linkaddr
 * @vdev: VDEV object
 *
 * API to get link MAC address from vdev object
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: Link MAC address
 */
static inline
uint8_t *wlan_vdev_mlme_get_linkaddr(struct wlan_objmgr_vdev *vdev)
{
	/* This API is invoked with lock acquired, do not add log prints */
	return vdev->vdev_mlme.linkaddr;
}

/**
 * wlan_vdev_mlme_set_linkaddr() - set vdev linkaddr
 * @vdev: VDEV object
 * @linkaddr: Link address
 *
 * API to set link addr in vdev object
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_linkaddr(struct wlan_objmgr_vdev *vdev,
					       uint8_t *linkaddr)
{
	/* This API is invoked with lock acquired, do not add log prints */
	qdf_copy_macaddr((struct qdf_mac_addr *)vdev->vdev_mlme.linkaddr,
			 (struct qdf_mac_addr *)linkaddr);
}

/**
 * wlan_vdev_mlme_get_mataddr() - get mataddr
 * @vdev: VDEV object
 *
 * API to get MAT address from vdev object
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: MAT address
 */
static inline uint8_t *wlan_vdev_mlme_get_mataddr(struct wlan_objmgr_vdev *vdev)
{
	/* This API is invoked with lock acquired, do not add log prints */
	return vdev->vdev_mlme.mataddr;
}

/**
 * wlan_vdev_get_id() - get vdev id
 * @vdev: VDEV object
 *
 * API to get vdev id
 *
 * Return: vdev id
 */
static inline uint8_t wlan_vdev_get_id(struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_objmgr.vdev_id;
}

#ifdef WLAN_FEATURE_11BE_MLO
static inline uint8_t wlan_vdev_get_link_id(struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_mlme.mlo_link_id;
}

/**
 * wlan_vdev_set_link_id() - set vdev mlo link id
 * @vdev: VDEV object
 * @link_id: link id
 *
 * API to set vdev mlo link id
 *
 * Return: void
 */
static inline void wlan_vdev_set_link_id(struct wlan_objmgr_vdev *vdev,
					 uint8_t link_id)
{
	vdev->vdev_mlme.mlo_link_id = link_id;
}

#ifdef WLAN_MLO_USE_SPINLOCK
/**
 * wlan_create_vdev_mlo_lock() - API to create spin lock
 * which protects the set/clear of WLAN_VDEV_FEXT2_MLO flag in
 * vdev MLME ext2 feature caps
 * @vdev: VDEV object
 *
 * Return: void
 */
static inline
void wlan_create_vdev_mlo_lock(struct wlan_objmgr_vdev *vdev)
{
	qdf_spinlock_create(&vdev->vdev_mlme.wlan_vdev_mlo_lock);
}

/**
 * wlan_destroy_vdev_mlo_lock() - API to destroy spin lock
 * which protects the set/clear of WLAN_VDEV_FEXT2_MLO flag in
 * vdev MLME ext2 feature caps
 * @vdev: VDEV object
 *
 * Return: void
 */
static inline
void wlan_destroy_vdev_mlo_lock(struct wlan_objmgr_vdev *vdev)
{
	qdf_spinlock_destroy(&vdev->vdev_mlme.wlan_vdev_mlo_lock);
}

/**
 * wlan_acquire_vdev_mlo_lock() - API to acquire spin lock
 * which protects the set/clear of WLAN_VDEV_FEXT2_MLO flag in
 * vdev MLME ext2 feature caps
 * @vdev: VDEV object
 *
 * Return: void
 */
static inline
void wlan_acquire_vdev_mlo_lock(struct wlan_objmgr_vdev *vdev)
{
	qdf_spin_lock_bh(&vdev->vdev_mlme.wlan_vdev_mlo_lock);
}

/**
 * wlan_release_vdev_mlo_lock() - API to release spin lock
 * which protects the set/clear of WLAN_VDEV_FEXT2_MLO flag in
 * vdev MLME ext2 feature caps
 * @vdev: VDEV object
 *
 * Return: void
 */
static inline
void wlan_release_vdev_mlo_lock(struct wlan_objmgr_vdev *vdev)
{
	qdf_spin_unlock_bh(&vdev->vdev_mlme.wlan_vdev_mlo_lock);
}
#else
/**
 * wlan_create_vdev_mlo_lock() - API to create mutex which protects the
 * set/clear of WLAN_VDEV_FEXT2_MLO flag in vdev MLME ext2 feature caps
 * @vdev: VDEV object
 *
 * Return: void
 */
static inline
void wlan_create_vdev_mlo_lock(struct wlan_objmgr_vdev *vdev)
{
	qdf_mutex_create(&vdev->vdev_mlme.wlan_vdev_mlo_lock);
}

/**
 * wlan_destroy_vdev_mlo_lock() - API to destroy mutex which protects the
 * set/clear of WLAN_VDEV_FEXT2_MLO flag in vdev MLME ext2 feature caps
 * @vdev: VDEV object
 *
 * Return: void
 */
static inline
void wlan_destroy_vdev_mlo_lock(struct wlan_objmgr_vdev *vdev)
{
	qdf_mutex_destroy(&vdev->vdev_mlme.wlan_vdev_mlo_lock);
}

/**
 * wlan_acquire_vdev_mlo_lock() - API to acquire mutex which protects the
 * set/clear of WLAN_VDEV_FEXT2_MLO flag in vdev MLME ext2 feature caps
 * @vdev: VDEV object
 *
 * Return: void
 */
static inline
void wlan_acquire_vdev_mlo_lock(struct wlan_objmgr_vdev *vdev)
{
	qdf_mutex_acquire(&vdev->vdev_mlme.wlan_vdev_mlo_lock);
}

/**
 * wlan_release_vdev_mlo_lock() - API to release mutex which protects the
 * set/clear of WLAN_VDEV_FEXT2_MLO flag in vdev MLME ext2 feature caps
 * @vdev: VDEV object
 *
 * Return: void
 */
static inline
void wlan_release_vdev_mlo_lock(struct wlan_objmgr_vdev *vdev)
{
	qdf_mutex_release(&vdev->vdev_mlme.wlan_vdev_mlo_lock);
}
#endif /* WLAN_MLO_USE_SPINLOCK */
#else
static inline uint8_t wlan_vdev_get_link_id(struct wlan_objmgr_vdev *vdev)
{
	return WLAN_INVALID_LINK_ID;
}

static inline
void wlan_create_vdev_mlo_lock(struct wlan_objmgr_vdev *vdev)
{
}

static inline
void wlan_destroy_vdev_mlo_lock(struct wlan_objmgr_vdev *vdev)
{
}
#endif

/**
 * wlan_vdev_get_hw_macaddr() - get hw macaddr
 * @vdev: VDEV object
 *
 * API to retrieve the HW MAC address from PDEV
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: HW MAC address
 */
static inline uint8_t *wlan_vdev_get_hw_macaddr(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);

	/* This API is invoked with lock acquired, do not add log prints */
	if (pdev)
		return wlan_pdev_get_hw_macaddr(pdev);
	else
		return NULL;
}

/**
 * wlan_vdev_obj_lock() - Acquire VDEV spinlock
 * @vdev: VDEV object
 *
 * API to acquire VDEV lock
 * Parent lock should not be taken in child lock context
 * but child lock can be taken in parent lock context
 * (for ex: psoc lock can't be invoked in pdev/vdev/peer lock context)
 *
 * Return: void
 */
static inline void wlan_vdev_obj_lock(struct wlan_objmgr_vdev *vdev)
{
	qdf_spin_lock_bh(&vdev->vdev_lock);
}

/**
 * wlan_vdev_obj_unlock() - Release VDEV spinlock
 * @vdev: VDEV object
 *
 * API to Release VDEV lock
 *
 * Return: void
 */
static inline void wlan_vdev_obj_unlock(struct wlan_objmgr_vdev *vdev)
{
	qdf_spin_unlock_bh(&vdev->vdev_lock);
}

/**
 * wlan_vdev_mlme_set_bss_chan() - set bss chan
 * @vdev: VDEV object
 * @bss_chan: Channel
 *
 * API to set the BSS channel
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_bss_chan(
				struct wlan_objmgr_vdev *vdev,
				struct wlan_channel *bss_chan)
{
	vdev->vdev_mlme.bss_chan = bss_chan;
}

/**
 * wlan_vdev_mlme_get_bss_chan() - get bss chan
 * @vdev: VDEV object
 *
 * API to get the BSS channel
 *
 * Return: Channel
 */
static inline struct wlan_channel *wlan_vdev_mlme_get_bss_chan(
				struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_mlme.bss_chan;
}

/**
 * wlan_vdev_mlme_set_des_chan() - set desired chan
 * @vdev: VDEV object
 * @des_chan: Channel configured by user
 *
 * API to set the desired channel
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_des_chan(
				struct wlan_objmgr_vdev *vdev,
				struct wlan_channel *des_chan)
{
	vdev->vdev_mlme.des_chan = des_chan;
}

/**
 * wlan_vdev_mlme_get_des_chan() - get desired chan
 * @vdev: VDEV object
 *
 * API to get the desired channel
 *
 * Return: Channel configured by user
 */
static inline struct wlan_channel *wlan_vdev_mlme_get_des_chan(
				struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_mlme.des_chan;
}

/**
 * wlan_vdev_mlme_feat_cap_set() - set feature caps
 * @vdev: VDEV object
 * @cap: capabilities to be set
 *
 * API to set MLME feature capabilities
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_feat_cap_set(struct wlan_objmgr_vdev *vdev,
				uint32_t cap)
{
	vdev->vdev_mlme.vdev_feat_caps |= cap;
}

/**
 * wlan_vdev_mlme_feat_cap_clear() - clear feature caps
 * @vdev: VDEV object
 * @cap: capabilities to be cleared
 *
 * API to clear MLME feature capabilities
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_feat_cap_clear(struct wlan_objmgr_vdev *vdev,
				uint32_t cap)
{
	vdev->vdev_mlme.vdev_feat_caps &= ~cap;
}

/**
 * wlan_vdev_mlme_feat_cap_get() - get feature caps
 * @vdev: VDEV object
 * @cap: capabilities to be checked
 *
 * API to know MLME feature capability is set or not
 *
 * Return: 1 -- if capabilities set
 *         0 -- if capabilities clear
 */
static inline uint8_t wlan_vdev_mlme_feat_cap_get(struct wlan_objmgr_vdev *vdev,
				uint32_t cap)
{
	return (vdev->vdev_mlme.vdev_feat_caps & cap) ? 1 : 0;
}

/**
 * wlan_vdev_mlme_feat_ext_cap_set() - set ext feature caps
 * @vdev: VDEV object
 * @cap: capabilities to be set
 *
 * API to set the MLME extensive feature capabilities
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_feat_ext_cap_set(
				struct wlan_objmgr_vdev *vdev,
				uint32_t cap)
{
	vdev->vdev_mlme.vdev_feat_ext_caps |= cap;
}

/**
 * wlan_vdev_mlme_feat_ext_cap_clear() - clear ext feature caps
 * @vdev: VDEV object
 * @cap: capabilities to be cleared
 *
 * API to clear the MLME extensive feature capabilities
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_feat_ext_cap_clear(
				struct wlan_objmgr_vdev *vdev,
				uint32_t cap)
{
	vdev->vdev_mlme.vdev_feat_ext_caps &= ~cap;
}

/**
 * wlan_vdev_mlme_feat_ext_cap_get() - get feature ext caps
 * @vdev: VDEV object
 * @cap: capabilities to be checked
 *
 * API to know MLME ext feature capability is set or not
 *
 * Return: 1 -- if capabilities set
 *         0 -- if capabilities clear
 */
static inline uint8_t wlan_vdev_mlme_feat_ext_cap_get(
				struct wlan_objmgr_vdev *vdev,
				uint32_t cap)
{
	return (vdev->vdev_mlme.vdev_feat_ext_caps & cap) ? 1 : 0;
}

/**
 * wlan_vdev_mlme_feat_ext2_cap_set() - set ext2 feature caps
 * @vdev: VDEV object
 * @cap: capabilities to be set
 *
 * API to set the MLME more extensive feature capabilities
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_feat_ext2_cap_set(
				struct wlan_objmgr_vdev *vdev,
				uint32_t cap)
{
	vdev->vdev_mlme.vdev_feat_ext2_caps |= cap;
}

/**
 * wlan_vdev_mlme_feat_ext2_cap_clear() - clear ext2 feature caps
 * @vdev: VDEV object
 * @cap: capabilities to be cleared
 *
 * API to clear the MLME more extensive feature capabilities
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_feat_ext2_cap_clear(
				struct wlan_objmgr_vdev *vdev,
				uint32_t cap)
{
	vdev->vdev_mlme.vdev_feat_ext2_caps &= ~cap;
}

/**
 * wlan_vdev_mlme_feat_ext2_cap_get() - get feature ext2 caps
 * @vdev: VDEV object
 * @cap: capabilities to be checked
 *
 * API to know MLME more ext feature capability is set or not
 *
 * Return: 1 -- if capabilities set
 *         0 -- if capabilities clear
 */
static inline uint8_t wlan_vdev_mlme_feat_ext2_cap_get(
				struct wlan_objmgr_vdev *vdev,
				uint32_t cap)
{
	return (vdev->vdev_mlme.vdev_feat_ext2_caps & cap) ? 1 : 0;
}

/**
 * wlan_vdev_mlme_op_flags_set() - set vdev op flag
 * @vdev: VDEV object
 * @flag: vdev op flag to be set
 *
 * API to set the MLME VDEV OP flag
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_op_flags_set(
				struct wlan_objmgr_vdev *vdev,
				uint32_t flag)
{
	vdev->vdev_mlme.vdev_op_flags |= flag;
}

/**
 * wlan_vdev_mlme_op_flags_clear() - clear vdev op flag
 * @vdev: VDEV object
 * @flag: vdev op flag to be cleared
 *
 * API to clear the MLME VDEV OP flag
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_op_flags_clear(
				struct wlan_objmgr_vdev *vdev,
				uint32_t flag)
{
	vdev->vdev_mlme.vdev_op_flags &= ~flag;
}

/**
 * wlan_vdev_mlme_op_flags_get() - get vdev op flag
 * @vdev: VDEV object
 * @flag: vdev op flags to be checked
 *
 * API to know MLME VDEV OP flag is set or not
 *
 * Return: 1 -- if flag is set
 *         0 -- if flag is clear
 */
static inline uint8_t wlan_vdev_mlme_op_flags_get(
				struct wlan_objmgr_vdev *vdev,
				uint32_t flag)
{
	return (vdev->vdev_mlme.vdev_op_flags & flag) ? 1 : 0;
}

/**
 * wlan_vdev_mlme_cap_set() - mlme caps set
 * @vdev: VDEV object
 * @cap: capabilities to be set
 *
 * API to set the MLME capabilities
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_cap_set(struct wlan_objmgr_vdev *vdev,
				uint32_t cap)
{
	vdev->vdev_mlme.vdev_caps |= cap;
}

/**
 * wlan_vdev_mlme_cap_clear() -  mlme caps clear
 * @vdev: VDEV object
 * @cap: capabilities to be cleared
 *
 * API to clear the MLME capabilities
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_cap_clear(struct wlan_objmgr_vdev *vdev,
				uint32_t cap)
{
	vdev->vdev_mlme.vdev_caps &= ~cap;
}

/**
 * wlan_vdev_mlme_cap_get() - get mlme caps
 * @vdev: VDEV object
 * @cap: capabilities to be checked
 *
 * API to know MLME capability is set or not
 *
 * Return: 1 -- if capabilities set
 *         0 -- if capabilities clear
 */
static inline uint8_t wlan_vdev_mlme_cap_get(struct wlan_objmgr_vdev *vdev,
				uint32_t cap)
{
	return (vdev->vdev_mlme.vdev_caps & cap) ? 1 : 0;
}

/**
 * wlan_vdev_mlme_get_state() - get mlme state
 * @vdev: VDEV object
 *
 * API to get MLME state
 *
 * Return: state of MLME
 */
static inline enum wlan_vdev_state wlan_vdev_mlme_get_state(
				struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_mlme.mlme_state;
}

/**
 * wlan_vdev_mlme_get_substate() - get mlme substate
 * @vdev: VDEV object
 *
 * API to get VDEV MLME substate
 *
 * Return: substate of VDEV MLME
 */
static inline enum wlan_vdev_state wlan_vdev_mlme_get_substate(
				struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_mlme.mlme_substate;
}

/**
 * wlan_vdev_set_selfpeer() - set self peer
 * @vdev: VDEV object
 * @peer: peer pointer
 *
 * API to set the self peer of VDEV
 *
 * Return: void
 */
static inline void wlan_vdev_set_selfpeer(struct wlan_objmgr_vdev *vdev,
						struct wlan_objmgr_peer *peer)
{
	vdev->vdev_objmgr.self_peer = peer;
}

/**
 * wlan_vdev_get_selfpeer() - get self peer
 * @vdev: VDEV object
 *
 * API to get the self peer of VDEV
 *
 * Return: peer pointer
 */
static inline struct wlan_objmgr_peer *wlan_vdev_get_selfpeer(
					struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_objmgr.self_peer;
}

/**
 * wlan_vdev_set_bsspeer() - set bss peer
 * @vdev: VDEV object
 * @peer: BSS peer pointer
 *
 * API to set the BSS peer of VDEV
 *
 * Return: void
 */
static inline void wlan_vdev_set_bsspeer(struct wlan_objmgr_vdev *vdev,
						 struct wlan_objmgr_peer *peer)
{
	vdev->vdev_objmgr.bss_peer = peer;
}

/**
 * wlan_vdev_get_bsspeer() - get bss peer
 * @vdev: VDEV object
 *
 * API to get the BSS peer of VDEV, wlan_objmgr_vdev_try_get_bsspeer API
 * preferred to use outside obj manager to take and handle ref count of
 * bss_peer with ref debug ID.
 *
 * Return: BSS peer pointer
 */
static inline struct wlan_objmgr_peer *wlan_vdev_get_bsspeer(
					struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_objmgr.bss_peer;
}

/**
 * wlan_objmgr_vdev_find_peer_by_mac() - get a peer with given mac from vdev
 * @vdev: VDEV object
 * @peer_mac: mac address of the peer to be found
 * @dbg_id: dbg_id of the module
 *
 * API to get and increment ref count of BSS peer of VDEV
 *
 * Return: peer pointer to the peer of the mac address
 */
struct wlan_objmgr_peer *
wlan_objmgr_vdev_find_peer_by_mac(struct wlan_objmgr_vdev *vdev,
				  uint8_t *peer_mac,
				  wlan_objmgr_ref_dbgid dbg_id);

/**
 * wlan_objmgr_vdev_try_get_bsspeer() - get and increment ref count of BSS peer
 * of VDEV
 * @vdev: VDEV object
 * @id:   Object Manager ref debug id
 *
 * API to get and increment ref count of BSS peer of VDEV
 *
 * Return: BSS peer pointer if bss peer is present and valid else NULL
 */
struct wlan_objmgr_peer *wlan_objmgr_vdev_try_get_bsspeer(
					struct wlan_objmgr_vdev *vdev,
					wlan_objmgr_ref_dbgid id);
/**
 * wlan_vdev_get_ospriv() - get os priv pointer
 * @vdev: VDEV object
 *
 * API to get OS private pointer from VDEV
 *
 * Return: ospriv - private pointer
 */
static inline struct vdev_osif_priv *wlan_vdev_get_ospriv(
	struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_nif.osdev;
}

/**
 * wlan_vdev_reset_ospriv() - reset os priv pointer
 * @vdev: VDEV object
 *
 * API to reset OS private pointer in VDEV
 *
 * Return: void
 */
static inline void wlan_vdev_reset_ospriv(struct wlan_objmgr_vdev *vdev)
{
	vdev->vdev_nif.osdev = NULL;
}

/**
 * wlan_vdev_get_peer_count() - get vdev peer count
 * @vdev: VDEV object
 *
 * API to get peer count from VDEV
 *
 * Return: peer_count - vdev's peer count
 */
static inline uint16_t wlan_vdev_get_peer_count(struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_objmgr.wlan_peer_count;
}

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * wlan_vdev_get_legacy_peer_count() - get vdev peer count
 * @vdev: VDEV object
 *
 * API to get legacy peer count from VDEV
 *
 * Return: peer_count - vdev's peer count
 */
static inline uint16_t wlan_vdev_get_legacy_peer_count(
					struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_objmgr.wlan_peer_count -
	       qdf_atomic_read(&vdev->vdev_objmgr.wlan_ml_peer_count);
}
#else
static inline uint16_t wlan_vdev_get_legacy_peer_count(
					struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_objmgr.wlan_peer_count;
}
#endif

/**
 * wlan_vdev_mlme_is_ap() - Check whether @vdev is an AP or not
 * @vdev: VDEV object
 *
 * Return: True if @vdev is ap, otherwise false.
 */
static inline bool wlan_vdev_mlme_is_ap(struct wlan_objmgr_vdev *vdev)
{
	return (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE);
}

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * wlan_vdev_mlme_is_mlo_vdev() - Determine whether the given vdev is an MLO
 * vdev or not
 * @vdev: VDEV object
 *
 * Return: True if it is MLO, otherwise false.
 */
bool wlan_vdev_mlme_is_mlo_vdev(struct wlan_objmgr_vdev *vdev);

#ifdef WLAN_MLO_MULTI_CHIP
/**
 * wlan_vdev_mlme_is_mlo_bridge_vdev() - check if it is bridge vdev
 * @vdev: Object manager VDEV object
 *
 * API to get if given vdev is bridge vdev or not
 *
 * Return: True if it is bridge vdev, otherwise false.
 */
bool wlan_vdev_mlme_is_mlo_bridge_vdev(struct wlan_objmgr_vdev *vdev);
#else
static inline bool
wlan_vdev_mlme_is_mlo_bridge_vdev(struct wlan_objmgr_vdev *vdev)
{
	return false;
}
#endif

/**
 * wlan_vdev_mlme_is_tdls_vdev() - Determine whether the given vdev is tdls MLO
 * vdev or not
 * @vdev: VDEV object
 *
 * Return: True if it is tdls MLO, otherwise false.
 */
bool wlan_vdev_mlme_is_tdls_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_is_mlo_ap() - whether it is mlo ap or not
 * @vdev: VDEV object
 *
 * Return: True if it is mlo ap, otherwise false.
 */
static inline bool wlan_vdev_mlme_is_mlo_ap(struct wlan_objmgr_vdev *vdev)
{
	return (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) &&
		wlan_vdev_mlme_is_mlo_vdev(vdev);
}

/**
 * wlan_vdev_is_mlo_ap_with_multi_vdev() - whether it is mlo ap with vdev count
 * more than 1
 * @vdev: VDEV object
 *
 * Return: True if it is mlo ap and multi vdev, otherwise false.
 */

static inline
bool wlan_vdev_is_mlo_ap_with_multi_vdev(struct wlan_objmgr_vdev *vdev)
{
	return wlan_vdev_mlme_is_mlo_ap(vdev) &&
		(vdev->mlo_dev_ctx->wlan_vdev_count > 1);
}

#ifdef WLAN_FEATURE_MULTI_LINK_SAP
/**
 * wlan_vdev_mlme_is_mlo_ap_sync_disabled() - check if vdev up sync between
 * vdev disabled for multi link sap
 * @vdev: VDEV object
 *
 * if vdev up sync is enabled for mlo sap, vdev up and beacon template update
 * of all vdevs will depend on last vdev start response. otherwise, each vdev
 * will up and set beacon template separately.
 *
 * Return: True if vdev sync disabled.
 */
static inline bool wlan_vdev_mlme_is_mlo_ap_sync_disabled(
				struct wlan_objmgr_vdev *vdev)
{
	return (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) &&
			wlan_vdev_mlme_get_mlo_sap_sync_disable(vdev);
}
#else
static inline bool wlan_vdev_mlme_is_mlo_ap_sync_disabled(
				struct wlan_objmgr_vdev *vdev)
{
	return false;
}
#endif

/**
 * wlan_vdev_mlme_set_epcs_flag() - Set epcs flag for vdev
 * @vdev: VDEV object
 * @flag: True or Flase
 *
 * Return: void
 */
void wlan_vdev_mlme_set_epcs_flag(struct wlan_objmgr_vdev *vdev, bool flag);

/**
 * wlan_vdev_mlme_get_epcs_flag() - Get epcs flag for vdev
 * @vdev: VDEV object
 *
 * Return: bool
 */
bool wlan_vdev_mlme_get_epcs_flag(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_set_user_dis_eht_flag() - Set user disable eht flag for vdev
 * @vdev: VDEV object
 * @flag: True or Flase
 *
 * Return: void
 */
void wlan_vdev_mlme_set_user_dis_eht_flag(struct wlan_objmgr_vdev *vdev,
					  bool flag);

/**
 * wlan_vdev_mlme_get_user_dis_eht_flag() - Get user disable eht flag for vdev
 * @vdev: VDEV object
 *
 * Return: bool
 */
bool wlan_vdev_mlme_get_user_dis_eht_flag(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_set_mlo_vdev() - Set vdev as an MLO vdev
 * @vdev: VDEV object
 *
 * Return: void
 */
void wlan_vdev_mlme_set_mlo_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_clear_mlo_vdev() - Mark that the vdev is no longer an MLO vdev
 * @vdev: VDEV object
 *
 * Return: void
 */
void wlan_vdev_mlme_clear_mlo_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_set_mlo_link_vdev() - Set vdev as an MLO link vdev
 * @vdev: VDEV object
 *
 * Return: void
 */
void wlan_vdev_mlme_set_mlo_link_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_clear_mlo_link_vdev() - Mark that the vdev is no longer an
 * MLO link vdev
 * @vdev: VDEV object
 *
 * Return: void
 */
void wlan_vdev_mlme_clear_mlo_link_vdev(struct wlan_objmgr_vdev *vdev);

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
/**
 * wlan_vdev_mlme_set_mlo_link_switch_in_progress() - Set link switch in
 * progress flag for VDEV.
 * @vdev: VDEV object manager.
 *
 * Return: void
 */
static inline void
wlan_vdev_mlme_set_mlo_link_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
	unsigned long flag = WLAN_VDEV_OP_MLO_LINK_SWITCH_IN_PROGRESS;

	wlan_vdev_mlme_op_flags_set(vdev, flag);
}

/**
 * wlan_vdev_mlme_clear_mlo_link_switch_in_progress() - Clear link switch in
 * progress flag for VDEV.
 * @vdev: VDEV object manager
 *
 * Return: void
 */
static inline void
wlan_vdev_mlme_clear_mlo_link_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
	unsigned long flag = WLAN_VDEV_OP_MLO_LINK_SWITCH_IN_PROGRESS;

	wlan_vdev_mlme_op_flags_clear(vdev, flag);
}

/**
 * wlan_vdev_mlme_is_mlo_link_switch_in_progress() - Return true if VDEV is
 * in link transitioning state.
 * @vdev: VDEV object manager.
 *
 * Return: bool
 */
static inline bool
wlan_vdev_mlme_is_mlo_link_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
	unsigned long flag = WLAN_VDEV_OP_MLO_LINK_SWITCH_IN_PROGRESS;

	return wlan_vdev_mlme_op_flags_get(vdev, flag);
}

/**
 * wlan_vdev_mlme_set_mlo_link_rejection_in_progress() - Set link rejection in
 * progress flag for VDEV.
 * @vdev: VDEV object manager.
 *
 * Return: void
 */
static inline void
wlan_vdev_mlme_set_mlo_link_rejection_in_progress(struct wlan_objmgr_vdev *vdev)
{
	unsigned long flag = WLAN_VDEV_OP_MLO_LINK_REJECTION_IN_PROGRESS;

	wlan_vdev_mlme_op_flags_set(vdev, flag);
}

/**
 * wlan_vdev_mlme_clear_mlo_link_rejection_in_progress() - Clear link rejection in
 * progress flag for VDEV.
 * @vdev: VDEV object manager
 *
 * Return: void
 */
static inline void
wlan_vdev_mlme_clear_mlo_link_rejection_in_progress(struct wlan_objmgr_vdev *vdev)
{
	unsigned long flag = WLAN_VDEV_OP_MLO_LINK_REJECTION_IN_PROGRESS;

	wlan_vdev_mlme_op_flags_clear(vdev, flag);
}

/**
 * wlan_vdev_mlme_is_mlo_link_rejection_in_progress() - Return true if VDEV is
 * in link rejection in progress.
 * @vdev: VDEV object manager.
 *
 * Return: bool
 */
static inline bool
wlan_vdev_mlme_is_mlo_link_rejection_in_progress(struct wlan_objmgr_vdev *vdev)
{
	unsigned long flag = WLAN_VDEV_OP_MLO_LINK_REJECTION_IN_PROGRESS;

	return wlan_vdev_mlme_op_flags_get(vdev, flag);
}

#else
static inline void
wlan_vdev_mlme_set_mlo_link_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
wlan_vdev_mlme_clear_mlo_link_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
}

static inline bool
wlan_vdev_mlme_is_mlo_link_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline void
wlan_vdev_mlme_set_mlo_link_rejection_in_progress(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
wlan_vdev_mlme_clear_mlo_link_rejection_in_progress(struct wlan_objmgr_vdev *vdev)
{
}

static inline bool
wlan_vdev_mlme_is_mlo_link_rejection_in_progress(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

#endif /* WLAN_FEATURE_11BE_MLO_ADV_FEATURE  */

#ifdef WLAN_MCAST_MLO
/**
 * wlan_vdev_mlme_is_mlo_mcast_vdev() - whether it is mlo mcast vdev or not
 * @vdev: VDEV object
 *
 * Return: True if it is mlo mcast vdev, otherwise false.
 */
static inline
bool wlan_vdev_mlme_is_mlo_mcast_vdev(struct wlan_objmgr_vdev *vdev)
{
	return wlan_vdev_mlme_feat_ext2_cap_get(vdev,
						WLAN_VDEV_FEXT2_MLO_MCAST);
}
#else
static inline
bool wlan_vdev_mlme_is_mlo_mcast_vdev(struct wlan_objmgr_vdev *vdev)
{
	return false;
}
#endif

/**
 * wlan_vdev_mlme_is_mlo_link_vdev() - whether it is mlo sta link vdev or not
 * @vdev: VDEV object
 *
 * Return: True if it is mlo sta link, otherwise false.
 */
static inline
bool wlan_vdev_mlme_is_mlo_link_vdev(struct wlan_objmgr_vdev *vdev)
{
	return wlan_vdev_mlme_feat_ext2_cap_get(vdev,
						WLAN_VDEV_FEXT2_MLO_STA_LINK);
}

/**
 * wlan_vdev_mlme_is_assoc_sta_vdev() - whether it is mlo sta assoc vdev or not
 * @vdev: VDEV object
 *
 * Return: True if it is mlo sta assoc vdev, otherwise false.
 */
static inline
bool wlan_vdev_mlme_is_assoc_sta_vdev(struct wlan_objmgr_vdev *vdev)
{
	if (!(wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) ||
	    !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return false;

	if (!wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		return true;

	return false;
}

/**
 * wlan_vdev_mlme_is_link_sta_vdev() - whether it is mlo sta link vdev or not
 * @vdev: VDEV object
 *
 * Return: True if it is mlo sta link vdev, otherwise false.
 */
static inline
bool wlan_vdev_mlme_is_link_sta_vdev(struct wlan_objmgr_vdev *vdev)
{
	if (!(wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) ||
	    !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return false;

	if (wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		return true;

	return false;
}
#else
static inline
void wlan_vdev_mlme_set_user_dis_eht_flag(struct wlan_objmgr_vdev *vdev,
					  bool flag)
{
}

static inline
bool wlan_vdev_mlme_get_user_dis_eht_flag(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
bool wlan_vdev_mlme_is_mlo_vdev(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline bool
wlan_vdev_mlme_is_mlo_bridge_vdev(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline bool wlan_vdev_mlme_is_mlo_ap(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline bool wlan_vdev_mlme_is_mlo_ap_sync_disabled(
				struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
bool wlan_vdev_is_mlo_ap_with_multi_vdev(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
void wlan_vdev_mlme_set_mlo_vdev(struct wlan_objmgr_vdev *vdev)
{
}

static inline
void wlan_vdev_mlme_clear_mlo_vdev(struct wlan_objmgr_vdev *vdev)
{
}

static inline
void wlan_vdev_mlme_set_mlo_link_vdev(struct wlan_objmgr_vdev *vdev)
{
}

static inline
void wlan_vdev_mlme_clear_mlo_link_vdev(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
wlan_vdev_mlme_set_mlo_link_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
wlan_vdev_mlme_clear_mlo_link_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
}

static inline bool
wlan_vdev_mlme_is_mlo_link_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
bool wlan_vdev_mlme_is_mlo_link_vdev(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
bool wlan_vdev_mlme_is_assoc_sta_vdev(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
bool wlan_vdev_mlme_is_link_sta_vdev(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline void
wlan_vdev_mlme_set_mlo_link_rejection_in_progress(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
wlan_vdev_mlme_clear_mlo_link_rejection_in_progress(struct wlan_objmgr_vdev *vdev)
{
}

static inline bool
wlan_vdev_mlme_is_mlo_link_rejection_in_progress(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

#endif

/**
 * DOC: Examples to use VDEV ref count APIs
 *
 * In all the scenarios, the pair of API should be followed
 * other it lead to memory leak
 *
 *  scenario 1:
 *
 *     wlan_objmgr_vdev_obj_create()
 *     ----
 *     wlan_objmgr_vdev_obj_delete()
 *
 *  scenario 2:
 *
 *     wlan_objmgr_vdev_get_ref()
 *     ----
 *     the operations which are done on
 *     vdev object
 *     ----
 *     wlan_objmgr_vdev_release_ref()
 *
 *  scenario 3:
 *
 *     API to retrieve vdev (xxx_get_vdev_xxx())
 *     ----
 *     the operations which are done on
 *     vdev object
 *     ----
 *     wlan_objmgr_vdev_release_ref()
 */

/**
 * wlan_objmgr_vdev_get_ref() - increment ref count
 * @vdev: VDEV object
 * @id:   Object Manager ref debug id
 *
 * API to increment ref count of vdev
 *
 * Return: void
 */
#ifdef WLAN_OBJMGR_REF_ID_TRACE
#define wlan_objmgr_vdev_get_ref(vdev, id) \
		wlan_objmgr_vdev_get_ref_debug(vdev, id, __func__, __LINE__)

void wlan_objmgr_vdev_get_ref_debug(struct wlan_objmgr_vdev *vdev,
				    wlan_objmgr_ref_dbgid id,
				    const char *func, int line);
#else
void wlan_objmgr_vdev_get_ref(struct wlan_objmgr_vdev *vdev,
				wlan_objmgr_ref_dbgid id);
#endif

/**
 * wlan_objmgr_vdev_try_get_ref() - increment ref count, if allowed
 * @vdev: VDEV object
 * @id:   Object Manager ref debug id
 *
 * API to increment ref count of vdev after checking valid object state
 *
 * Return: void
 */
#ifdef WLAN_OBJMGR_REF_ID_TRACE
#define wlan_objmgr_vdev_try_get_ref(vdev, id) \
		wlan_objmgr_vdev_try_get_ref_debug(vdev, id, \
		__func__, __LINE__)

QDF_STATUS wlan_objmgr_vdev_try_get_ref_debug(struct wlan_objmgr_vdev *vdev,
					      wlan_objmgr_ref_dbgid id,
					      const char *func, int line);
#else
QDF_STATUS wlan_objmgr_vdev_try_get_ref(struct wlan_objmgr_vdev *vdev,
						wlan_objmgr_ref_dbgid id);
#endif

/**
 * wlan_objmgr_vdev_release_ref() - decrement ref count
 * @vdev: VDEV object
 * @id:   Object Manager ref debug id
 *
 * API to decrement ref count of vdev, if ref count is 1, it initiates the
 * VDEV deletion
 *
 * Return: void
 */
#ifdef WLAN_OBJMGR_REF_ID_TRACE
#define wlan_objmgr_vdev_release_ref(vdev, id)\
		wlan_objmgr_vdev_release_ref_debug(vdev, id, \
		__func__, __LINE__)

void wlan_objmgr_vdev_release_ref_debug(struct wlan_objmgr_vdev *vdev,
					wlan_objmgr_ref_dbgid id,
					const char *func, int line);
#else
void wlan_objmgr_vdev_release_ref(struct wlan_objmgr_vdev *vdev,
						wlan_objmgr_ref_dbgid id);
#endif

/**
 * wlan_vdev_get_next_active_vdev_of_pdev() - get next active vdev
 * @pdev: PDEV object
 * @vdev_list: qdf_list_t
 * @vdev: VDEV object
 * @dbg_id: id of the caller
 *
 * API to get next active vdev object pointer of vdev
 *
 * Return: VDEV object
 */
#ifdef WLAN_OBJMGR_REF_ID_TRACE
#define wlan_vdev_get_next_active_vdev_of_pdev(pdev, vdev_list, vdev, dbg_id) \
		wlan_vdev_get_next_active_vdev_of_pdev_debug(pdev, vdev_list, \
		vdev, dbg_id, __func__, __LINE__)

struct wlan_objmgr_vdev *wlan_vdev_get_next_active_vdev_of_pdev_debug(
					struct wlan_objmgr_pdev *pdev,
					qdf_list_t *vdev_list,
					struct wlan_objmgr_vdev *vdev,
					wlan_objmgr_ref_dbgid dbg_id,
					const char *func, int line);
#else
struct wlan_objmgr_vdev *wlan_vdev_get_next_active_vdev_of_pdev(
					struct wlan_objmgr_pdev *pdev,
					qdf_list_t *vdev_list,
					struct wlan_objmgr_vdev *vdev,
					wlan_objmgr_ref_dbgid dbg_id);
#endif

/**
 * wlan_pdev_peek_active_first_vdev() - get first active vdev from pdev list
 * @pdev: PDEV object
 * @dbg_id: id of the caller
 *
 * API to get the head active vdev of given pdev (of pdev's vdev list)
 *
 * Return:
 */
#ifdef WLAN_OBJMGR_REF_ID_TRACE
#define wlan_pdev_peek_active_first_vdev(pdev, dbg_id) \
		wlan_pdev_peek_active_first_vdev_debug(pdev, dbg_id, \
		__func__, __LINE__)

struct wlan_objmgr_vdev *wlan_pdev_peek_active_first_vdev_debug(
		struct wlan_objmgr_pdev *pdev,
		wlan_objmgr_ref_dbgid dbg_id,
		const char *func, int line);
#else
struct wlan_objmgr_vdev *wlan_pdev_peek_active_first_vdev(
		struct wlan_objmgr_pdev *pdev,
		wlan_objmgr_ref_dbgid dbg_id);
#endif

/**
 * wlan_pdev_vdev_list_peek_active_head() - get first active vdev from pdev list
 * @pdev: PDEV object
 * @vdev_list: qdf_list_t
 * @dbg_id: id of the caller
 *
 * API to get the head active vdev of given vdev (of pdev's vdev list)
 *
 * Return: head peer
 */
#ifdef WLAN_OBJMGR_REF_ID_TRACE
#define wlan_pdev_vdev_list_peek_active_head(pdev, vdev_list, dbg_id) \
		wlan_pdev_vdev_list_peek_active_head_debug(pdev, vdev_list, \
		dbg_id, __func__, __LINE__)

struct wlan_objmgr_vdev *wlan_pdev_vdev_list_peek_active_head_debug(
				struct wlan_objmgr_pdev *pdev,
				qdf_list_t *vdev_list,
				wlan_objmgr_ref_dbgid dbg_id,
				const char *func, int line);
#else
struct wlan_objmgr_vdev *wlan_pdev_vdev_list_peek_active_head(
				struct wlan_objmgr_pdev *pdev,
				qdf_list_t *vdev_list,
				wlan_objmgr_ref_dbgid dbg_id);
#endif

/**
 * wlan_objmgr_vdev_peer_freed_notify() - Notifies modules about peer freed
 * @vdev: VDEV object
 *
 * API to invokes registered callbacks to notify about peer freed
 *
 * Return: void
 */
void wlan_objmgr_vdev_peer_freed_notify(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_set_max_peer_count() - set max peer count
 * @vdev: VDEV object
 * @count: Max peer count
 *
 * API to set max peer count of VDEV
 *
 * Return: void
 */
static inline void wlan_vdev_set_max_peer_count(struct wlan_objmgr_vdev *vdev,
						uint16_t count)
{
	vdev->vdev_objmgr.max_peer_count = count;
}

/**
 * wlan_vdev_get_max_peer_count() - get max peer count
 * @vdev: VDEV object
 *
 * API to get max peer count of VDEV
 *
 * Return: max peer count
 */
static inline uint16_t wlan_vdev_get_max_peer_count(
						struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_objmgr.max_peer_count;
}

#ifdef WLAN_FEATURE_11BE_MLO
/**wlan_vdev_set_mlo_external_sae_auth_conversion() - set MLO external sae auth
 * @vdev: VDEV object
 * @val: true or false
 *
 * API to set mlo external sae auth of VDEV
 *
 * Return: void
 */
static inline void
wlan_vdev_set_mlo_external_sae_auth_conversion(struct wlan_objmgr_vdev *vdev,
					       bool val)
{
	vdev->vdev_mlme.mlo_external_sae_auth = val;
}

/**wlan_vdev_get_mlo_external_sae_auth_conversion() - get MLO external sae auth
 * @vdev: VDEV object
 *
 * API to get mlo external sae auth of VDEV
 *
 * Return: mlo external sae auth of VDEV
 */
static inline bool
wlan_vdev_get_mlo_external_sae_auth_conversion(struct wlan_objmgr_vdev *vdev)
{
	return vdev->vdev_mlme.mlo_external_sae_auth;
}
#endif

/**
 * wlan_print_vdev_info() - print vdev members
 * @vdev: vdev object pointer
 *
 * Return: void
 */
#ifdef WLAN_OBJMGR_DEBUG
void wlan_print_vdev_info(struct wlan_objmgr_vdev *vdev);
#else
static inline void wlan_print_vdev_info(struct wlan_objmgr_vdev *vdev) {}
#endif

/**
 * wlan_objmgr_vdev_trace_init_lock() - Initialize trace lock
 * @vdev: vdev object pointer
 *
 * Return: void
 */
#ifdef WLAN_OBJMGR_REF_ID_TRACE
static inline void
wlan_objmgr_vdev_trace_init_lock(struct wlan_objmgr_vdev *vdev)
{
	wlan_objmgr_trace_init_lock(&vdev->vdev_objmgr.trace);
}
#else
static inline void
wlan_objmgr_vdev_trace_init_lock(struct wlan_objmgr_vdev *vdev)
{
}
#endif

/**
 * wlan_objmgr_vdev_trace_deinit_lock() - Deinitialize trace lock
 * @vdev: vdev object pointer
 *
 * Return: void
 */
#ifdef WLAN_OBJMGR_REF_ID_TRACE
static inline void
wlan_objmgr_vdev_trace_deinit_lock(struct wlan_objmgr_vdev *vdev)
{
	wlan_objmgr_trace_deinit_lock(&vdev->vdev_objmgr.trace);
}
#else
static inline void
wlan_objmgr_vdev_trace_deinit_lock(struct wlan_objmgr_vdev *vdev)
{
}
#endif

/**
 * wlan_objmgr_vdev_trace_del_ref_list() - Delete trace ref list
 * @vdev: vdev object pointer
 *
 * Return: void
 */
#ifdef WLAN_OBJMGR_REF_ID_TRACE
static inline void
wlan_objmgr_vdev_trace_del_ref_list(struct wlan_objmgr_vdev *vdev)
{
	wlan_objmgr_trace_del_ref_list(&vdev->vdev_objmgr.trace);
}
#else
static inline void
wlan_objmgr_vdev_trace_del_ref_list(struct wlan_objmgr_vdev *vdev)
{
}
#endif

/**
 * wlan_vdev_get_bss_peer_mac_for_pmksa() - To get bss peer mac/mld
 * address based on association to cache/retrieve PMK.
 * @vdev: Pointer to vdev
 * @bss_peer_mac: Pointer to BSS peer MAC address.
 *
 * The PMKSA entry for an ML candaidate will be present with MLD
 * address, whereas for non-ML candidate legacy MAC address is used
 * to save the PMKSA. To get the right entry during lookup, this API
 * will return MLD address if the VDEV is MLO VDEV else return
 * MAC address of BSS peer.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_vdev_get_bss_peer_mac_for_pmksa(struct wlan_objmgr_vdev *vdev,
				     struct qdf_mac_addr *bss_peer_mac);

/**
 * wlan_vdev_get_bss_peer_mac() - to get bss peer mac address
 * @vdev: pointer to vdev
 * @bss_peer_mac: pointer to bss_peer_mac_address
 *
 * This API is used to get mac address of peer.
 *
 * Context: Any context.
 *
 * Return: QDF_STATUS based on overall success
 */
QDF_STATUS wlan_vdev_get_bss_peer_mac(struct wlan_objmgr_vdev *vdev,
				      struct qdf_mac_addr *bss_peer_mac);

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * wlan_vdev_get_bss_peer_mld_mac() - to get bss peer mld mac address
 * @vdev: pointer to vdev
 * @mld_mac: pointer to mld mac address
 *
 * This API is used to get mld mac address of peer.
 *
 * Context: Any context.
 *
 * Return: QDF_STATUS based on overall success
 */
QDF_STATUS wlan_vdev_get_bss_peer_mld_mac(struct wlan_objmgr_vdev *vdev,
					  struct qdf_mac_addr *mld_mac);

/**
 * wlan_vdev_get_mlo_dev_ctx() - get MLO dev context
 * @vdev: VDEV object
 *
 * API to get MLO dev context pointer from vdev
 *
 * Return: MLO dev context pointer
 */
static inline struct wlan_mlo_dev_context *wlan_vdev_get_mlo_dev_ctx(
				struct wlan_objmgr_vdev *vdev)
{
	return vdev->mlo_dev_ctx;
}

/**
 * wlan_objmgr_vdev_init_ml_peer_count() - initialize ml_peer_count
 * @vdev: vdev object pointer
 *
 * Return: void
 */
static inline void
wlan_objmgr_vdev_init_ml_peer_count(struct wlan_objmgr_vdev *vdev)
{
	qdf_atomic_init(&vdev->vdev_objmgr.wlan_ml_peer_count);
}

#else
static inline
QDF_STATUS wlan_vdev_get_bss_peer_mld_mac(struct wlan_objmgr_vdev *vdev,
					  struct qdf_mac_addr *mld_mac)
{
	return QDF_STATUS_E_INVAL;
}

static inline void
wlan_objmgr_vdev_init_ml_peer_count(struct wlan_objmgr_vdev *vdev)
{
}

static inline
bool wlan_vdev_mlme_is_tdls_vdev(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

#endif

/**
 * wlan_mlo_peer_delete_is_not_allowed()
 * @vdev: VDEV object
 *
 * API to check if WLAN_VDEV_OP_MLME_LEGACY_PEER_DISCON_TRIG is set therefore
 * whether mlo peer delete should be allowed or not
 *
 * Return: True if MLO peer delete is not allowed, otherwise false.
 */
static inline bool wlan_mlo_peer_delete_is_not_allowed(
		struct wlan_objmgr_vdev *vdev)
{
	return wlan_vdev_mlme_op_flags_get(vdev,
				WLAN_VDEV_OP_MLME_LEGACY_PEER_DISCON_TRIG);
}

#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
/**
 * wlan_vdev_init_skip_pumac_cnt() - init skip_pumac_cnt
 * @vdev: VDEV object
 *
 * API to initialize skip_pumac_cnt
 *
 * Return: void
 */
static inline void
wlan_vdev_init_skip_pumac_cnt(struct wlan_objmgr_vdev *vdev)
{
	qdf_atomic_init(&vdev->vdev_mlme.skip_pumac_cnt);
}

/**
 * wlan_vdev_inc_skip_pumac_cnt() - inc skip_pumac_cnt
 * @vdev: VDEV object
 *
 * API to increment skip_pumac_cnt
 *
 * Return: void
 */
static inline void
wlan_vdev_inc_skip_pumac_cnt(struct wlan_objmgr_vdev *vdev)
{
	qdf_atomic_inc(&vdev->vdev_mlme.skip_pumac_cnt);
}

/**
 * wlan_vdev_dec_skip_pumac_cnt() - dec skip_pumac_cnt
 * @vdev: VDEV object
 *
 * API to decrement skip_pumac_cnt
 *
 * Return: void
 */
static inline void
wlan_vdev_dec_skip_pumac_cnt(struct wlan_objmgr_vdev *vdev)
{
	qdf_atomic_dec(&vdev->vdev_mlme.skip_pumac_cnt);
}

/**
 * wlan_vdev_read_skip_pumac_cnt() - read skip_pumac_cnt
 * @vdev: VDEV object
 *
 * API to read skip_pumac_cnt value
 *
 * Return: skip_pumac_cnt value
 */
static inline int32_t
wlan_vdev_read_skip_pumac_cnt(struct wlan_objmgr_vdev *vdev)
{
	return qdf_atomic_read(&vdev->vdev_mlme.skip_pumac_cnt);
}
#else
static inline void
wlan_vdev_init_skip_pumac_cnt(struct wlan_objmgr_vdev *vdev)
{ }

static inline void
wlan_vdev_inc_skip_pumac_cnt(struct wlan_objmgr_vdev *vdev)
{ }

static inline void
wlan_vdev_dec_skip_pumac_cnt(struct wlan_objmgr_vdev *vdev)
{ }

static inline int32_t
wlan_vdev_read_skip_pumac_cnt(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}
#endif

/**
 * wlan_vdev_get_peer_sta_count: Get peer STA count
 * @vdev: Pointer to vdev
 *
 * Return: STA peer count
 */
uint8_t wlan_vdev_get_peer_sta_count(struct wlan_objmgr_vdev *vdev);
#endif /* _WLAN_OBJMGR_VDEV_OBJ_H_*/
