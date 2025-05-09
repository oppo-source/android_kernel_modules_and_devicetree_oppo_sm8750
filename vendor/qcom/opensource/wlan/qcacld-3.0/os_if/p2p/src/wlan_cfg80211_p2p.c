/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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
 * DOC: defines driver functions interfacing with linux kernel
 */

#include <qdf_util.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_p2p_public_struct.h>
#include <wlan_p2p_ucfg_api.h>
#include <wlan_policy_mgr_api.h>
#include <wlan_utility.h>
#include <wlan_osif_priv.h>
#include "wlan_cfg80211.h"
#include "wlan_cfg80211_p2p.h"
#include "wlan_mlo_mgr_sta.h"

#define MAX_NO_OF_2_4_CHANNELS 14
#define MAX_OFFCHAN_TIME_FOR_DNBS 150

#ifdef FEATURE_WLAN_SUPPORT_USD
const struct nla_policy
p2p_usd_chan_config_policy[QCA_WLAN_VENDOR_ATTR_USD_CHAN_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_USD_CHAN_CONFIG_DEFAULT_FREQ] = {
		.type = NLA_U32,
		.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_USD_CHAN_CONFIG_FREQ_LIST] = {
		.type = NLA_BINARY,
		.len = P2P_USD_CHAN_CONFIG_FREQ_LIST_MAX_SIZE * sizeof(uint32_t)
	},
};

const struct nla_policy
p2p_usd_attr_policy[QCA_WLAN_VENDOR_ATTR_USD_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_USD_SRC_ADDR] = {
		.type = NLA_BINARY,
		.len = QDF_MAC_ADDR_SIZE
	},
	[QCA_WLAN_VENDOR_ATTR_USD_OP_TYPE] = {
		.type = NLA_U8,
		.len = sizeof(uint8_t)
	},
	[QCA_WLAN_VENDOR_ATTR_USD_INSTANCE_ID] = {
		.type = NLA_U8,
		.len = sizeof(uint8_t)
	},
	[QCA_WLAN_VENDOR_ATTR_USD_SERVICE_ID] = {
		.type = NLA_BINARY,
		.len = P2P_USD_SERVICE_LEN
	},
	[QCA_WLAN_VENDOR_ATTR_USD_SERVICE_PROTOCOL_TYPE] = {
		.type = NLA_U8,
		.len = sizeof(uint8_t)
	},
	[QCA_WLAN_VENDOR_ATTR_USD_SSI] = {
		.type = NLA_BINARY,
		.len = P2P_USD_SSI_LEN
	},
	[QCA_WLAN_VENDOR_ATTR_USD_CHAN_CONFIG] = {
		.type = NLA_NESTED,
	},
	[QCA_WLAN_VENDOR_ATTR_USD_ELEMENT_CONTAINER] = {
		.type = NLA_BINARY,
		.len = P2P_USD_FRAME_LEN
	},
	[QCA_WLAN_VENDOR_ATTR_USD_TTL] = {
		.type = NLA_U16,
		.len = sizeof(uint16_t)
	},
	[QCA_WLAN_VENDOR_ATTR_USD_STATUS] = {
		.type = NLA_U8,
	},
};
#endif /* FEATURE_WLAN_SUPPORT_USD */

/**
 * wlan_p2p_rx_callback() - Callback for rx mgmt frame
 * @user_data: pointer to soc object
 * @rx_frame: RX mgmt frame information
 *
 * This callback will be used to rx frames in os interface.
 *
 * Return: None
 */
static void wlan_p2p_rx_callback(void *user_data,
	struct p2p_rx_mgmt_frame *rx_frame)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev, *assoc_vdev;
	struct vdev_osif_priv *osif_priv;
	struct wireless_dev *wdev;
	enum QDF_OPMODE opmode;
	uint32_t mgmt_frm_registration_update = 0;

	psoc = user_data;
	if (!psoc) {
		osif_err("psoc is null");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
		rx_frame->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		osif_err("vdev is null");
		return;
	}

	if (rx_frame->frm_type == MGMT_PROBE_REQ) {
		mgmt_frm_registration_update =
			ucfg_p2p_get_mgmt_frm_registration_update(psoc);
		if (!(mgmt_frm_registration_update & BIT(MGMT_SUBTYPE_PROBE_REQ >> 4))) {
			osif_debug("Drop mgmt frame_type %d, kernel registration is not open",
				   rx_frame->frm_type);
			goto fail;
		}
	}

	assoc_vdev = vdev;
	opmode = wlan_vdev_mlme_get_opmode(assoc_vdev);

	if (opmode == QDF_STA_MODE && wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		assoc_vdev = ucfg_mlo_get_assoc_link_vdev(vdev);
		if (!assoc_vdev) {
			osif_err("Assoc vdev is NULL");
			goto fail;
		}
	}

	osif_priv = wlan_vdev_get_ospriv(assoc_vdev);
	if (!osif_priv) {
		osif_err("osif_priv is null");
		goto fail;
	}

	wdev = osif_priv->wdev;
	if (!wdev) {
		osif_err("wdev is null");
		goto fail;
	}

	osif_debug("Indicate frame over nl80211, idx:%d",
		   wdev->netdev->ifindex);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
	cfg80211_rx_mgmt(wdev, rx_frame->rx_freq, rx_frame->rx_rssi * 100,
			 rx_frame->buf, rx_frame->frame_len,
			 NL80211_RXMGMT_FLAG_ANSWERED);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0))
	cfg80211_rx_mgmt(wdev, rx_frame->rx_freq, rx_frame->rx_rssi * 100,
			 rx_frame->buf, rx_frame->frame_len,
			 NL80211_RXMGMT_FLAG_ANSWERED, GFP_ATOMIC);
#else
	cfg80211_rx_mgmt(wdev, rx_frame->rx_freq, rx_frame->rx_rssi * 100,
			 rx_frame->buf, rx_frame->frame_len, GFP_ATOMIC);
#endif /* LINUX_VERSION_CODE */
fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
}

/**
 * wlan_p2p_action_tx_cnf_callback() - Callback for tx confirmation
 * @user_data: pointer to soc object
 * @tx_cnf: tx confirmation information
 *
 * This callback will be used to give tx mgmt frame confirmation to
 * os interface.
 *
 * Return: None
 */
static void wlan_p2p_action_tx_cnf_callback(void *user_data,
	struct p2p_tx_cnf *tx_cnf)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;
	struct vdev_osif_priv *osif_priv;
	struct wireless_dev *wdev;
	bool is_success;

	psoc = user_data;
	if (!psoc) {
		osif_err("psoc is null");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
		tx_cnf->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		osif_err("vdev is null");
		return;
	}

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		osif_err("osif_priv is null");
		goto fail;
	}

	wdev = osif_priv->wdev;
	if (!wdev) {
		osif_err("wireless dev is null");
		goto fail;
	}

	is_success = tx_cnf->status ? false : true;
	cfg80211_mgmt_tx_status(
		wdev,
		tx_cnf->action_cookie,
		tx_cnf->buf, tx_cnf->buf_len,
		is_success, GFP_KERNEL);
fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
}

#ifdef FEATURE_P2P_LISTEN_OFFLOAD
/**
 * wlan_p2p_lo_event_callback() - Callback for listen offload event
 * @user_data: pointer to soc object
 * @p2p_lo_event: listen offload event information
 *
 * This callback will be used to give listen offload event to os interface.
 *
 * Return: None
 */
static void wlan_p2p_lo_event_callback(void *user_data,
	struct p2p_lo_event *p2p_lo_event)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;
	struct vdev_osif_priv *osif_priv;
	struct wireless_dev *wdev;
	struct sk_buff *vendor_event;
	enum qca_nl80211_vendor_subcmds_index index =
		QCA_NL80211_VENDOR_SUBCMD_P2P_LO_EVENT_INDEX;

	osif_debug("user data:%pK, vdev id:%d, reason code:%d",
		   user_data, p2p_lo_event->vdev_id,
		   p2p_lo_event->reason_code);

	psoc = user_data;
	if (!psoc) {
		osif_err("psoc is null");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
		p2p_lo_event->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		osif_err("vdev is null");
		return;
	}

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		osif_err("osif_priv is null");
		goto fail;
	}

	wdev = osif_priv->wdev;
	if (!wdev) {
		osif_err("wireless dev is null");
		goto fail;
	}

	vendor_event = wlan_cfg80211_vendor_event_alloc(wdev->wiphy, NULL,
							sizeof(uint32_t) +
							NLMSG_HDRLEN,
							index, GFP_KERNEL);
	if (!vendor_event) {
		osif_err("wlan_cfg80211_vendor_event_alloc failed");
		goto fail;
	}

	if (nla_put_u32(vendor_event,
		QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_STOP_REASON,
		p2p_lo_event->reason_code)) {
		osif_err("nla put failed");
		wlan_cfg80211_vendor_free_skb(vendor_event);
		goto fail;
	}

	wlan_cfg80211_vendor_event(vendor_event, GFP_KERNEL);

fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
}

static inline void wlan_p2p_init_lo_event(struct p2p_start_param *start_param,
					  struct wlan_objmgr_psoc *psoc)
{
	start_param->lo_event_cb = wlan_p2p_lo_event_callback;
	start_param->lo_event_cb_data = psoc;
}
#else
static inline void wlan_p2p_init_lo_event(struct p2p_start_param *start_param,
					  struct wlan_objmgr_psoc *psoc)
{
}
#endif /* FEATURE_P2P_LISTEN_OFFLOAD */
/**
 * wlan_p2p_event_callback() - Callback for P2P event
 * @user_data: pointer to soc object
 * @p2p_event: p2p event information
 *
 * This callback will be used to give p2p event to os interface.
 *
 * Return: None
 */
static void wlan_p2p_event_callback(void *user_data,
	struct p2p_event *p2p_event)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;
	struct ieee80211_channel *chan;
	struct vdev_osif_priv *osif_priv;
	struct wireless_dev *wdev;
	struct wlan_objmgr_pdev *pdev;

	osif_debug("user data:%pK, vdev id:%d, event type:%d",
		   user_data, p2p_event->vdev_id, p2p_event->roc_event);

	psoc = user_data;
	if (!psoc) {
		osif_err("psoc is null");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
		p2p_event->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		osif_err("vdev is null");
		return;
	}

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		osif_err("osif_priv is null");
		goto fail;
	}

	wdev = osif_priv->wdev;
	if (!wdev) {
		osif_err("wireless dev is null");
		goto fail;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	chan = ieee80211_get_channel(wdev->wiphy, p2p_event->chan_freq);
	if (!chan) {
		osif_err("channel conversion failed");
		goto fail;
	}

	if (p2p_event->roc_event == ROC_EVENT_READY_ON_CHAN) {
		cfg80211_ready_on_channel(wdev,
			p2p_event->cookie, chan,
			p2p_event->duration, GFP_KERNEL);
	} else if (p2p_event->roc_event == ROC_EVENT_COMPLETED) {
		cfg80211_remain_on_channel_expired(wdev,
			p2p_event->cookie, chan, GFP_KERNEL);
	} else {
		osif_err("Invalid p2p event");
	}

fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
}

QDF_STATUS p2p_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	struct p2p_start_param start_param;

	if (!psoc) {
		osif_err("psoc null");
		return QDF_STATUS_E_INVAL;
	}

	start_param.rx_cb = wlan_p2p_rx_callback;
	start_param.rx_cb_data = psoc;
	start_param.event_cb = wlan_p2p_event_callback;
	start_param.event_cb_data = psoc;
	start_param.tx_cnf_cb = wlan_p2p_action_tx_cnf_callback;
	start_param.tx_cnf_cb_data = psoc;
	wlan_p2p_init_lo_event(&start_param, psoc);

	return ucfg_p2p_psoc_start(psoc, &start_param);
}

QDF_STATUS p2p_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	if (!psoc) {
		osif_err("psoc null");
		return QDF_STATUS_E_INVAL;
	}

	return ucfg_p2p_psoc_stop(psoc);
}

int wlan_cfg80211_roc(struct wlan_objmgr_vdev *vdev,
	struct ieee80211_channel *chan, uint32_t duration,
	uint64_t *cookie)
{
	struct p2p_roc_req roc_req = {0};
	struct wlan_objmgr_psoc *psoc;
	uint8_t vdev_id;
	bool ok;
	int ret;
	struct wlan_objmgr_pdev *pdev = NULL;

	if (!vdev) {
		osif_err("invalid vdev object");
		return -EINVAL;
	}

	if (!chan) {
		osif_err("invalid channel");
		return -EINVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	vdev_id = wlan_vdev_get_id(vdev);
	pdev = wlan_vdev_get_pdev(vdev);

	if (!psoc) {
		osif_err("psoc handle is NULL");
		return -EINVAL;
	}

	roc_req.chan_freq = chan->center_freq;
	roc_req.duration = duration;
	roc_req.vdev_id = (uint32_t)vdev_id;

	ret = policy_mgr_is_chan_ok_for_dnbs(psoc, chan->center_freq, &ok);
	if (QDF_IS_STATUS_ERROR(ret)) {
		osif_err("policy_mgr_is_chan_ok_for_dnbs():ret:%d",
			 ret);
		return -EINVAL;
	}

	if (!ok) {
		osif_err("channel%d not OK for DNBS", roc_req.chan_freq);
		return -EINVAL;
	}

	return qdf_status_to_os_return(
		ucfg_p2p_roc_req(psoc, &roc_req, cookie));
}

int wlan_cfg80211_cancel_roc(struct wlan_objmgr_vdev *vdev,
		uint64_t cookie)
{
	struct wlan_objmgr_psoc *psoc;

	if (!vdev) {
		osif_err("invalid vdev object");
		return -EINVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("psoc handle is NULL");
		return -EINVAL;
	}

	return qdf_status_to_os_return(
		ucfg_p2p_roc_cancel_req(psoc, cookie));
}

int wlan_cfg80211_mgmt_tx(struct wlan_objmgr_vdev *vdev,
		struct ieee80211_channel *chan, bool offchan,
		unsigned int wait,
		const uint8_t *buf, uint32_t len, bool no_cck,
		bool dont_wait_for_ack, uint64_t *cookie)
{
	struct p2p_mgmt_tx mgmt_tx = {0};
	struct wlan_objmgr_psoc *psoc;
	uint8_t vdev_id;
	qdf_freq_t chan_freq = 0;
	struct wlan_objmgr_pdev *pdev = NULL;
	if (!vdev) {
		osif_err("invalid vdev object");
		return -EINVAL;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (chan)
		chan_freq = chan->center_freq;
	else
		osif_debug("NULL chan, set channel to 0");

	psoc = wlan_vdev_get_psoc(vdev);
	vdev_id = wlan_vdev_get_id(vdev);
	if (!psoc) {
		osif_err("psoc handle is NULL");
		return -EINVAL;
	}

	/**
	 * When offchannel time is more than MAX_OFFCHAN_TIME_FOR_DNBS,
	 * allow offchannel only if Do_Not_Switch_Channel is not set.
	 */
	if (wait > MAX_OFFCHAN_TIME_FOR_DNBS) {
		int ret;
		bool ok;

		ret = policy_mgr_is_chan_ok_for_dnbs(psoc, chan_freq, &ok);
		if (QDF_IS_STATUS_ERROR(ret)) {
			osif_err("policy_mgr_is_chan_ok_for_dnbs():ret:%d",
				 ret);
			return -EINVAL;
		}
		if (!ok) {
			osif_err("Rejecting mgmt_tx for channel:%d as DNSC is set",
				 chan_freq);
			return -EINVAL;
		}
	}

	mgmt_tx.vdev_id = (uint32_t)vdev_id;
	mgmt_tx.chan_freq = chan_freq;
	mgmt_tx.wait = wait;
	mgmt_tx.len = len;
	mgmt_tx.no_cck = (uint32_t)no_cck;
	mgmt_tx.dont_wait_for_ack = (uint32_t)dont_wait_for_ack;
	mgmt_tx.off_chan = (uint32_t)offchan;
	mgmt_tx.buf = buf;

	return qdf_status_to_os_return(
		ucfg_p2p_mgmt_tx(psoc, &mgmt_tx, cookie, pdev));
}

int wlan_cfg80211_mgmt_tx_cancel(struct wlan_objmgr_vdev *vdev,
	uint64_t cookie)
{
	struct wlan_objmgr_psoc *psoc;

	if (!vdev) {
		osif_err("invalid vdev object");
		return -EINVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("psoc handle is NULL");
		return -EINVAL;
	}

	return qdf_status_to_os_return(
		ucfg_p2p_mgmt_tx_cancel(psoc, vdev, cookie));
}

#ifdef FEATURE_WLAN_SUPPORT_USD
int osif_p2p_send_usd_params(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			     const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_USD_MAX + 1];
	struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_USD_CHAN_CONFIG_MAX + 1];
	struct p2p_usd_attr_params *usd_param;
	QDF_STATUS status;
	uint8_t freq_config;
	int ret = -EINVAL;

	/* Parse and fetch USD params*/
	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_USD_MAX,
				    data, data_len, p2p_usd_attr_policy)) {
		osif_err("Invalid USD vendor command attributes");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_USD_OP_TYPE]) {
		osif_err("P2P USD OP type parse failed");
		return -EINVAL;
	}

	usd_param = qdf_mem_malloc(sizeof(*usd_param));
	if (!usd_param)
		return -ENOMEM;

	qdf_mem_zero(usd_param, sizeof(*usd_param));

	usd_param->op_type =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_USD_OP_TYPE]);

	if (usd_param->op_type == QCA_WLAN_VENDOR_USD_OP_TYPE_FLUSH)
		goto end;

	if (!tb[QCA_WLAN_VENDOR_ATTR_USD_INSTANCE_ID]) {
		osif_err("P2P instance ID parse failed");
		goto mem_free;
	}

	usd_param->instance_id =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_USD_INSTANCE_ID]);

	osif_debug("op type %d instance id %d", usd_param->op_type,
		   usd_param->instance_id);

	if (tb[QCA_WLAN_VENDOR_ATTR_USD_SSI]) {
		usd_param->ssi.len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_USD_SSI]);

		usd_param->ssi.data = qdf_mem_malloc(
						sizeof(*usd_param->ssi.data));
		if (!usd_param->ssi.data)
			goto mem_free;

		qdf_mem_copy(usd_param->ssi.data,
			     nla_data(tb[QCA_WLAN_VENDOR_ATTR_USD_SSI]),
			     usd_param->ssi.len);

		osif_debug("SSI dump:");
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
				   usd_param->ssi.data, usd_param->ssi.len);
	}

	/**
	 * no need to parse other attributes for the OP type other than
	 * Publish and Subscribe
	 */
	if (usd_param->op_type != QCA_WLAN_VENDOR_USD_OP_TYPE_PUBLISH &&
	    usd_param->op_type != QCA_WLAN_VENDOR_USD_OP_TYPE_SUBSCRIBE)
		goto end;

	if (!tb[QCA_WLAN_VENDOR_ATTR_USD_SRC_ADDR]) {
		osif_err("P2P MAC address parse failed");
		goto mem_free;
	}

	qdf_mem_copy(usd_param->p2p_mac_addr.bytes,
		     nla_data(tb[QCA_WLAN_VENDOR_ATTR_USD_SRC_ADDR]),
		     QDF_MAC_ADDR_SIZE);

	if (!tb[QCA_WLAN_VENDOR_ATTR_USD_SERVICE_ID]) {
		osif_err("P2P service ID parse failed");
		goto mem_free;
	}

	usd_param->service_info.len =
			nla_len(tb[QCA_WLAN_VENDOR_ATTR_USD_SERVICE_ID]);

	usd_param->service_info.service_id =
			qdf_mem_malloc(
				sizeof(*usd_param->service_info.service_id));
	if (!usd_param->service_info.service_id)
		goto mem_free;

	qdf_mem_copy(usd_param->service_info.service_id,
		     nla_data(tb[QCA_WLAN_VENDOR_ATTR_USD_SERVICE_ID]),
		     usd_param->service_info.len);

	osif_debug("service id dump:");
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
			   usd_param->service_info.service_id,
			   usd_param->service_info.len);

	if (!tb[QCA_WLAN_VENDOR_ATTR_USD_SERVICE_PROTOCOL_TYPE]) {
		osif_err("P2P service protocol type parse failed");
		goto mem_free;
	}

	usd_param->service_info.protocol_type =
	nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_USD_SERVICE_PROTOCOL_TYPE]);

	osif_debug("service protocol type %d",
		   usd_param->service_info.protocol_type);

	freq_config = QCA_WLAN_VENDOR_ATTR_USD_CHAN_CONFIG;
	if (!tb[freq_config]) {
		osif_err("freq config parse failed");
		goto mem_free;
	}

	if (wlan_cfg80211_nla_parse_nested(
				tb2, QCA_WLAN_VENDOR_ATTR_USD_CHAN_CONFIG_MAX,
				tb[freq_config], p2p_usd_chan_config_policy)) {
		osif_err("Failed to parse channel config");
		goto mem_free;
	}

	freq_config = QCA_WLAN_VENDOR_ATTR_USD_CHAN_CONFIG_DEFAULT_FREQ;

	if (!tb2[freq_config]) {
		osif_err("P2P freq config default freq parse fail");
		goto mem_free;
	}

	usd_param->freq_config.default_freq = nla_get_u32(tb2[freq_config]);

	freq_config = QCA_WLAN_VENDOR_ATTR_USD_CHAN_CONFIG_FREQ_LIST;

	if (tb2[freq_config]) {
		usd_param->freq_config.freq_list.len =
						nla_len(tb2[freq_config]);

		usd_param->freq_config.freq_list.freq =
			qdf_mem_malloc(
				sizeof(*usd_param->freq_config.freq_list.freq));
		if (!usd_param->freq_config.freq_list.freq)
			goto mem_free;

		qdf_mem_copy(usd_param->freq_config.freq_list.freq,
			     nla_data(tb2[freq_config]),
			     usd_param->freq_config.freq_list.len);

		osif_debug("freq list dump:");
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
				   usd_param->freq_config.freq_list.freq,
				   usd_param->freq_config.freq_list.len);
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_USD_ELEMENT_CONTAINER]) {
		osif_err("fail to fetch P2P USD frame");
		goto mem_free;
	}

	usd_param->frame.len =
		nla_len(tb[QCA_WLAN_VENDOR_ATTR_USD_ELEMENT_CONTAINER]);

	usd_param->frame.data = qdf_mem_malloc(sizeof(*usd_param->frame.data));
	if (!usd_param->frame.data)
		goto mem_free;

	qdf_mem_copy(
		usd_param->frame.data,
		nla_data(tb[QCA_WLAN_VENDOR_ATTR_USD_ELEMENT_CONTAINER]),
		usd_param->frame.len);

	osif_debug("Frame dump:");
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
			   usd_param->frame.data, usd_param->frame.len);

	if (!tb[QCA_WLAN_VENDOR_ATTR_USD_TTL]) {
		osif_err("fail to fetch P2P TTL");
		goto mem_free;
	}

	usd_param->ttl = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_USD_TTL]);

end:
	usd_param->vdev_id = vdev_id;
	status = ucfg_p2p_send_usd_params(psoc, usd_param);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("fail to send P2P USD params");
		goto mem_free;
	}

	osif_debug("P2P USD request success");
	ret = 0;

mem_free:
	if (usd_param->ssi.data)
		qdf_mem_free(usd_param->ssi.data);
	if (usd_param->service_info.service_id)
		qdf_mem_free(usd_param->service_info.service_id);
	if (usd_param->freq_config.freq_list.freq)
		qdf_mem_free(usd_param->freq_config.freq_list.freq);
	if (usd_param->frame.data)
		qdf_mem_free(usd_param->frame.data);

	qdf_mem_free(usd_param);

	return ret;
}
#endif /* FEATURE_WLAN_SUPPORT_USD */
