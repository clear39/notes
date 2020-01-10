
//  @   hardware/interfaces/graphics/allocator/2.0/default/service.cpp

#define LOG_TAG "android.hardware.graphics.allocator@2.0-service"

#include <android/hardware/graphics/allocator/2.0/IAllocator.h>

#include <hidl/LegacySupport.h>

using android::hardware::defaultPassthroughServiceImplementation;
using android::hardware::graphics::allocator::V2_0::IAllocator;

int main() {
    return defaultPassthroughServiceImplementation<IAllocator>(4);
}

//  @   hardware/interfaces/graphics/allocator/2.0/default/passthrough.cpp
#include <allocator-passthrough/2.0/GrallocLoader.h>
#include <android/hardware/graphics/allocator/2.0/IAllocator.h>

using android::hardware::graphics::allocator::V2_0::IAllocator;
using android::hardware::graphics::allocator::V2_0::passthrough::GrallocLoader;

extern "C" IAllocator* HIDL_FETCH_IAllocator(const char* /* name */) {
    return GrallocLoader::load();
}



allocator@2.0-s  1663     system  cwd       DIR              253,0      4096          2 /
allocator@2.0-s  1663     system  rtd       DIR              253,0      4096          2 /
allocator@2.0-s  1663     system  txt       REG              253,1     68344        191 /vendor/bin/hw/android.hardware.graphics.allocator@2.0-service
allocator@2.0-s  1663     system  mem       REG              253,1     68344        191 /vendor/bin/hw/android.hardware.graphics.allocator@2.0-service
allocator@2.0-s  1663     system  mem       REG              253,1     69032        325 /vendor/lib64/hw/gralloc_viv.imx8.so
allocator@2.0-s  1663     system  mem       REG              253,1     68120        221 /vendor/lib64/libdrm_vivante.so
allocator@2.0-s  1663     system  mem       REG              253,0     68192        575 /system/lib64/libsync.so
allocator@2.0-s  1663     system  mem       REG              253,1     68216        339 /vendor/lib64/libedid.so
allocator@2.0-s  1663     system  mem       REG              253,1    134144        285 /vendor/lib64/libdrm_android.so
allocator@2.0-s  1663     system  mem       REG              253,1    135824        276 /vendor/lib64/nxp.hardware.display@1.0.so
allocator@2.0-s  1663     system  mem       REG              253,0     68192        516 /system/lib64/vndk-sp-28/libion.so
allocator@2.0-s  1663     system  mem       REG              253,1     68840        305 /vendor/lib64/hw/gralloc.imx8.so
allocator@2.0-s  1663     system  mem       REG              253,1    200720        296 /vendor/lib64/libfsldisplay.so
allocator@2.0-s  1663     system  mem       REG              253,0     68152        530 /system/lib64/vndk-sp-28/libhardware.so
allocator@2.0-s  1663     system  mem       REG              253,1     68840        320 /vendor/lib64/hw/android.hardware.graphics.allocator@2.0-impl.so
allocator@2.0-s  1663     system  mem       CHR              10,58                  211 /dev/hwbinder
allocator@2.0-s  1663     system  mem       REG              253,0     68304        609 /system/lib64/libnetd_client.so
allocator@2.0-s  1663     system  mem       REG               0,19    131072       8414 /dev/__properties__/u:object_r:public_vendor_default_prop:s0
allocator@2.0-s  1663     system  mem       REG              253,0    136312        515 /system/lib64/vndk-sp-28/android.hardware.graphics.mapper@2.0.so
allocator@2.0-s  1663     system  mem       REG              253,0    201784        529 /system/lib64/vndk-sp-28/libhwbinder.so
allocator@2.0-s  1663     system  mem       REG              253,0    136800        106 /system/lib64/liblog.so
allocator@2.0-s  1663     system  mem       REG              253,0    135896        292 /system/lib64/vndk-28/android.hardware.graphics.allocator@2.0.so
allocator@2.0-s  1663     system  mem       REG              253,0     67496        343 /system/lib64/libdl.so
allocator@2.0-s  1663     system  mem       REG              253,0   1126600         62 /system/lib64/libc.so
allocator@2.0-s  1663     system  mem       REG              253,0    134912        537 /system/lib64/vndk-sp-28/libcutils.so
allocator@2.0-s  1663     system  mem       REG              253,0    926864        375 /system/lib64/libc++.so
allocator@2.0-s  1663     system  mem       REG              253,0    926864        522 /system/lib64/vndk-sp-28/libc++.so
allocator@2.0-s  1663     system  mem       REG              253,0     68280        629 /system/lib64/libvndksupport.so
allocator@2.0-s  1663     system  mem       REG               0,19    131072       8398 /dev/__properties__/u:object_r:log_tag_prop:s0
allocator@2.0-s  1663     system  mem       REG              253,0     67992        517 /system/lib64/vndk-sp-28/android.hardware.graphics.common@1.0.so
allocator@2.0-s  1663     system  mem       REG               0,19    131072       8399 /dev/__properties__/u:object_r:logd_prop:s0
allocator@2.0-s  1663     system  mem       REG              253,0    265464        504 /system/lib64/libm.so
allocator@2.0-s  1663     system  mem       REG              253,0    134920        540 /system/lib64/vndk-sp-28/libutils.so
allocator@2.0-s  1663     system  mem       REG               0,19    131072       8397 /dev/__properties__/u:object_r:log_prop:s0
allocator@2.0-s  1663     system  mem       REG              253,0    201040        538 /system/lib64/vndk-sp-28/libhidlbase.so
allocator@2.0-s  1663     system  mem       REG              253,0    134904        531 /system/lib64/vndk-sp-28/libbase.so
allocator@2.0-s  1663     system  mem       REG               0,19    131072       8359 /dev/__properties__/u:object_r:debug_prop:s0
allocator@2.0-s  1663     system  mem       REG              253,0    468376        534 /system/lib64/vndk-sp-28/libhidltransport.so
allocator@2.0-s  1663     system  mem       REG               0,19    131072       8379 /dev/__properties__/u:object_r:exported_default_prop:s0
allocator@2.0-s  1663     system  mem       REG               0,19    131072       8395 /dev/__properties__/u:object_r:hwservicemanager_prop:s0
allocator@2.0-s  1663     system  mem       REG               0,19    131072       8368 /dev/__properties__/u:object_r:exported2_default_prop:s0
allocator@2.0-s  1663     system  mem       REG               0,19    131072       8434 /dev/__properties__/properties_serial
allocator@2.0-s  1663     system  mem       REG               0,19     34728       8335 /dev/__properties__/property_info
allocator@2.0-s  1663     system  mem       REG               0,19    131072       8379 /dev/__properties__/u:object_r:exported_default_prop:s0
allocator@2.0-s  1663     system  mem       REG               0,19    131072       8359 /dev/__properties__/u:object_r:debug_prop:s0
allocator@2.0-s  1663     system  mem       REG               0,19    131072       8434 /dev/__properties__/properties_serial
allocator@2.0-s  1663     system  mem       REG              253,0   1741568       2016 /system/bin/linker64
allocator@2.0-s  1663     system  mem       REG               0,19     34728       8335 /dev/__properties__/property_info
allocator@2.0-s  1663     system    0u      CHR                1,3       0t0        209 /dev/null
allocator@2.0-s  1663     system    1u      CHR                1,3       0t0        209 /dev/null
allocator@2.0-s  1663     system    2u      CHR                1,3       0t0        209 /dev/null
allocator@2.0-s  1663     system    3u      CHR              10,58       0t0        211 /dev/hwbinder
allocator@2.0-s  1663     system    4w      REG               0,10         0        116 /sys/kernel/debug/tracing/trace_marker
allocator@2.0-s  1663     system    5u     unix                          0t0        580 socket
allocator@2.0-s  1663     system    6r      CHR              10,62       0t0      13382 /dev/ion
allocator@2.0-s  1663     system    7u      CHR            226,128       0t0        180 /dev/dri/renderD128





