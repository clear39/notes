
//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/keymaster/3.0/default/android.hardware.keymaster@3.0-service.rc
service vendor.keymaster-3-0 /vendor/bin/hw/android.hardware.keymaster@3.0-service
    class early_hal
    user system
    group system drmrpc



//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/keymaster/3.0/default/service.cpp
int main() {
    return defaultPassthroughServiceImplementation<IKeymasterDevice>();
}



//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/keymaster/3.0/default/KeymasterDevice.cpp
IKeymasterDevice* HIDL_FETCH_IKeymasterDevice(const char* name) {
    ALOGI("Fetching keymaster device name %s", name);

    if (name && strcmp(name, "softwareonly") == 0) {
        return ::keymaster::ng::CreateKeymasterDevice();
    } else if (name && strcmp(name, "default") == 0) {//默认 name = "default"
        return createKeymaster3Device();
    }
    return nullptr;
}




static IKeymasterDevice* createKeymaster3Device() {
    const hw_module_t* mod = nullptr;

    int rc = hw_get_module_by_class(KEYSTORE_HARDWARE_MODULE_ID, NULL, &mod);
    if (rc) {
        ALOGI("Could not find any keystore module, using software-only implementation.");
        // SoftKeymasterDevice will be deleted by keymaster_device_release()
        return ::keymaster::ng::CreateKeymasterDevice();
    }


//  @hardware/libhardware/include/hardware/keymaster_common.h:54:#define KEYMASTER_MODULE_API_VERSION_1_0 HARDWARE_MODULE_API_VERSION(1, 0)
    if (mod->module_api_version < KEYMASTER_MODULE_API_VERSION_1_0) {
        keymaster0_device_t* dev = nullptr;
        if (get_keymaster0_dev(&dev, mod)) {
            return nullptr;
        }
        return ::keymaster::ng::CreateKeymasterDevice(dev);
    } else if (mod->module_api_version == KEYMASTER_MODULE_API_VERSION_1_0) { //执行这里
        keymaster1_device_t* dev = nullptr;
        if (get_keymaster1_dev(&dev, mod)) {
            return nullptr;
        }
        return ::keymaster::ng::CreateKeymasterDevice(dev);
    } else {
        keymaster2_device_t* dev = nullptr;
        if (get_keymaster2_dev(&dev, mod)) {
            return nullptr;
        }
        return ::keymaster::ng::CreateKeymasterDevice(dev);
    }
}


static int get_keymaster1_dev(keymaster1_device_t** dev, const hw_module_t* mod) {
    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/libhardware/include/hardware/keymaster1.h
    int rc = keymaster1_open(mod, dev);
    if (rc) {
        ALOGE("Error %d opening keystore keymaster1 device", rc);
        if (*dev) {
            (*dev)->common.close(&(*dev)->common);
            *dev = nullptr;
        }
    }
    return rc;
}

//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/libhardware/include/hardware/keymaster1.h
static inline int keymaster1_open(const struct hw_module_t* module, keymaster1_device_t** device) {                                                                                                       
    return module->methods->open(module, KEYSTORE_KEYMASTER, TO_HW_DEVICE_T_OPEN(device));
}



