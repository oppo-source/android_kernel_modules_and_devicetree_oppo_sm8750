load(":audio_modules.bzl", "audio_modules")
load(":module_mgr.bzl", "define_target_modules")

def define_sun():
    define_target_modules(
        target = "sun",
        variants = ["consolidate", "perf"],
        registry = audio_modules,
        modules = [
            "q6_dlkm",
            "spf_core_dlkm",
            "audpkt_ion_dlkm",
            "q6_notifier_dlkm",
            "adsp_loader_dlkm",
            "audio_prm_dlkm",
            "q6_pdr_dlkm",
            "gpr_dlkm",
            "audio_pkt_dlkm",
            "pinctrl_lpi_dlkm",
            "swr_dlkm",
            "swr_ctrl_dlkm",
            "snd_event_dlkm",
            "machine_dlkm",
            "wcd_core_dlkm",
            "mbhc_dlkm",
            "swr_dmic_dlkm",
            "wcd9xxx_dlkm",
            "swr_haptics_dlkm",
            "stub_dlkm",
            "hdmi_dlkm",
            "lpass_cdc_dlkm",
            "lpass_cdc_wsa_macro_dlkm",
            "lpass_cdc_wsa2_macro_dlkm",
            "lpass_cdc_va_macro_dlkm",
            "lpass_cdc_rx_macro_dlkm",
            "lpass_cdc_tx_macro_dlkm",
            "wsa883x_dlkm",
            "wsa884x_dlkm",
            "wcd938x_dlkm",
            "wcd938x_slave_dlkm",
            "wcd939x_dlkm",
            "wcd939x_slave_dlkm",
#ifdef OPLUS_ARCH_EXTENDS
#add for oplus audio extends driver
            "oplus_audio_extend",
            "oplus_audio_aw882xx",
            "oplus_audio_tfa98xx_v6",
            "oplus_audio_sipa",
            "oplus_audio_sipa_tuning",
#endif /* OPLUS_ARCH_EXTENDS */
#ifdef CONFIG_AUDIO_DAEMON_KERNEL_QCOM
#add for oplus audio daemon kernel
            "oplus_audio_daemon",
#endif /* CONFIG_AUDIO_DAEMON_KERNEL_QCOM */
            "lpass_bt_swr_dlkm",
			"qmp_dlkm"
        ],
        config_options = [
            "CONFIG_SND_SOC_SUN",
            "CONFIG_SND_SOC_MSM_QDSP6V2_INTF",
            "CONFIG_MSM_QDSP6_SSR",
            "CONFIG_DIGITAL_CDC_RSC_MGR",
            "CONFIG_SOUNDWIRE_MSTR_CTRL",
            "CONFIG_SWRM_VER_2P0",
            "CONFIG_WCD9XXX_CODEC_CORE_V2",
            "CONFIG_MSM_CDC_PINCTRL",
            "CONFIG_SND_SOC_WCD_IRQ",
            "CONFIG_SND_SOC_WCD9XXX_V2",
            "CONFIG_SND_SOC_WCD_MBHC_ADC",
            "CONFIG_LPASS_BT_SWR",
            "CONFIG_AUDIO_BTFM_PROXY",
            "CONFIG_MSM_EXT_DISPLAY",
            "CONFIG_SND_SOC_QMP",
#ifdef OPLUS_ARCH_EXTENDS
#add for oplus audio extends driver
            "OPLUS_ARCH_EXTENDS",
            "OPLUS_FEATURE_SPEAKER_MUTE",
            "OPLUS_FEATURE_RINGTONE_HAPTIC",
#endif /* OPLUS_ARCH_EXTENDS */
        ]
    )
