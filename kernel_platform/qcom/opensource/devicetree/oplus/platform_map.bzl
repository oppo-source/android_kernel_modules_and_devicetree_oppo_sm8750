_platform_map = {
    "sun": {
        "dtb_list": [
            # keep sorted
            {"name": "sun.dtb"},
            {
                "name": "sunp.dtb",
                "apq": True,
            },
            {
                "name": "sunp-v2.dtb",
                "apq": True,
            },
            {"name": "sun-v2.dtb"},
            {"name": "sun-tp.dtb"},
            {"name": "sun-tp-v2.dtb"},
            {
                "name": "sunp-tp.dtb",
                "apq": True,
            },
            {
                "name": "sunp-tp-v2.dtb",
                "apq": True,
            },
        ],
        "dtbo_list": [
            # keep sorted
            {"name": "petrel-24001-sun-overlay.dtbo"},
            {"name": "petrel-24201-sun-overlay.dtbo"},
            {"name": "petrel-24001-sun-overlay-T0.dtbo"},
            {"name": "petrel-24201-sun-overlay-T0.dtbo"},
            {"name": "petrel-24001-sun-overlay-EVT1.dtbo"},
            {"name": "petrel-24002-sun-overlay-EVT1.dtbo"},
            {"name": "petrel-24201-sun-overlay-EVT1.dtbo"},
            {"name": "petrel-24001-sun-overlay-EVT2.dtbo"},
            {"name": "petrel-24002-sun-overlay-EVT2.dtbo"},
            {"name": "petrel-24201-sun-overlay-EVT2.dtbo"},
            {"name": "petrel-24002-sun-overlay.dtbo"},
            {"name": "zhufeng-24019-sun-overlay.dtbo"},
            {"name": "zhufeng-24018-sun-overlay.dtbo"},
            {"name": "zhufeng-24019-sun-overlay-PreT0.dtbo"},
            {"name": "zhufeng-24018-sun-overlay-PreT0.dtbo"},
            {"name": "erhai-24924-sun-overlay.dtbo"},
            {"name": "erhai-24924-sun-overlay-DVT.dtbo"},
        ],
    },
    "tuna": {
        "dtb_list": [

        ],
        "dtbo_list": [
            {

            },
        ],
    },
    "kera": {
        "dtb_list": [

        ],
        "dtbo_list": [
            {

            },
        ],
    },
    "sun-tuivm": {
        "dtb_list": [
            # keep sorted

        ],
    },
    "sun-oemvm": {
        "dtb_list": [
            # keep sorted

        ],
    },
    "pineapple": {
        "dtb_list": [

        ],
        "dtbo_list": [

        ],
    },
    "pineapple-tuivm": {
        "dtb_list": [

        ],
    },
    "pineapple-oemvm": {
        "dtb_list": [

        ],
    },
    "monaco": {
        "dtb_list": [

        ],
        "dtbo_list": [
        ],
    },
    "parrot": {
        "dtb_list": [

        ],
        "dtbo_list": [

        ],
    },
    "sdxkova": {
         "dtb_list": [

          ],
         "dtbo_list": [

         ],
   },
}

def _get_dtb_lists(target, dt_overlay_supported):
    if not target in _platform_map:
        fail("{} not in device tree platform map!".format(target))

    ret = {
        "dtb_list": [],
        "dtbo_list": [],
    }

    for dtb_node in [target] + _platform_map[target].get("binary_compatible_with", []):
        ret["dtb_list"].extend(_platform_map[dtb_node].get("dtb_list", []))
        if dt_overlay_supported:
            ret["dtbo_list"].extend(_platform_map[dtb_node].get("dtbo_list", []))
        else:
            # Translate the dtbo list into dtbs we can append to main dtb_list
            for dtb in _platform_map[dtb_node].get("dtb_list", []):
                dtb_base = dtb["name"].replace(".dtb", "")
                for dtbo in _platform_map[dtb_node].get("dtbo_list", []):
                    if not dtbo.get("apq", True) and dtb.get("apq", False):
                        continue

                    dtbo_base = dtbo["name"].replace(".dtbo", "")
                    ret["dtb_list"].append({"name": "{}-{}.dtb".format(dtb_base, dtbo_base)})

    return ret

def get_dtb_list(target, dt_overlay_supported = True):
    return [dtb["name"] for dtb in _get_dtb_lists(target, dt_overlay_supported).get("dtb_list", [])]

def get_dtbo_list(target, dt_overlay_supported = True):
    return [dtb["name"] for dtb in _get_dtb_lists(target, dt_overlay_supported).get("dtbo_list", [])]
