/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DSI_PHY_H_
#define _DSI_PHY_H_

#include "dsi_defs.h"
#include "dsi_clk.h"
#include "dsi_pwr.h"
#include "dsi_phy_hw.h"
#include "dsi_pll.h"

struct dsi_ver_spec_info {
	enum dsi_phy_version version;
	u32 lane_cfg_count;
	u32 strength_cfg_count;
	u32 regulator_cfg_count;
	u32 timing_cfg_count;
};

/**
 * struct dsi_phy_power_info - digital and analog power supplies for DSI PHY
 * @digital:       Digital power supply for DSI PHY.
 * @phy_pwr:       Analog power supplies for DSI PHY to work.
 */
struct dsi_phy_power_info {
	struct dsi_regulator_info digital;
	struct dsi_regulator_info phy_pwr;
};

/**
 * enum phy_engine_state - define engine status for dsi phy.
 * @DSI_PHY_ENGINE_OFF:  Engine is turned off.
 * @DSI_PHY_ENGINE_ON:   Engine is turned on.
 * @DSI_PHY_ENGINE_MAX:  Maximum value.
 */
enum phy_engine_state {
	DSI_PHY_ENGINE_OFF = 0,
	DSI_PHY_ENGINE_ON,
	DSI_PHY_ENGINE_MAX,
};

/**
 * enum phy_ulps_return_type - define set_ulps return type for dsi phy.
 * @DSI_PHY_ULPS_HANDLED:      ulps is handled in phy.
 * @DSI_PHY_ULPS_NOT_HANDLED:  ulps is not handled in phy.
 * @DSI_PHY_ULPS_ERROR:        ulps request failed in phy.
 */
enum phy_ulps_return_type {
	DSI_PHY_ULPS_HANDLED = 0,
	DSI_PHY_ULPS_NOT_HANDLED,
	DSI_PHY_ULPS_ERROR,
};

/**
 * struct msm_dsi_phy - DSI PHY object
 * @pdev:              Pointer to platform device.
 * @index:             Instance id.
 * @name:              Name of the PHY instance.
 * @refcount:          Reference count.
 * @sync_en_refcount:  Reference count for each phy in sync mode.
 * @phy_lock:          Mutex for hardware and object access.
 * @ver_info:          Version specific phy parameters.
 * @hw:                DSI PHY hardware object.
 * @pwr_info:          Power information.
 * @cfg:               DSI phy configuration.
 * @clk_cb:	       structure containing call backs for clock control
 * @power_state:       True if PHY is powered on.
 * @dsi_phy_state:     PHY state information.
 * @mode:              Current mode.
 * @data_lanes:        Number of data lanes used.
 * @dst_format:        Destination format.
 * @pll:	       Pointer to PLL resource.
 * @allow_phy_power_off: True if PHY is allowed to power off when idle
 * @regulator_min_datarate_bps: Minimum per lane data rate to turn on regulator
 * @regulator_required: True if phy regulator is required
 * @dfps_trigger_mdpintf_flush: mdp intf flush controls dfps trigger.
 * @dsi_phy_shared:	True if phy is shared between dual displays.
 */
struct msm_dsi_phy {
	struct platform_device *pdev;
	int index;
	const char *name;
	u32 refcount;
	u32 sync_en_refcount;
	struct mutex phy_lock;

	const struct dsi_ver_spec_info *ver_info;
	struct dsi_phy_hw hw;

	struct dsi_phy_power_info pwr_info;

	struct dsi_phy_cfg cfg;
	struct clk_ctrl_cb clk_cb;

	enum phy_engine_state dsi_phy_state;
	bool power_state;
	struct dsi_mode_info mode;
	enum dsi_data_lanes data_lanes;
	enum dsi_pixel_format dst_format;

	struct dsi_pll_resource *pll;

	bool allow_phy_power_off;
	u32 regulator_min_datarate_bps;
	bool regulator_required;
	bool dfps_trigger_mdpintf_flush;
	bool dsi_phy_shared;
};

/**
 * dsi_phy_check_resource() - check if DSI PHY is probed
 * @of_node:    of_node of the DSI PHY.
 *
 * Checks if the DSI PHY has been probed and is available.
 *
 * Return: status of DSI PHY
 */
bool dsi_phy_check_resource(struct device_node *of_node);

/**
 * dsi_phy_get() - get a dsi phy handle from device node
 * @of_node:           device node for dsi phy controller
 *
 * Gets the DSI PHY handle for the corresponding of_node. The ref count is
 * incremented to one all subsequents get will fail until the original client
 * calls a put.
 *
 * Return: DSI PHY handle or an error code.
 */
struct msm_dsi_phy *dsi_phy_get(struct device_node *of_node);

/**
 * dsi_phy_put() - release dsi phy handle
 * @dsi_phy:              DSI PHY handle.
 *
 * Release the DSI PHY hardware. Driver will clean up all resources and puts
 * back the DSI PHY into reset state.
 */
void dsi_phy_put(struct msm_dsi_phy *dsi_phy);

/**
 * dsi_phy_get_version() - returns dsi phy version
 * @dsi_phy:              DSI PHY handle.
 *
 * Return: phy version
 */
int dsi_phy_get_version(struct msm_dsi_phy *phy);

/**
 * dsi_phy_drv_init() - initialize dsi phy driver
 * @dsi_phy:         DSI PHY handle.
 *
 * Initializes DSI PHY driver. Should be called after dsi_phy_get().
 *
 * Return: error code.
 */
int dsi_phy_drv_init(struct msm_dsi_phy *dsi_phy);

/**
 * dsi_phy_drv_deinit() - de-initialize dsi phy driver
 * @dsi_phy:          DSI PHY handle.
 *
 * Release all resources acquired by dsi_phy_drv_init().
 *
 * Return: error code.
 */
int dsi_phy_drv_deinit(struct msm_dsi_phy *dsi_phy);

/**
 * dsi_phy_validate_mode() - validate a display mode
 * @dsi_phy:            DSI PHY handle.
 * @mode:               Mode information.
 *
 * Validation will fail if the mode cannot be supported by the PHY driver or
 * hardware.
 *
 * Return: error code.
 */
int dsi_phy_validate_mode(struct msm_dsi_phy *dsi_phy,
			  struct dsi_mode_info *mode);

/**
 * dsi_phy_set_power_state() - enable/disable dsi phy power supplies
 * @dsi_phy:               DSI PHY handle.
 * @enable:                Boolean flag to enable/disable.
 *
 * Return: error code.
 */
int dsi_phy_set_power_state(struct msm_dsi_phy *dsi_phy, bool enable);

/**
 * dsi_phy_enable() - enable DSI PHY hardware
 * @dsi_phy:            DSI PHY handle.
 * @config:             DSI host configuration.
 * @pll_source:         Source PLL for PHY clock.
 * @skip_validation:    Validation will not be performed on parameters.
 * @skip_op:            Skip re-enabling dsi phy hw during usecases like
 *                      cont-splash/trusted-vm if set to true.
 *
 * Validates and enables DSI PHY.
 *
 * Return: error code.
 */
int dsi_phy_enable(struct msm_dsi_phy *dsi_phy,
		   struct dsi_host_config *config,
		   enum dsi_phy_pll_source pll_source,
		   bool skip_validation,
		   bool skip_op);

/**
 * dsi_phy_disable() - disable DSI PHY hardware.
 * @phy:            DSI PHY handle.
 * @skip_op:        Skip disabling dsi phy hw during usecases like
 *                  trusted-vm if set to true.
 *
 * Return: error code.
 */
int dsi_phy_disable(struct msm_dsi_phy *phy, bool skip_op);

/**
 * dsi_phy_set_ulps() - set ulps state for DSI pHY
 * @phy:          DSI PHY handle
 * @config:	  DSi host configuration information.
 * @enable:	  Enable/Disable
 * @clamp_enabled: mmss_clamp enabled/disabled
 *
 * Return: error code.
 */
int dsi_phy_set_ulps(struct msm_dsi_phy *phy,  struct dsi_host_config *config,
		bool enable, bool clamp_enabled);

/**
 * dsi_phy_clk_cb_register() - Register PHY clock control callback
 * @phy:          DSI PHY handle
 * @clk_cb:	  Structure containing call back for clock control
 *
 * Return: error code.
 */
int dsi_phy_clk_cb_register(struct msm_dsi_phy *phy,
	struct clk_ctrl_cb *clk_cb);

/**
 * dsi_phy_idle_ctrl() - enable/disable DSI PHY during idle screen
 * @phy:          DSI PHY handle
 * @enable:       boolean to specify PHY enable/disable.
 *
 * Return: error code.
 */
int dsi_phy_idle_ctrl(struct msm_dsi_phy *phy, bool enable);

/**
 * dsi_phy_set_clamp_state() - configure clamps for DSI lanes
 * @phy:        DSI PHY handle.
 * @enable:     boolean to specify clamp enable/disable.
 *
 * Return: error code.
 */
int dsi_phy_set_clamp_state(struct msm_dsi_phy *phy, bool enable);

/**
 * dsi_phy_set_clk_freq() - set DSI PHY clock frequency setting
 * @phy:          DSI PHY handle
 * @clk_freq:     link clock frequency
 *
 * Return: error code.
 */
int dsi_phy_set_clk_freq(struct msm_dsi_phy *phy,
		struct link_clk_freq *clk_freq);

/**
 * dsi_phy_set_timing_params() - timing parameters for the panel
 * @phy:          DSI PHY handle
 * @timing:       array holding timing params.
 * @size:         size of the array.
 * @commit:		  boolean to indicate if programming PHY HW registers is
 *				  required
 *
 * When PHY timing calculator is not implemented, this array will be used to
 * pass PHY timing information.
 *
 * Return: error code.
 */
int dsi_phy_set_timing_params(struct msm_dsi_phy *phy,
			      u32 *timing, u32 size, bool commit);

/**
 * dsi_phy_lane_reset() - Reset DSI PHY lanes in case of error
 * @phy:	DSI PHY handle
 *
 * Return: error code.
 */
int dsi_phy_lane_reset(struct msm_dsi_phy *phy);

/**
 * dsi_phy_toggle_resync_fifo() - toggle resync retime FIFO
 * @phy:          DSI PHY handle
 *
 * Toggle the resync retime FIFO to synchronize the data paths.
 * This should be done everytime there is a change in the link clock
 * rate
 */
void dsi_phy_toggle_resync_fifo(struct msm_dsi_phy *phy);

/**
 * dsi_phy_reset_clk_en_sel() - reset clk_en_select on cmn_clk_cfg1 register
 * @phy:          DSI PHY handle
 *
 * After toggling resync fifo regiater, clk_en_sel bit on cmn_clk_cfg1
 * register has to be reset
 */
void dsi_phy_reset_clk_en_sel(struct msm_dsi_phy *phy);

/**
 * dsi_phy_drv_register() - register platform driver for dsi phy
 */
void dsi_phy_drv_register(void);

/**
 * dsi_phy_drv_unregister() - unregister platform driver
 */
void dsi_phy_drv_unregister(void);

/**
 * dsi_phy_update_phy_timings() - Update dsi phy timings
 * @phy:	DSI PHY handle
 * @config:	DSI Host config parameters
 * @use_mode_bit_clk:	Boolean to indicate whether reacalculate dsi
 *			bitclk or use the existing bitclk(for dynamic clk case).
 *
 * Return: error code.
 */
int dsi_phy_update_phy_timings(struct msm_dsi_phy *phy,
		struct dsi_host_config *config, bool use_mode_bit_clk);

/**
 * dsi_phy_config_dynamic_refresh() - Configure dynamic refresh registers
 * @phy:	DSI PHY handle
 * @delay:	pipe delays for dynamic refresh
 * @is_master:	Boolean to indicate if for master or slave
 */
void dsi_phy_config_dynamic_refresh(struct msm_dsi_phy *phy,
				    struct dsi_dyn_clk_delay *delay,
				    bool is_master);
/**
 * dsi_phy_dynamic_refresh_trigger_sel() - dynamic refresh trigger select.
 * @phy:	DSI PHY handle
 * @is_master:	Boolean to indicate if for master or slave.
 */
void dsi_phy_dynamic_refresh_trigger_sel(struct msm_dsi_phy *phy,
		bool is_master);

/**
 * dsi_phy_dynamic_refresh_trigger() - trigger dynamic refresh
 * @phy:	DSI PHY handle
 * @is_master:	Boolean to indicate if for master or slave.
 */
void dsi_phy_dynamic_refresh_trigger(struct msm_dsi_phy *phy, bool is_master);

/**
 * dsi_phy_dynamic_refresh_clear() - clear dynamic refresh config
 * @phy:	DSI PHY handle
 */
void dsi_phy_dynamic_refresh_clear(struct msm_dsi_phy *phy);

/**
 * dsi_phy_dyn_refresh_cache_phy_timings - cache the phy timings calculated
 *				as part of dynamic refresh.
 * @phy:	   DSI PHY Handle.
 * @dst:	   Pointer to cache location.
 * @size:	   Number of phy lane settings.
 */
int dsi_phy_dyn_refresh_cache_phy_timings(struct msm_dsi_phy *phy, u32 *dst,
		u32 size);

/**
 * dsi_phy_set_continuous_clk() - API to set/unset force clock lane HS request.
 * @phy:	DSI PHY Handle.
 * @enable:	variable to control continuous clock.
 */
void dsi_phy_set_continuous_clk(struct msm_dsi_phy *phy, bool enable);

/**
 * dsi_phy_get_io_resources() - reads associated register range
 *
 * @io_res:	 pointer to msm_io_res struct to populate the ranges
 *
 * Return: error code.
 */
int dsi_phy_get_io_resources(struct msm_io_res *io_res);

/**
 * dsi_phy_configure() -   Configure DSI PHY PLL
 * @dsi_phy:               DSI PHY handle.
 * @commit:		   boolean to specify if calculated PHY configuration
 *			   needs to be committed. Set to false in case of
 *			   dynamic clock switch.
 *
 * Return: error code.
 */
int dsi_phy_configure(struct msm_dsi_phy *dsi_phy, bool commit);

/**
 * dsi_phy_pll_toggle() -   Toggle DSI PHY PLL
 * @dsi_phy:                DSI PHY handle.
 * @prepare:		    specifies if PLL needs to be turned on or not.
 *
 * Return: error code.
 */
int dsi_phy_pll_toggle(struct msm_dsi_phy *dsi_phy, bool prepare);

/**
 * dsi_phy_dynclk_configure() -   Configure DSI PHY PLL during dynamic clock
 * @dsi_phy:			  DSI PHY handle.
 *
 * Return: error code.
 */
int dsi_phy_dynclk_configure(struct msm_dsi_phy *phy);

/**
 * dsi_phy_pll_parse_dfps_data() - parse dfps data for PLL
 * @phy:	DSI PHY handle
 */
void dsi_phy_pll_parse_dfps_data(struct msm_dsi_phy *phy);
#endif /* _DSI_PHY_H_ */
