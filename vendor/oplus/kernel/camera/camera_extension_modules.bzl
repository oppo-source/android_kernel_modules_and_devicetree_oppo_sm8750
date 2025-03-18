load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("//build/bazel_common_rules/dist:dist.bzl", "copy_to_dist_dir")
load("//msm-kernel:target_variants.bzl", "get_all_variants")

def _define_module(target, variant):
    tv = "{}_{}".format(target, variant)

    ddk_module(
        name = "{}_camera_extension".format(tv),
        out = "camera_extension.ko",
        srcs = native.glob([
            "camera_extension/**/*.h",
            "camera_extension/cam_sensor_module/**/*.h",
            "camera_extension/cam_sensor_module/cam_ois/**/*.h",
            "camera_extension/cam_sensor_module/cam_ois/fw_download_interface.h",
            "camera_extension/cam_sensor_module/cam_actuator/cam_actuator_custom.c",
            "camera_extension/cam_sensor_module/cam_sensor_io/cam_sensor_io_custom.c",
            "camera_extension/cam_sensor_module/cam_sensor_io/cam_sensor_util_custom.c",
            "camera_extension/cam_sensor_module/cam_eeprom/cam_eeprom_custom.c",
            "camera_extension/cam_sensor_module/cam_sensor/cam_sensor_custom.c",
            "camera_extension/cam_sensor_module/cam_ois/cam_ois_custom.c",
            "camera_extension/cam_sensor_module/cam_ois/fw_download_interface.c",
            "camera_extension/cam_sensor_module/cam_ois/SEM1217S/sem1217_fw.c",
            "camera_extension/cam_sensor_module/cam_ois/BU24721/bu24721_fw.c",
            "camera_extension/cam_sensor_module/cam_ois/DW9786/dw9786_fw.c",
            "camera_extension/cam_monitor/cam_monitor.c",
            "camera_extension/cam_utils/cam_debug.c",
            "camera_extension/main.c",
            "camera_extension/cam_sensor_module/cam_tof/cam_tof8801/tof8801_driver.c",
            "camera_extension/cam_sensor_module/cam_tof/cam_tof8801/tof8801_bootloader.c",
            "camera_extension/cam_sensor_module/cam_tof/cam_tof8801/tof_hex_interpreter.c",
            "camera_extension/cam_sensor_module/cam_tof/cam_tof8801/tof8801_app0.c",
            "camera_extension/cam_sensor_module/cam_tof/cam_tof8806/ams_i2c.c",
            "camera_extension/cam_sensor_module/cam_tof/cam_tof8806/tmf8806_driver.c",
            "camera_extension/cam_sensor_module/cam_tof/cam_tof8806/tmf8806_hex_interpreter.c",
            "camera_extension/cam_sensor_module/cam_tof/cam_tof8806/tmf8806_shim.c",
            "camera_extension/cam_sensor_module/cam_tof/cam_tof8806/tmf8806.c",
            "camera_extension/cam_sensor_module/cam_tof/cam_tof8806/tmf8806_image.c",
            "camera_extension/cam_sensor_module/cam_tof/cam_tof_common/tof_pdrv.c",
        ]),
        local_defines = ["OPLUS_FEATURE_CAMERA_COMMON", "FEATURE_ENABLE=1"],
        deps  = [
             "//msm-kernel:all_headers",
             "//vendor/qcom/opensource/camera-kernel:camera_headers",
             "//vendor/qcom/opensource/camera-kernel:camera_banner",
             "//vendor/qcom/opensource/camera-kernel:{}_camera".format(tv),
             ":camera_extension_headers",
        ],
        kernel_build = "//msm-kernel:{}".format(tv),
    )

    copy_to_dist_dir(
        name = "{}_camera_extension_dist".format(tv),
        data = [":{}_camera_extension".format(tv)],
        dist_dir = "out/target/product/{}/dlkm/lib/modules/".format(target),
        flat = True,
        wipe_dist_dir = False,
        allow_duplicate_filenames = False,
        mode_overrides = {"**/*": "644"},
    )

def define_camera_extension_modules():
    for (t, v) in get_all_variants():
        _define_module(t, v)
