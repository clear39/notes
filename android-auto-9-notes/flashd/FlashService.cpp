#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cutils/properties.h>
#include <sysutils/NetlinkEvent.h>
#include "log.h"
#include "FlashService.h"

#include <libusb.h>

FlashService *FlashService::sInstance = NULL;

FlashService *FlashService::Instance() {
    if (!sInstance)
        sInstance = new FlashService();

    return sInstance;
}

FlashService::FlashService() {
    ALOGD("FlashService::FlashService called");
}

FlashService::~FlashService() {
    ALOGD("FlashService::~FlashService called");
}

int FlashService::start() {
    ALOGD("FlashService::start called");

    if (isScannerGun()) {
        ALOGD("FlashService::start called, isScannerGun(True)");
        launchApp();
    }

    return 0;
}

int FlashService::stop() {
    ALOGD("FlashService::stop called");

    // TODO: nothing

    return 0;
}

int FlashService::isScannerGun() {
    libusb_device *dev;
    libusb_device **devs;
    int ret;
    bool isScanner = false;

    //  @   external/libusb/libusb/core.c:2062:int API_EXPORTED libusb_init(libusb_context **context)
    ret = libusb_init(NULL);
    if (ret != 0) {
        ALOGE("Failed to initialise libusb");
        return false;
    }

    libusb_set_debug(NULL,LIBUSB_LOG_LEVEL_DEBUG);

    if (libusb_get_device_list(NULL, &devs) < 0) {
        ALOGE("Failed to get device list");
        libusb_exit(NULL);
        return false;
    }

    int i = 0;
    while ((dev = devs[i++]) != NULL) {
        // if (1 != libusb_get_bus_number(dev))
        //     continue;

        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(dev, &desc) < 0) {
            ALOGE("Failed to get device descriptor");
            break;
        }

        for (int j = 0; j < desc.bNumConfigurations; j++) {
            struct libusb_config_descriptor *config;
            ret = libusb_get_config_descriptor(dev, j, &config);
            if (LIBUSB_SUCCESS != ret) {
               ALOGE("FlashService::isScannerGun called, Couldn't retrieve descriptors");
               continue;
            }

            for (int k = 0; k < config->bNumInterfaces; k++) {
                struct libusb_interface *interface = (struct libusb_interface *)&config->interface[k];
                for (int m = 0; m < interface->num_altsetting; m++) {
                    struct libusb_interface_descriptor *interfacedesc = (struct libusb_interface_descriptor *)&interface->altsetting[m];
                    if ((interfacedesc->bInterfaceClass == LIBUSB_CLASS_HID) && interfacedesc->bInterfaceProtocol == 0x1) {
                        ALOGD("FlashService::isScannerGun called, True");
                        isScanner = true;
                    }
                }
            }

            libusb_free_config_descriptor(config);
        }
    }

    libusb_free_device_list(devs, 1);

    libusb_exit(NULL);

    return isScanner;
}

int FlashService::handleHidrawEvent(NetlinkEvent *evt) {
    ALOGD("FlashService::handleHidrawEvent called");
    if( NetlinkEvent::Action::kAdd == evt->getAction()){
        if (isScannerGun()) {
            ALOGD("FlashService::handleHidrawEvent called, isScannerGun(True)");
            launchApp();
        }
    }

    return 0;
}

void FlashService::launchApp() {
    ALOGD("FlashService::launchApp called(cmd: am start -a com.autolink.trigger.ScanningGun)");
    int ret = system("am start -a com.autolink.trigger.ScanningGun");
    ALOGD("system ret:%d,%s",ret,strerror(errno));
}
