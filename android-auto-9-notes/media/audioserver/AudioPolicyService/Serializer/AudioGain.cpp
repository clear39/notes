class AudioGain: public RefBase{

}


/**
 * */

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioGain.cpp
/***
 * index 由循环累加
 * */
AudioGain::AudioGain(int index, bool useInChannelMask)
{
    mIndex = index;
    mUseInChannelMask = useInChannelMask;
    /*
        @   system/media/audio/include/system/audio.h:342:struct audio_gain  {
    */
    memset(&mGain, 0, sizeof(struct audio_gain));
}


/*
    system/media/audio/include/system/audio.h:337:typedef uint32_t audio_gain_mode_t;

    对应 gain 标签的 mode 属性值

*/
void setMode(audio_gain_mode_t mode) { 
    mGain.mode = mode; 
}


/*
    system/media/audio/include/system/audio.h:137:typedef uint32_t audio_channel_mask_t;

    对应 gain 标签的 channel_mask 属性值

*/

void setChannelMask(audio_channel_mask_t mask) { 
    mGain.channel_mask = mask; 
}

