//		 selinux_restore_context();  //分析四


// The files and directories that were created before initial sepolicy load or
// files on ramdisk need to have their security context restored to the proper
// value. This must happen before /dev is populated by ueventd.
static void selinux_restore_context() {
    LOG(INFO) << "Running restorecon...";
    selinux_android_restorecon("/dev", 0);
    selinux_android_restorecon("/dev/kmsg", 0);
    selinux_android_restorecon("/dev/socket", 0);
    selinux_android_restorecon("/dev/random", 0);
    selinux_android_restorecon("/dev/urandom", 0);
    selinux_android_restorecon("/dev/__properties__", 0);

    selinux_android_restorecon("/file_contexts.bin", 0);
    selinux_android_restorecon("/plat_file_contexts", 0);
    selinux_android_restorecon("/nonplat_file_contexts", 0);
    selinux_android_restorecon("/plat_property_contexts", 0);
    selinux_android_restorecon("/nonplat_property_contexts", 0);
    selinux_android_restorecon("/plat_seapp_contexts", 0);
    selinux_android_restorecon("/nonplat_seapp_contexts", 0);
    selinux_android_restorecon("/plat_service_contexts", 0);
    selinux_android_restorecon("/nonplat_service_contexts", 0);
    selinux_android_restorecon("/plat_hwservice_contexts", 0);
    selinux_android_restorecon("/nonplat_hwservice_contexts", 0);
    selinux_android_restorecon("/sepolicy", 0);
    selinux_android_restorecon("/vndservice_contexts", 0);

    //	@external/selinux/libselinux/include/selinux/android.h:47:#define SELINUX_ANDROID_RESTORECON_RECURSE  4
    selinux_android_restorecon("/sys", SELINUX_ANDROID_RESTORECON_RECURSE);
    selinux_android_restorecon("/dev/block", SELINUX_ANDROID_RESTORECON_RECURSE);
    selinux_android_restorecon("/dev/device-mapper", 0);

    selinux_android_restorecon("/sbin/mke2fs_static", 0);
    selinux_android_restorecon("/sbin/e2fsdroid_static", 0);
}
