public final class UsbAlsaManager {

    /* package */ UsbAlsaManager(Context context) {
        mContext = context;
        /***
         * public static final String FEATURE_MIDI = "android.software.midi";
         */
        mHasMidiFeature = context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_MIDI);
    }

    public void systemReady() {
        mAudioService = IAudioService.Stub.asInterface(ServiceManager.getService(Context.AUDIO_SERVICE));
    }


}