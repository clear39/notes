


//  @   /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/packages/services/Car/service/src/com/android/car/CarVolumeGroup.java

/* package */ final class CarVolumeGroup {

    CarVolumeGroup(Context context, int id, @NonNull int[] contexts) {
        mContentResolver = context.getContentResolver();
        mId = id;
        mContexts = contexts;
        
        /***
         * android.car.VOLUME_GROUP/0=32
         */
        mStoredGainIndex = Settings.Global.getInt(mContentResolver, CarAudioManager.getVolumeSettingsKeyForGroup(mId), -1);;
    }


    void bind(int contextNumber, int busNumber, CarAudioDeviceInfo info) {
        if (mBusToCarAudioDeviceInfos.size() == 0) {
            mStepSize = info.getAudioGain().stepValue();
        } else {
            Preconditions.checkArgument(
                    info.getAudioGain().stepValue() == mStepSize,
                    "Gain controls within one group must have same step value");
        }

        mContextToBus.put(contextNumber, busNumber);
        mBusToCarAudioDeviceInfos.put(busNumber, info);

        if (info.getDefaultGain() > mDefaultGain) {
            // We're arbitrarily selecting the highest bus default gain as the group's default.
            mDefaultGain = info.getDefaultGain();
        }
        if (info.getMaxGain() > mMaxGain) {
            mMaxGain = info.getMaxGain();
        }
        if (info.getMinGain() < mMinGain) {
            mMinGain = info.getMinGain();
        }
        if (mStoredGainIndex < getMinGainIndex() || mStoredGainIndex > getMaxGainIndex()) {
            // We expected to load a value from last boot, but if we didn't (perhaps this is the
            // first boot ever?), then use the highest "default" we've seen to initialize
            // ourselves.
            mCurrentGainIndex = getIndexForGain(mDefaultGain);
        } else {
            // Just use the gain index we stored last time the gain was set (presumably during our
            // last boot cycle).
            mCurrentGainIndex = mStoredGainIndex;
        }
    }

    int getId() {
        return mId;
    }

    int[] getContexts() {
        return mContexts;
    }

    int[] getBusNumbers() {
        final int[] busNumbers = new int[mBusToCarAudioDeviceInfos.size()];
        for (int i = 0; i < busNumbers.length; i++) {
            busNumbers[i] = mBusToCarAudioDeviceInfos.keyAt(i);
        }
        return busNumbers;
    }

}