
查看文件权限：ls -Z
u:object_r:hal_allocator_default_exec:s0 android.hidl.allocator@1.0-service

“u:object_r:hal_allocator_default_exec:s0” 表明 SELinux用户、SELinux角色、类型 和 安全级别分别为u、object_r、rootfs和s0

查看运行进程权限：ps -Zef

u:r:hal_allocator_default:s0   system         251     1 0 00:00:22 ?     00:00:00 android.hidl.allocator@1.0-service
u:r:hal_audio_default:s0       audioserver    252     1 0 00:00:22 ?     00:00:00 android.hardware.audio@2.0-service
u:r:hal_bluetooth_default:s0   bluetooth      253     1 0 00:00:22 ?     00:00:00 android.hardware.bluetooth@1.0-service
u:r:hal_camera_default:s0      cameraserver   254     1 0 00:00:22 ?     00:00:00 android.hardware.camera.provider@2.4-service
u:r:hal_configstore_default:s0 system         255     1 0 00:00:22 ?     00:00:00 android.hardware.configstore@1.0-service
u:r:hal_gnss_default:s0        gps            256     1 0 00:00:22 ?     00:00:00 android.hardware.gnss@1.0-service
u:r:hal_graphics_allocator_default:s0 system  257     1 0 00:00:22 ?     00:00:02 android.hardware.graphics.allocator@2.0-service
u:r:hal_light_default:s0       system         258     1 0 00:00:22 ?     00:00:00 android.hardware.light@2.0-service
u:r:hal_memtrack_default:s0    system         259     1 0 00:00:22 ?     00:00:01 android.hardware.memtrack@1.0-service
u:r:hal_power_default:s0       system         260     1 0 00:00:22 ?     00:00:00 android.hardware.power@1.0-service
u:r:hal_sensors_default:s0     system         261     1 0 00:00:22 ?     00:00:00 android.hardware.sensors@1.0-service
u:r:hal_usb_default:s0         system         262     1 0 00:00:22 ?     00:00:00 android.hardware.usb@1.0-service
u:r:hal_wifi_default:s0        wifi           263     1 0 00:00:22 ?     00:00:00 android.hardware.wifi@1.0-servic



https://blog.csdn.net/innost/article/details/19299937
