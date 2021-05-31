

//	@	frameworks/base/core/java/android/view/ThreadedRenderer.java


public final class ThreadedRenderer {

	 /**
     * Creates a threaded renderer using OpenGL.
     *
     * @param translucent True if the surface is translucent, false otherwise
     *
     * @return A threaded renderer backed by OpenGL.
     */
    public static ThreadedRenderer create(Context context, boolean translucent, String name) {
        ThreadedRenderer renderer = null;
        if (isAvailable()) {
            renderer = new ThreadedRenderer(context, translucent, name);
        }
        return renderer;
    }


    /**
     * Indicates whether threaded rendering is available under any form for
     * the view hierarchy.
     *
     * @return True if the view hierarchy can potentially be defer rendered,
     *         false otherwise
     */
    public static boolean isAvailable() {
        // when OpenGL ES 2.0 is disabled, this property should be set to false.
        // it is set to true by default.
        String supportOpenGL = SystemProperties.get("sys.viewroot.hw", "true");
        if (supportOpenGL.equals("false")) {
            Log.i(LOG_TAG, "OpenGL ES 2.0 is disabled");
            return false;
        }

        if (sSupportsOpenGL != null) {
            return sSupportsOpenGL.booleanValue();
        }
        if (SystemProperties.getInt("ro.kernel.qemu", 0) == 0) {
            // Device is not an emulator.
            sSupportsOpenGL = true;
            return true;
        }
        int qemu_gles = SystemProperties.getInt("qemu.gles", -1);
        if (qemu_gles == -1) {
            // In this case, the value of the qemu.gles property is not ready
            // because the SurfaceFlinger service may not start at this point.
            return false;
        }
        // In the emulator this property will be set > 0 when OpenGL ES 2.0 is
        // enabled, 0 otherwise. On old emulator versions it will be undefined.
        sSupportsOpenGL = qemu_gles > 0;
        return sSupportsOpenGL.booleanValue();
    }


     ThreadedRenderer(Context context, boolean translucent, String name) {
        final TypedArray a = context.obtainStyledAttributes(null, R.styleable.Lighting, 0, 0);
        mLightY = a.getDimension(R.styleable.Lighting_lightY, 0);
        mLightZ = a.getDimension(R.styleable.Lighting_lightZ, 0);
        mLightRadius = a.getDimension(R.styleable.Lighting_lightRadius, 0);
        mAmbientShadowAlpha = (int) (255 * a.getFloat(R.styleable.Lighting_ambientShadowAlpha, 0) + 0.5f);
        mSpotShadowAlpha = (int) (255 * a.getFloat(R.styleable.Lighting_spotShadowAlpha, 0) + 0.5f);
        a.recycle();

        long rootNodePtr = nCreateRootRenderNode();
        mRootNode = RenderNode.adopt(rootNodePtr);
        mRootNode.setClipToBounds(false);
        mIsOpaque = !translucent;
        mNativeProxy = nCreateProxy(translucent, rootNodePtr);
        nSetName(mNativeProxy, name);

        ProcessInitializer.sInstance.init(context, mNativeProxy);

        loadSystemProperties();
    }


}