//  @  /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioRoute.h
class AudioRoute  : public virtual RefBase{

}

AudioRoute::AudioRoute(audio_route_type_t type) : mType(type) {

}

void AudioRoute::setSink(const sp<AudioPort> &sink) { 
    mSink = sink; 
}


void AudioRoute::setSources(const AudioPortVector &sources) { 
    mSources = sources; 
}
















void AudioRoute::dump(int fd, int spaces) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    snprintf(buffer, SIZE, "%*s- Type: %s\n", spaces, "", mType == AUDIO_ROUTE_MUX ? "Mux" : "Mix");
    result.append(buffer);

    snprintf(buffer, SIZE, "%*s- Sink: %s\n", spaces, "", mSink->getTagName().string());
    result.append(buffer);

    if (mSources.size() != 0) {
        snprintf(buffer, SIZE, "%*s- Sources: \n", spaces, "");
        result.append(buffer);
        for (size_t i = 0; i < mSources.size(); i++) {
            snprintf(buffer, SIZE, "%*s%s \n", spaces + 4, "", mSources[i]->getTagName().string());
            result.append(buffer);
        }
    }
    result.append("\n");

    write(fd, result.string(), result.size());
}
