
//    @   /work/workcodes/aosp-p9.x-auto-ga/hardware/interfaces/audio/4.0/IStreamIn.hal


/**
 * 
 *   @    hardware/interfaces/audio/core/all-versions/default/include/core/all-versions/default/StreamIn.impl.h
*/
StreamIn::StreamIn(const sp<Device>& device, audio_stream_in_t* stream)
    : mIsClosed(false),
      mDevice(device),
      mStream(stream),
      mStreamCommon(new Stream(&stream->common)),
      mStreamMmap(new StreamMmap<audio_stream_in_t>(stream)),
      mEfGroup(nullptr),
      mStopReadThread(false) {}