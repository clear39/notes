//  @   frameworks/base/core/java/android/hardware/radio/RadioManager.java


/*****************************************************************************
 * Lists properties, options and radio bands supported by a given broadcast radio module.
 * Each module has a unique ID used to address it when calling RadioManager APIs.
 * Module properties are returned by {@link #listModules(List <ModuleProperties>)} method.
 ****************************************************************************/
public static class ModuleProperties implements Parcelable {

    public ModuleProperties(int id, String serviceName, int classId, String implementor,
                String product, String version, String serial, int numTuners, int numAudioSources,
                boolean isInitializationRequired, boolean isCaptureSupported,
                BandDescriptor[] bands, boolean isBgScanSupported,
                @ProgramSelector.ProgramType int[] supportedProgramTypes,
                @ProgramSelector.IdentifierType int[] supportedIdentifierTypes,
                @Nullable Map<String, Integer> dabFrequencyTable,
                Map<String, String> vendorInfo) {
        mId = id;
        mServiceName = TextUtils.isEmpty(serviceName) ? "default" : serviceName;
        mClassId = classId;
        mImplementor = implementor;
        mProduct = product;
        mVersion = version;
        mSerial = serial;
        mNumTuners = numTuners;
        mNumAudioSources = numAudioSources;
        mIsInitializationRequired = isInitializationRequired;
        mIsCaptureSupported = isCaptureSupported;
        mBands = bands;
        mIsBgScanSupported = isBgScanSupported;
        mSupportedProgramTypes = arrayToSet(supportedProgramTypes);
        mSupportedIdentifierTypes = arrayToSet(supportedIdentifierTypes);
        if (dabFrequencyTable != null) {
            for (Map.Entry<String, Integer> entry : dabFrequencyTable.entrySet()) {
                Objects.requireNonNull(entry.getKey());
                Objects.requireNonNull(entry.getValue());
            }
        }
        mDabFrequencyTable = dabFrequencyTable;
        mVendorInfo = (vendorInfo == null) ? new HashMap<>() : vendorInfo;
    }
    
}