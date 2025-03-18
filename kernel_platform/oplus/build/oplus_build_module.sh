#!/bin/bash

source kernel_platform/oplus/build/oplus_setup.sh $1 $2
init_build_environment

if [ "$3" = "" ]; then
    echo "Please input the ko directory:"
    print_module_help
    read ko_path
else
    ko_path=$3
fi

# in tree ko build: /SM8650_U_MASTER/vnd/kernel_platform/msm-kernel/pineapple.bzl
function build_in_tree_ko () {
    echo "entor build_in_tree_ko"
    cd ${TOPDIR}/kernel_platform
    if ./tools/bazel query //msm-kernel:${variants_platform}_${variants_type}/${ko_path}
    then
        echo "find the ko successfully!!!"
        ./tools/bazel build //msm-kernel:${variants_platform}_${variants_type}/${ko_path}
    else
        echo "the directory you input is not exit or not right, please exit and try again!!!"
    fi

    echo "you can push below ko or repack *.img by tools or build local *.img to test"
    ls -lh ${INTREE_MODULE_OUT}/${ko_path}
    echo "last ko will copy to dist dir"
    echo "${KERNEL_OUT}"
    echo "${DIST_INSTALL}"

    file=$(find ${KERNEL_OUT} -type f -name $(basename "${INTREE_MODULE_OUT}/${ko_path}"))
    if [ -n "$file" ]; then
    echo "拷贝KO："${INTREE_MODULE_OUT}/${ko_path}" -> $file"
    cp "${INTREE_MODULE_OUT}/${ko_path}" "$file"
    fi

    cp ${INTREE_MODULE_OUT}/${ko_path}  ${DIST_INSTALL}

    echo "exit build_in_tree_ko"
}

# out of tree ko build: SM8650_U_MASTER/vnd/kernel_platform/oplus/config/modules.ext.oplus
function build_out_of_tree_ko () {
    echo "build_out_of_tree_ko"
    cd ${TOPDIR}/kernel_platform
    if build_target=$(./tools/bazel query --ui_event_filters=-info --noshow_progress "filter('${variants_platform}_${variants_type}_.*_dist$', //${ko_path}/...)") && [ -n "$build_target" ]
    then
        echo "find the ko successfully!!!"
        ./tools/bazel run --//msm-kernel:skip_abi=true --user_kmi_symbol_lists=//msm-kernel:android/abi_gki_aarch64_qcom --ignore_missing_projects ${build_target} -- --dist_dir=${TOPDIR}/kernel_platform/out/android15-6.6/msm-kernel/../../${ko_path}
    else
        echo "the directory you input is not exist or not right, please exit and try again!!!"
    fi

    echo "you can push below ko or repack *.img by tools or build local *.img to test"
    ls -lh ${ACKDIR}/out/${ko_path}/*.ko
    echo "last ko will copy to dist dir"
    echo "${KERNEL_OUT}"
    echo "${DIST_INSTALL}"
    for file1 in $(find ${ACKDIR}/out/${ko_path}/ -type f -name "*.ko"); do
      filename=$(basename "$file1")
      file2=$(find ${KERNEL_OUT} -type f -name "$filename")

      if [ -n "$file2" ]; then
        echo "拷贝KO："$file1" -> $file2"
        cp "$file1" "$file2"
      fi
    done

    cp ${ACKDIR}/out/${ko_path}/*.ko  ${DIST_INSTALL}
    echo "exit build_out_of_tree_ko"
}

function build_ko {
    if [[ "$ko_path" == *".ko" ]]; then
        echo "-------------------------this is in tree ko-------------------------"
        build_in_tree_ko
    elif [ -d "$ko_path" ]; then
        echo "------------------------this is out of tree ko----------------------"
        build_out_of_tree_ko
    else
        echo "Invalid parameter. Please provide a path to a .ko file or a directory."
        exit 1
    fi
}
build_ko
