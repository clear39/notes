

//	@	frameworks/av/services/audiopolicy/engine/common/src/VolumeCurve.cpp

class VolumeCurve : public RefBase{

private:
    const device_category mDeviceCategory;
    SortedVector<CurvePoint> mCurvePoints;
}

class VolumeCurves : public KeyedVector<device_category, sp<VolumeCurve> >,
                     public IVolumeCurves
{

	VolumeCurves(int indexMin = 0, int indexMax = 100) :
        mIndexMin(indexMin), mIndexMax(indexMax)
    {
    	//系统默认添加
        addCurrentVolumeIndex(AUDIO_DEVICE_OUT_DEFAULT_FOR_VOLUME, 0);
    }

    void addCurrentVolumeIndex(audio_devices_t device, int index) override
    {
    	//std::map<audio_devices_t, int> mIndexCur; /**< current volume index per device. */
        mIndexCur[device] = index;
    }


    void addStreamType(audio_stream_type_t stream)
    {
        mStreams.push_back(stream);
    }


    AttributesVector mAttributes;
    StreamTypeVector mStreams; /**< Keep it for legacy. */

}