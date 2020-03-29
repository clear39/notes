

class CameraService :
    public BinderService<CameraService>,
    public virtual ::android::hardware::BnCameraService,
    public virtual IBinder::DeathRecipient,
    public virtual CameraProviderManager::StatusListener
{

}



//  @   frameworks/av/services/camera/libcameraservice/CameraService.cpp

//frameworks/av/services/camera/libcameraservice/CameraService.h:86:    static const size_t DEFAULT_EVENT_LOG_LENGTH = 100;
CameraService::CameraService() :
        mEventLog(DEFAULT_EVENT_LOG_LENGTH), // 
        mNumberOfCameras(0),
        mSoundRef(0), mInitialized(false) {
    ALOGI("CameraService started (pid=%d)", getpid());
    mServiceLockWrapper = std::make_shared<WaitableMutexWrapper>(&mServiceLock);
}

/**
 * 由于 CameraService 继承 BinderService , 在CameraService::instantiate()中调用注册
 * 
*/
static char const* CameraService::getServiceName() { return "media.camera"; }

/**
 * 由于 CameraService 继承 BinderService 
 * 
*/
void CameraService::onFirstRef()
{
    ALOGI("CameraService process starting");

    BnCameraService::onFirstRef();//

    // Update battery life tracking if service is restarting
    BatteryNotifier& notifier(BatteryNotifier::getInstance());
    notifier.noteResetCamera();
    notifier.noteResetFlashlight();

    status_t res = INVALID_OPERATION;

    res = enumerateProviders();
    if (res == OK) {
        mInitialized = true;
    }

    CameraService::pingCameraServiceProxy();

/**
 * 
*/
    mUidPolicy = new UidPolicy(this);
    mUidPolicy->registerSelf();
}


status_t CameraService::enumerateProviders() {
    status_t res;

    std::vector<std::string> deviceIds;
    {
        Mutex::Autolock l(mServiceLock);

        if (nullptr == mCameraProviderManager.get()) {
            mCameraProviderManager = new CameraProviderManager();
            res = mCameraProviderManager->initialize(this);
            if (res != OK) {
                ALOGE("%s: Unable to initialize camera provider manager: %s (%d)", __FUNCTION__, strerror(-res), res);
                return res;
            }
        }


        // Setup vendor tags before we call get_camera_info the first time
        // because HAL might need to setup static vendor keys in get_camera_info
        // TODO: maybe put this into CameraProviderManager::initialize()?
        mCameraProviderManager->setUpVendorTags();

        if (nullptr == mFlashlight.get()) {
            mFlashlight = new CameraFlashlight(mCameraProviderManager, this);
        }

        res = mFlashlight->findFlashUnits();
        if (res != OK) {
            ALOGE("Failed to enumerate flash units: %s (%d)", strerror(-res), res);
        }

        deviceIds = mCameraProviderManager->getCameraDeviceIds();
    }


    for (auto& cameraId : deviceIds) {
        String8 id8 = String8(cameraId.c_str());
        onDeviceStatusChanged(id8, CameraDeviceStatus::PRESENT);
    }

    return OK;
}



void CameraService::pingCameraServiceProxy() {
    sp<ICameraServiceProxy> proxyBinder = getCameraServiceProxy();
    if (proxyBinder == nullptr) return;
    proxyBinder->pingForUserUpdate();
}




