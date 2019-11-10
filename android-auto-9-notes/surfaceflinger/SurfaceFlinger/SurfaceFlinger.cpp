
//  @   frameworks/native/services/surfaceflinger/SurfaceFlinger.h
class SurfaceFlinger : public BnSurfaceComposer,
                       public PriorityDumper,
                       private IBinder::DeathRecipient,
                       private HWC2::ComposerCallback
{

}

/**
 * 
*/
SurfaceFlinger::SurfaceFlinger() : SurfaceFlinger(SkipInitialization) {
    ALOGI("SurfaceFlinger is starting");

    vsyncPhaseOffsetNs = getInt64< ISurfaceFlingerConfigs,&ISurfaceFlingerConfigs::vsyncEventPhaseOffsetNs>(1000000);

    sfVsyncPhaseOffsetNs = getInt64< ISurfaceFlingerConfigs,&ISurfaceFlingerConfigs::vsyncSfEventPhaseOffsetNs>(1000000);

    hasSyncFramework = getBool< ISurfaceFlingerConfigs,&ISurfaceFlingerConfigs::hasSyncFramework>(true);

    dispSyncPresentTimeOffset = getInt64< ISurfaceFlingerConfigs,&ISurfaceFlingerConfigs::presentTimeOffsetFromVSyncNs>(0);

    useHwcForRgbToYuv = getBool< ISurfaceFlingerConfigs,&ISurfaceFlingerConfigs::useHwcForRGBtoYUV>(false);

    maxVirtualDisplaySize = getUInt64<ISurfaceFlingerConfigs, &ISurfaceFlingerConfigs::maxVirtualDisplaySize>(0);
    /**
     * 
     * 
    */
    // Vr flinger is only enabled on Daydream ready devices.
    useVrFlinger = getBool< ISurfaceFlingerConfigs,&ISurfaceFlingerConfigs::useVrFlinger>(false);

    maxFrameBufferAcquiredBuffers = getInt64< ISurfaceFlingerConfigs,
            &ISurfaceFlingerConfigs::maxFrameBufferAcquiredBuffers>(2);

    hasWideColorDisplay = getBool<ISurfaceFlingerConfigs, &ISurfaceFlingerConfigs::hasWideColorDisplay>(false);

    V1_1::DisplayOrientation primaryDisplayOrientation =
        getDisplayOrientation< V1_1::ISurfaceFlingerConfigs, &V1_1::ISurfaceFlingerConfigs::primaryDisplayOrientation>(
            V1_1::DisplayOrientation::ORIENTATION_0);

    switch (primaryDisplayOrientation) {
        case V1_1::DisplayOrientation::ORIENTATION_90:
            mPrimaryDisplayOrientation = DisplayState::eOrientation90;
            break;
        case V1_1::DisplayOrientation::ORIENTATION_180:
            mPrimaryDisplayOrientation = DisplayState::eOrientation180;
            break;
        case V1_1::DisplayOrientation::ORIENTATION_270:
            mPrimaryDisplayOrientation = DisplayState::eOrientation270;
            break;
        default:
            mPrimaryDisplayOrientation = DisplayState::eOrientationDefault;
            break;
    }
    ALOGV("Primary Display Orientation is set to %2d.", mPrimaryDisplayOrientation);

    mPrimaryDispSync.init(SurfaceFlinger::hasSyncFramework, SurfaceFlinger::dispSyncPresentTimeOffset);

    // debugging stuff...
    char value[PROPERTY_VALUE_MAX];

    property_get("ro.bq.gpu_to_cpu_unsupported", value, "0");
    mGpuToCpuSupported = !atoi(value);

    property_get("debug.sf.showupdates", value, "0");
    mDebugRegion = atoi(value);

    property_get("debug.sf.ddms", value, "0");
    mDebugDDMS = atoi(value);
    if (mDebugDDMS) {
        if (!startDdmConnection()) {
            // start failed, and DDMS debugging not enabled
            mDebugDDMS = 0;
        }
    }
    ALOGI_IF(mDebugRegion, "showupdates enabled");
    ALOGI_IF(mDebugDDMS, "DDMS debugging enabled");

    property_get("debug.sf.disable_backpressure", value, "0");
    mPropagateBackpressure = !atoi(value);
    ALOGI_IF(!mPropagateBackpressure, "Disabling backpressure propagation");

    property_get("debug.sf.enable_hwc_vds", value, "0");
    mUseHwcVirtualDisplays = atoi(value);
    ALOGI_IF(!mUseHwcVirtualDisplays, "Enabling HWC virtual displays");

    property_get("ro.sf.disable_triple_buffer", value, "1");
    mLayerTripleBufferingDisabled = atoi(value);
    ALOGI_IF(mLayerTripleBufferingDisabled, "Disabling Triple Buffering");

    const size_t defaultListSize = MAX_LAYERS;
    auto listSize = property_get_int32("debug.sf.max_igbp_list_size", int32_t(defaultListSize));
    mMaxGraphicBufferProducerListSize = (listSize > 0) ? size_t(listSize) : defaultListSize;

    property_get("debug.sf.early_phase_offset_ns", value, "-1");
    const int earlySfOffsetNs = atoi(value);

    property_get("debug.sf.early_gl_phase_offset_ns", value, "-1");
    const int earlyGlSfOffsetNs = atoi(value);

    property_get("debug.sf.early_app_phase_offset_ns", value, "-1");
    const int earlyAppOffsetNs = atoi(value);

    property_get("debug.sf.early_gl_app_phase_offset_ns", value, "-1");
    const int earlyGlAppOffsetNs = atoi(value);

    const VSyncModulator::Offsets earlyOffsets =
            {earlySfOffsetNs != -1 ? earlySfOffsetNs : sfVsyncPhaseOffsetNs,
            earlyAppOffsetNs != -1 ? earlyAppOffsetNs : vsyncPhaseOffsetNs};
    const VSyncModulator::Offsets earlyGlOffsets =
            {earlyGlSfOffsetNs != -1 ? earlyGlSfOffsetNs : sfVsyncPhaseOffsetNs,
            earlyGlAppOffsetNs != -1 ? earlyGlAppOffsetNs : vsyncPhaseOffsetNs};
    mVsyncModulator.setPhaseOffsets(earlyOffsets, earlyGlOffsets,
            {sfVsyncPhaseOffsetNs, vsyncPhaseOffsetNs});

    // We should be reading 'persist.sys.sf.color_saturation' here
    // but since /data may be encrypted, we need to wait until after vold
    // comes online to attempt to read the property. The property is
    // instead read after the boot animation

    if (useTrebleTestingOverride()) {
        // Without the override SurfaceFlinger cannot connect to HIDL
        // services that are not listed in the manifests.  Considered
        // deriving the setting from the set service name, but it
        // would be brittle if the name that's not 'default' is used
        // for production purposes later on.
        setenv("TREBLE_TESTING_OVERRIDE", "true", true);
    }
}

/**
 * struct SurfaceFlinger::SkipInitializationTag {};
*/
SurfaceFlinger::SurfaceFlinger(SurfaceFlinger::SkipInitializationTag)
      : BnSurfaceComposer(),
        mTransactionFlags(0),
        mTransactionPending(false),
        mAnimTransactionPending(false),
        mLayersRemoved(false),
        mLayersAdded(false),
        mRepaintEverything(0),
        /***/
        mBootTime(systemTime()),
        mBuiltinDisplays(),
        mVisibleRegionsDirty(false),
        mGeometryInvalid(false),
        mAnimCompositionPending(false),
        mBootStage(BootStage::BOOTLOADER),
        mDebugRegion(0),
        mDebugDDMS(0),
        mDebugDisableHWC(0),
        mDebugDisableTransformHint(0),
        mDebugInSwapBuffers(0),
        mLastSwapBufferTime(0),
        mDebugInTransaction(0),
        mLastTransactionTime(0),
        mForceFullDamage(false),
        mPrimaryDispSync("PrimaryDispSync"),  // DispSync mPrimaryDispSync;
        mPrimaryHWVsyncEnabled(false),
        mHWVsyncAvailable(false),
        mHasPoweredOff(false),
        mNumLayers(0),
        mVrFlingerRequestsDisplay(false),
        mMainThreadId(std::this_thread::get_id()),
        /***/
        mCreateBufferQueue(&BufferQueue::createBufferQueue),
        /***/
        mCreateNativeWindowSurface(&impl::NativeWindowSurface::create) {}

void SurfaceFlinger::onFirstRef()
{
    /**
     * 
    */
    mEventQueue->init(this);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * int main(int, char**)
 * ---> flinger->init();
*/
// Do not call property_set on main thread which will be blocked by init
// Use StartPropertySetThread instead.
void SurfaceFlinger::init() {
    ALOGI(  "SurfaceFlinger's main thread ready to run. " "Initializing graphics H/W...");

    ALOGI("Phase offest NS: %" PRId64 "", vsyncPhaseOffsetNs);

    Mutex::Autolock _l(mStateLock);

    // start the EventThread
    mEventThreadSource = std::make_unique<DispSyncSource>(&mPrimaryDispSync, SurfaceFlinger::vsyncPhaseOffsetNs, true, "app");
    mEventThread = std::make_unique<impl::EventThread>(mEventThreadSource.get(),
                                                       [this]() { resyncWithRateLimit(); },
                                                       impl::EventThread::InterceptVSyncsCallback(),
                                                       "appEventThread");
    mSfEventThreadSource =
            std::make_unique<DispSyncSource>(&mPrimaryDispSync,
                                             SurfaceFlinger::sfVsyncPhaseOffsetNs, true, "sf");

    mSFEventThread =
            std::make_unique<impl::EventThread>(mSfEventThreadSource.get(),
                                                [this]() { resyncWithRateLimit(); },
                                                [this](nsecs_t timestamp) {
                                                    mInterceptor->saveVSyncEvent(timestamp);
                                                },
                                                "sfEventThread");
    mEventQueue->setEventThread(mSFEventThread.get());
    mVsyncModulator.setEventThreads(mSFEventThread.get(), mEventThread.get());

    // Get a RenderEngine for the given display / config (can't fail)
    getBE().mRenderEngine = RE::impl::RenderEngine::create(HAL_PIXEL_FORMAT_RGBA_8888,
                                           hasWideColorDisplay
                                                   ? RE::RenderEngine::WIDE_COLOR_SUPPORT
                                                   : 0);
    LOG_ALWAYS_FATAL_IF(getBE().mRenderEngine == nullptr, "couldn't create RenderEngine");

    LOG_ALWAYS_FATAL_IF(mVrFlingerRequestsDisplay,
            "Starting with vr flinger active is not currently supported.");
    getBE().mHwc.reset(new HWComposer(std::make_unique<Hwc2::impl::Composer>(getBE().mHwcServiceName)));
    getBE().mHwc->registerCallback(this, getBE().mComposerSequenceId);
    // Process any initial hotplug and resulting display changes.
    processDisplayHotplugEventsLocked();
    LOG_ALWAYS_FATAL_IF(!getBE().mHwc->isConnected(HWC_DISPLAY_PRIMARY),"Registered composer callback but didn't create the default primary display");

    // make the default display GLContext current so that we can create textures
    // when creating Layers (which may happens before we render something)
    getDefaultDisplayDeviceLocked()->makeCurrent();

    if (useVrFlinger) {
        auto vrFlingerRequestDisplayCallback = [this] (bool requestDisplay) {
            // This callback is called from the vr flinger dispatch thread. We
            // need to call signalTransaction(), which requires holding
            // mStateLock when we're not on the main thread. Acquiring
            // mStateLock from the vr flinger dispatch thread might trigger a
            // deadlock in surface flinger (see b/66916578), so post a message
            // to be handled on the main thread instead.
            sp<LambdaMessage> message = new LambdaMessage([=]() {
                ALOGI("VR request display mode: requestDisplay=%d", requestDisplay);
                mVrFlingerRequestsDisplay = requestDisplay;
                signalTransaction();
            });
            postMessageAsync(message);
        };
        mVrFlinger = dvr::VrFlinger::Create(getBE().mHwc->getComposer(),
                getBE().mHwc->getHwcDisplayId(HWC_DISPLAY_PRIMARY).value_or(0),
                vrFlingerRequestDisplayCallback);
        if (!mVrFlinger) {
            ALOGE("Failed to start vrflinger");
        }
    }

    mEventControlThread = std::make_unique<impl::EventControlThread>([this](bool enabled) { setVsyncEnabled(HWC_DISPLAY_PRIMARY, enabled); });

    // initialize our drawing state
    mDrawingState = mCurrentState;

    // set initial conditions (e.g. unblank default device)
    initializeDisplays();

    getBE().mRenderEngine->primeCache();

    // Inform native graphics APIs whether the present timestamp is supported:
    if (getHwComposer().hasCapability(HWC2::Capability::PresentFenceIsNotReliable)) {
        mStartPropertySetThread = new StartPropertySetThread(false);
    } else {
        mStartPropertySetThread = new StartPropertySetThread(true);
    }

    if (mStartPropertySetThread->Start() != NO_ERROR) {
        ALOGE("Run StartPropertySetThread failed!");
    }

    // This is a hack. Per definition of getDataspaceSaturationMatrix, the returned matrix
    // is used to saturate legacy sRGB content. However, to make sure the same color under
    // Display P3 will be saturated to the same color, we intentionally break the API spec
    // and apply this saturation matrix on Display P3 content. Unless the risk of applying
    // such saturation matrix on Display P3 is understood fully, the API should always return
    // identify matrix.
    mEnhancedSaturationMatrix = getBE().mHwc->getDataspaceSaturationMatrix(HWC_DISPLAY_PRIMARY,Dataspace::SRGB_LINEAR);

    // we will apply this on Display P3.
    if (mEnhancedSaturationMatrix != mat4()) {
        ColorSpace srgb(ColorSpace::sRGB());
        ColorSpace displayP3(ColorSpace::DisplayP3());
        mat4 srgbToP3 = mat4(ColorSpaceConnector(srgb, displayP3).getTransform());
        mat4 p3ToSrgb = mat4(ColorSpaceConnector(displayP3, srgb).getTransform());
        mEnhancedSaturationMatrix = srgbToP3 * mEnhancedSaturationMatrix * p3ToSrgb;
    }

    ALOGV("Done initializing");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * int main(int, char**)
 * ---> flinger->run();
*/
void SurfaceFlinger::run() {
    do {
        waitForEvent();
    } while (true);
}

void SurfaceFlinger::waitForEvent() {
    mEventQueue->waitMessage();
}