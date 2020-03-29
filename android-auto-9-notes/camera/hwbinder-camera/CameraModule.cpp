

//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/hardware/interfaces/camera/common/1.0/default/CameraModule.cpp

CameraModule::CameraModule(camera_module_t *module) {
    if (module == NULL) {
        ALOGE("%s: camera hardware module must not be null", __FUNCTION__);
        assert(0);
    }
    mModule = module;
}



int CameraModule::init() {
    ATRACE_CALL();
    int res = OK;
    if (getModuleApiVersion() >= CAMERA_MODULE_API_VERSION_2_4 && mModule->init != NULL) {
        ATRACE_BEGIN("camera_module->init");
        res = mModule->init();
        ATRACE_END();
    }
    mCameraInfoMap.setCapacity(getNumberOfCameras());
    return res;
}


int CameraModule::getNumberOfCameras() {
    int numCameras;
    ATRACE_BEGIN("camera_module->get_number_of_cameras");
    numCameras = mModule->get_number_of_cameras();
    ATRACE_END();
    return numCameras;
}




bool CameraModule::isSetTorchModeSupported() const {
    if (getModuleApiVersion() >= CAMERA_MODULE_API_VERSION_2_4) {
        if (mModule->set_torch_mode == NULL) {
            ALOGE("%s: Module 2.4 device must support set torch API!",  __FUNCTION__);
            return false;
        }
        return true;
    }
    return false;
}