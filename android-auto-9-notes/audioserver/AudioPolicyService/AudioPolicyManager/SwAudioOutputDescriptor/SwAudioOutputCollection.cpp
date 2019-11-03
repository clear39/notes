
/**
 * @    frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioOutputDescriptor.h
 * 这里的 audio_io_handle_t 对应 AudioFlinger 中的混音线程 PlaybackThread(或者其子类)
*/
class SwAudioOutputCollection :
        public DefaultKeyedVector< audio_io_handle_t, sp<SwAudioOutputDescriptor> >{

}