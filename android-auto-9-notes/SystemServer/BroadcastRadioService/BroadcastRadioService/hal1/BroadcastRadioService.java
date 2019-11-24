
//  @   frameworks/base/services/core/java/com/android/server/broadcastradio/hal1/BroadcastRadioService.java

public class BroadcastRadioService {
    /**
     * This field is used by native code, do not access or modify.
     */
    private final long mNativeContext = nativeInit();

    private final Object mLock = new Object();

    @Override
    protected void finalize() throws Throwable {
        nativeFinalize(mNativeContext);
        super.finalize();
    }
    /**
     * jni实现 @    frameworks/base/services/core/jni/BroadcastRadio/BroadcastRadioService.cpp
     */
    private native long nativeInit();
    private native void nativeFinalize(long nativeContext);
    private native List<RadioManager.ModuleProperties> nativeLoadModules(long nativeContext);
    private native Tuner nativeOpenTuner(long nativeContext, int moduleId,
            RadioManager.BandConfig config, boolean withAudio, ITunerCallback callback);

    public @NonNull List<RadioManager.ModuleProperties> loadModules() {
        synchronized (mLock) {
            return Objects.requireNonNull(nativeLoadModules(mNativeContext));
        }
    }

    public ITuner openTuner(int moduleId, RadioManager.BandConfig bandConfig,
            boolean withAudio, @NonNull ITunerCallback callback) {
        synchronized (mLock) {
            return nativeOpenTuner(mNativeContext, moduleId, bandConfig, withAudio, callback);
        }
    }
}