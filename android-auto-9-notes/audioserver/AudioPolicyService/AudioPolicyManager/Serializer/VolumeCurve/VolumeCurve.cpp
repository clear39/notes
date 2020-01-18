
struct CurvePoint
{
    CurvePoint() {}
    CurvePoint(int index, int attenuationInMb) :
        mIndex(index), mAttenuationInMb(attenuationInMb) {}
    uint32_t mIndex;
    int mAttenuationInMb;
};

/**
 * 
 * @    frameworks/av/services/audiopolicy/common/include/Volume.h
 * device categories used for volume curve management.
    enum device_category {
        DEVICE_CATEGORY_HEADSET,
        DEVICE_CATEGORY_SPEAKER,
        DEVICE_CATEGORY_EARPIECE,
        DEVICE_CATEGORY_EXT_MEDIA,
        DEVICE_CATEGORY_HEARING_AID,
        DEVICE_CATEGORY_CNT
    };
*
*
*/

//  @   frameworks/av/services/audiopolicy/common/managerdefinitions/include/VolumeCurve.h
VolumeCurve(device_category device, audio_stream_type_t stream) :mDeviceCategory(device), mStreamType(stream) {

}

device_category VolumeCurve::getDeviceCategory() const { return mDeviceCategory; }

audio_stream_type_t VolumeCurve::getStreamType() const { return mStreamType; }

void VolumeCurve::add(const CurvePoint &point) { 
        mCurvePoints.add(point);
 }


/*
<volume stream="AUDIO_STREAM_VOICE_CALL"  deviceCategory="DEVICE_CATEGORY_HEADSET">
    <!--
    
        格式：    mIndex , mAttenuationInMb

        point 对应封装类为 CurvePoint
    
        <point> CurvePoint::mIndex, CurvePoint::mAttenuationInMb</point>
        通过  VolumeCurve::add 将每一个 CurvePoint 添加到 VolumeCurve::mCurvePoints 中
    -->
    <point>0,-4200</point>
    <point>33,-2800</point>
    <point>66,-1400</point>
    <point>100,0</point>
</volume>
*/
float VolumeCurve::volIndexToDb(int indexInUi, int volIndexMin, int volIndexMax) const
{
    ALOG_ASSERT(!mCurvePoints.isEmpty(), "Invalid volume curve");

    size_t nbCurvePoints = mCurvePoints.size();
    // the volume index in the UI is relative to the min and max volume indices for this stream
    int nbSteps = 1 + mCurvePoints[nbCurvePoints - 1].mIndex - mCurvePoints[0].mIndex;
    if (indexInUi < volIndexMin) {
        ALOGV("VOLUME remapping index from %d to min index %d", indexInUi, volIndexMin);
        indexInUi = volIndexMin;
    } else if (indexInUi > volIndexMax) {
        ALOGV("VOLUME remapping index from %d to max index %d", indexInUi, volIndexMax);
        indexInUi = volIndexMax;
    }
    int volIdx = (nbSteps * (indexInUi - volIndexMin)) / (volIndexMax - volIndexMin);

    // Where would this volume index been inserted in the curve point
    size_t indexInUiPosition = mCurvePoints.orderOf(CurvePoint(volIdx, 0));
    if (indexInUiPosition >= nbCurvePoints) {
        //use last point of table
        return mCurvePoints[nbCurvePoints - 1].mAttenuationInMb / 100.0f;
    }
    if (indexInUiPosition == 0) {
        if (indexInUiPosition != mCurvePoints[0].mIndex) {
            return VOLUME_MIN_DB; // out of bounds
        }
        return mCurvePoints[0].mAttenuationInMb / 100.0f;
    }
    // linear interpolation in the attenuation table in dB
    float decibels = (mCurvePoints[indexInUiPosition - 1].mAttenuationInMb / 100.0f) +
            ((float)(volIdx - mCurvePoints[indexInUiPosition - 1].mIndex)) *
                ( ((mCurvePoints[indexInUiPosition].mAttenuationInMb / 100.0f) -
                        (mCurvePoints[indexInUiPosition - 1].mAttenuationInMb / 100.0f)) /
                    ((float)(mCurvePoints[indexInUiPosition].mIndex - mCurvePoints[indexInUiPosition - 1].mIndex)) );

    ALOGV("VOLUME mDeviceCategory %d, mStreamType %d vol index=[%d %d %d], dB=[%.1f %.1f %.1f]",
            mDeviceCategory, mStreamType,
            mCurvePoints[indexInUiPosition - 1].mIndex, volIdx,
            mCurvePoints[indexInUiPosition].mIndex,
            ((float)mCurvePoints[indexInUiPosition - 1].mAttenuationInMb / 100.0f), decibels,
            ((float)mCurvePoints[indexInUiPosition].mAttenuationInMb / 100.0f));

    return decibels;
}



