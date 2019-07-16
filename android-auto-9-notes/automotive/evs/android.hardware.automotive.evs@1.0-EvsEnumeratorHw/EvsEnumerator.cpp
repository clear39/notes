//  @   /work/workcodes/aosp-p9.x-auto-ga/vendor/nxp-opensource/imx/evs/EvsEnumerator.cpp
EvsEnumerator::EvsEnumerator() {
    ALOGD("EvsEnumerator created");

    if (!EnumAvailableVideo()){
        mPollVideoFileThread = new PollVideoFileThread();
    }
}


bool EvsEnumerator::EnumAvailableVideo() {
    unsigned videoCount   = 0;
    unsigned captureCount = 0;
    bool videoReady = false;
    //  EvsEnumerator.cpp:43:#define EVS_FAKE_PROP   "vendor.evs.fake.enable"
    int enableFake = property_get_int32(EVS_FAKE_PROP, 0);
    if (enableFake != 0) {
        sCameraList.emplace_back(EVS_FAKE_SENSOR, EVS_FAKE_NAME);
        captureCount++;
    }

    // For every video* entry in the dev folder, see if it reports suitable capabilities
    // WARNING:  Depending on the driver implementations this could be slow, especially if
    //           there are timeouts or round trips to hardware required to collect the needed
    //           information.  Platform implementers should consider hard coding this list of
    //           known good devices to speed up the startup time of their EVS implementation.
    //           For example, this code might be replaced with nothing more than:
    //                   sCameraList.emplace_back("/dev/video0");
    //                   sCameraList.emplace_back("/dev/video1");
    ALOGI("Starting dev/video* enumeration");

    DIR* dir = opendir("/dev");
    if (!dir) {
        LOG_FATAL("Failed to open /dev folder\n");
    }
    struct dirent* entry;
    FILE *fp = NULL;
    char devPath[HWC_PATH_LENGTH];
    char value[HWC_PATH_LENGTH];
    while ((entry = readdir(dir)) != nullptr) {
        // We're only looking for entries starting with 'video'
        if (strncmp(entry->d_name, "video", 5) == 0) {
            std::string deviceName("/dev/");
            deviceName += entry->d_name;
            videoCount++;
            if (qualifyCaptureDevice(deviceName.c_str())) {
                snprintf(devPath, HWC_PATH_LENGTH,"/sys/class/video4linux/%s/name", entry->d_name);
                if ((fp = fopen(devPath, "r")) == nullptr) {
                    ALOGE("can't open %s", devPath);
                    continue;
                }
                if(fgets(value, sizeof(value), fp) == nullptr) {
                    fclose(fp);
                    ALOGE("can't read %s", devPath);
                    continue;
                }
                fclose(fp);
                ALOGI("enum name:%s path:%s", value, deviceName.c_str());
                sCameraList.emplace_back(value, deviceName.c_str());
                captureCount++;
            }
        }
    }

    if (captureCount != 0) {
        videoReady = true;
        //  EvsEnumerator.cpp:40:#define EVS_VIDEO_READY "vendor.evs.video.ready"
        if (property_set(EVS_VIDEO_READY, "1") < 0)
            ALOGE("Can not set property %s", EVS_VIDEO_READY);
    }

    closedir(dir);
    ALOGI("Found %d qualified video capture devices of %d checked\n", captureCount, videoCount);
    return videoReady;
}



Return<sp<IEvsDisplay>> EvsEnumerator::openDisplay() {
    ALOGD("openDisplay");

    // If we already have a display active, then we need to shut it down so we can
    // give exclusive access to the new caller.
    /***
     * wp<EvsDisplay>                           EvsEnumerator::sActiveDisplay;
     */
    sp<EvsDisplay> pActiveDisplay = sActiveDisplay.promote();
    if (pActiveDisplay != nullptr) {
        ALOGW("Killing previous display because of new caller");
        closeDisplay(pActiveDisplay);
    }

    // Create a new display interface and return it
    pActiveDisplay = new EvsDisplay();
    sActiveDisplay = pActiveDisplay;

    ALOGD("Returning new EvsDisplay object %p", pActiveDisplay.get());
    return pActiveDisplay;
}



