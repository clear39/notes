
/*
<volumeGroups>
 	<volumeGroup>
        <name>voice_call</name>
        <indexMin>1</indexMin>
        <indexMax>7</indexMax>
        <volume deviceCategory="DEVICE_CATEGORY_HEADSET">
            <point>0,-4200</point>
            <point>33,-2800</point>
            <point>66,-1400</point>
            <point>100,0</point>
        </volume>
        。。。。。。
    </volumeGroup>

    <volumeGroup>
        <name>music</name>
        <indexMin>0</indexMin>
        <indexMax>25</indexMax>
        <volume deviceCategory="DEVICE_CATEGORY_HEADSET" ref="DEFAULT_MEDIA_VOLUME_CURVE"/>
        <volume deviceCategory="DEVICE_CATEGORY_SPEAKER" ref="DEFAULT_DEVICE_CATEGORY_SPEAKER_VOLUME_CURVE"/>
        <volume deviceCategory="DEVICE_CATEGORY_EARPIECE" ref="DEFAULT_MEDIA_VOLUME_CURVE"/>
        <volume deviceCategory="DEVICE_CATEGORY_EXT_MEDIA" ref="DEFAULT_MEDIA_VOLUME_CURVE"/>
        <volume deviceCategory="DEVICE_CATEGORY_HEARING_AID"  ref="DEFAULT_HEARING_AID_VOLUME_CURVE"/>
    </volumeGroup>

    ......

</volumeGroups>



    
<volumes>
    ......
    <reference name="DEFAULT_MEDIA_VOLUME_CURVE">
    <!-- Default Media reference Volume Curve -->
        <point>1,-5800</point>
        <point>20,-4000</point>
        <point>60,-1700</point>
        <point>100,0</point>
    </reference>
</volumes>
*/


//	@	frameworks/av/services/audiopolicy/engine/common/src/VolumeGroup.cpp
class VolumeGroupMap : public std::map<volume_group_t, sp<VolumeGroup> >
{

	void VolumeGroupMap::dump(String8 *dst, int spaces) const
	{
	    dst->appendFormat("\n%*sVolume Groups dump:", spaces, "");
	    for (const auto &iter : *this) {
	        iter.second->dump(dst, spaces + 2);
	    }
	}

}

// @ frameworks/av/media/libaudioclient/include/media/AudioCommonTypes.h
enum volume_group_t : uint32_t;

// name 为 voice_call 或者 music
// VolumeCurves mGroupVolumeCurves;
VolumeGroup::VolumeGroup(const std::string &name, int indexMin, int indexMax) :
    mName(name), mId(static_cast<volume_group_t>(HandleGenerator<uint32_t>::getNextHandle())),
    mGroupVolumeCurves(VolumeCurves(indexMin, indexMax))
{
}




void VolumeGroup::addSupportedStream(audio_stream_type_t stream)
{
    mGroupVolumeCurves.addStreamType(stream);
}