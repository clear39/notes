

/* package */ class CarAudioDeviceInfo {


    CarAudioDeviceInfo(AudioDeviceInfo audioDeviceInfo) {
        mAudioDeviceInfo = audioDeviceInfo;
        mBusNumber = parseDeviceAddress(audioDeviceInfo.getAddress());
        /***
         * 以下三个参数如何到来
         */
        mSampleRate = getMaxSampleRate(audioDeviceInfo);
        mEncodingFormat = getEncodingFormat(audioDeviceInfo);
        mChannelCount = getMaxChannels(audioDeviceInfo);

        /***
         * 
         */
        final AudioGain audioGain = Preconditions.checkNotNull(getAudioGain(), "No audio gain on device port " + audioDeviceInfo);
        mDefaultGain = audioGain.defaultValue();
        mMaxGain = audioGain.maxValue();
        mMinGain = audioGain.minValue();

        mCurrentGain = -1; // Not initialized till explicitly set
    }

    /**
     * Parse device address. Expected format is BUS%d_%s, address, usage hint
     * @return valid address (from 0 to positive) or -1 for invalid address.
     * 
     * 这里以 bus0_media_out 为例子 
     */
    private int parseDeviceAddress(String address) {
        String[] words = address.split("_");  // bus0 media out
        int addressParsed = -1;
        if (words[0].toLowerCase().startsWith("bus")) {// bus0
            try {
                addressParsed = Integer.parseInt(words[0].substring(3)); // 0
            } catch (NumberFormatException e) {
                //ignore
            }
        }
        if (addressParsed < 0) {
            return -1;
        }
        return addressParsed;
    }



    int getBusNumber() {
        return mBusNumber;
    }
}