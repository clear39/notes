//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/native/services/surfaceflinger/BufferLayer.cpp
BufferLayer::BufferLayer(SurfaceFlinger* flinger, const sp<Client>& client, const String8& name,uint32_t w, uint32_t h, uint32_t flags)
      : Layer(flinger, client, name, w, h, flags),
        mConsumer(nullptr),
        mTextureName(UINT32_MAX),
        mFormat(PIXEL_FORMAT_NONE),
        mCurrentScalingMode(NATIVE_WINDOW_SCALING_MODE_FREEZE),
        mBufferLatched(false),
        mPreviousFrameNumber(0),
        mUpdateTexImageFailed(false),
        mRefreshPending(false) {
    ALOGV("Creating Layer %s", name.string());

    mTextureName = mFlinger->getNewTexture();
    mTexture.init(Texture::TEXTURE_EXTERNAL, mTextureName);

    if (flags & ISurfaceComposerClient::eNonPremultiplied) mPremultipliedAlpha = false;

    mCurrentState.requested = mCurrentState.active;

    // drawing state & current state are identical
    mDrawingState = mCurrentState;
}

void BufferLayer::onFirstRef() {
    Layer::onFirstRef();

    // Creates a custom BufferQueue for SurfaceFlingerConsumer to use
    sp<IGraphicBufferProducer> producer;
    sp<IGraphicBufferConsumer> consumer;
    BufferQueue::createBufferQueue(&producer, &consumer, true);
    mProducer = new MonitoredProducer(producer, mFlinger, this);
    {
        // Grab the SF state lock during this since it's the only safe way to access RenderEngine
        Mutex::Autolock lock(mFlinger->mStateLock);
        mConsumer = new BufferLayerConsumer(consumer, mFlinger->getRenderEngine(), mTextureName,this);
    }
    mConsumer->setConsumerUsageBits(getEffectiveUsage(0));
    mConsumer->setContentsChangedListener(this);
    mConsumer->setName(mName);

    if (mFlinger->isLayerTripleBufferingDisabled()) {
        mProducer->setMaxDequeuedBufferCount(2);
    }

    const sp<const DisplayDevice> hw(mFlinger->getDefaultDisplayDevice());
    updateTransformHint(hw);
}


status_t BufferLayer::setBuffers(uint32_t w, uint32_t h, PixelFormat format, uint32_t flags) {
    //  
    uint32_t const maxSurfaceDims = min(mFlinger->getMaxTextureSize(), mFlinger->getMaxViewportDims());

    // never allow a surface larger than what our underlying GL implementation
    // can handle.
    if ((uint32_t(w) > maxSurfaceDims) || (uint32_t(h) > maxSurfaceDims)) {
        ALOGE("dimensions too large %u x %u", uint32_t(w), uint32_t(h));
        return BAD_VALUE;
    }

    mFormat = format;

    mPotentialCursor = (flags & ISurfaceComposerClient::eCursorWindow) ? true : false;
    mProtectedByApp = (flags & ISurfaceComposerClient::eProtectedByApp) ? true : false;
    mCurrentOpacity = getOpacityForFormat(format);

    mConsumer->setDefaultBufferSize(w, h);
    mConsumer->setDefaultBufferFormat(format);
    mConsumer->setConsumerUsageBits(getEffectiveUsage(0));

    return NO_ERROR;
}