

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/native/services/surfaceflinger/RenderEngine/Surface.cpp
Surface::Surface(const RenderEngine& engine)
      : mEGLDisplay(engine.getEGLDisplay()), mEGLConfig(engine.getEGLConfig()) {
    // RE does not assume any config when EGL_KHR_no_config_context is supported
    if (mEGLConfig == EGL_NO_CONFIG_KHR) {
        mEGLConfig = RenderEngine::chooseEglConfig(mEGLDisplay, PIXEL_FORMAT_RGBA_8888, false);
    }
}