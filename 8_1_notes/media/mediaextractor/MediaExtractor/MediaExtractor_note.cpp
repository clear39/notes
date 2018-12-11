//	@frameworks/av/include/media/IMediaExtractor.h
class IMediaExtractor : public IInterface {
public:
    DECLARE_META_INTERFACE(MediaExtractor);

    virtual size_t countTracks() = 0;
    // This function could return NULL IMediaSource even when index is within the
    // track count returned by countTracks, since it's possible the track is malformed
    // and it's not detected during countTracks call.
    virtual sp<IMediaSource> getTrack(size_t index) = 0;

    enum GetTrackMetaDataFlags {
        kIncludeExtensiveMetaData = 1
    };
    virtual sp<MetaData> getTrackMetaData(
            size_t index, uint32_t flags = 0) = 0;

    // Return container specific meta-data. The default implementation
    // returns an empty metadata object.
    virtual sp<MetaData> getMetaData() = 0;

    virtual status_t getMetrics(Parcel *reply) = 0;

    enum Flags {
        CAN_SEEK_BACKWARD  = 1,  // the "seek 10secs back button"
        CAN_SEEK_FORWARD   = 2,  // the "seek 10secs forward button"
        CAN_PAUSE          = 4,
        CAN_SEEK           = 8,  // the "seek bar"
    };

    // If subclasses do _not_ override this, the default is
    // CAN_SEEK_BACKWARD | CAN_SEEK_FORWARD | CAN_SEEK | CAN_PAUSE
    virtual uint32_t flags() const = 0;

    // for DRM
    virtual char* getDrmTrackInfo(size_t trackID, int *len)  = 0;

    virtual status_t setMediaCas(const HInterfaceToken &casToken) = 0;

    virtual void setUID(uid_t uid)  = 0;

    virtual const char * name() = 0;

    virtual void release() = 0;
};



//	@frameworks/av/media/libstagefright/include/media/stagefright/MediaExtractor.h
class MediaExtractor : public BnMediaExtractor {}


//	@frameworks/av/media/libstagefright/MediaExtractor.cpp
sp<MediaExtractor> MediaExtractor::CreateFromService(const sp<DataSource> &source, const char *mime) {

    ALOGV("MediaExtractor::CreateFromService %s", mime);
    RegisterDefaultSniffers();

    // initialize source decryption if needed
    source->DrmInitialization(nullptr /* mime */);

    sp<AMessage> meta;

    String8 tmp;
    if (mime == NULL) {
        float confidence;
        if (!sniff(source, &tmp, &confidence, &meta)) {
            ALOGW("FAILED to autodetect media content.");

            return NULL;
        }

        mime = tmp.string();
        ALOGV("Autodetected media content as '%s' with confidence %.2f",mime, confidence);
    }

    bool use_fsl = false;
    int value;
    MediaExtractor *ret = NULL;
    value = property_get_int32("media.fsl_codec.flag", 0);//	7
    if(value & 0x01)
        use_fsl = true;  //这里执行

    if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_MPEG4)  || !strcasecmp(mime, "audio/mp4")) {
        if(use_fsl)
            ret = new FslExtractor(source,mime);
        else
            ret = new MPEG4Extractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_MPEG)) {
        if(use_fsl)
            ret = new FslExtractor(source,mime);
        else
            ret = new MP3Extractor(source, meta);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AMR_NB) || !strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AMR_WB)) {
        ret = new AMRExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_FLAC)) {
        if(use_fsl)
            ret = new FslExtractor(source,mime);
        else
            ret = new FLACExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_WAV)) {
        ret = new WAVExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_OGG)) {
        ret = new OggExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_MATROSKA)) {
        if(use_fsl)
            ret = new FslExtractor(source,mime);
        else
            ret = new MatroskaExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_MPEG2TS)) {
        if(use_fsl)
            ret = new FslExtractor(source,mime);
        else
            ret = new MPEG2TSExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AAC_ADTS)) {
        if(use_fsl)
            ret = new FslExtractor(source,mime);
        else
            ret = new AACExtractor(source, meta);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_MPEG2PS)) {
        if(use_fsl)
            ret = new FslExtractor(source,mime);
        else
            ret = new MPEG2PSExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_MIDI)) {
        ret = new MidiExtractor(source);
    } else if(!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_AVI)){
        ret = new FslExtractor(source,mime);
    } else if(!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_FLV)){
        ret = new FslExtractor(source,mime);
    } else if(!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_ASF)){
        ret = new FslExtractor(source,mime);
    } else if(!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_RMVB)){
        ret = new FslExtractor(source,mime);
    } else if(!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_DSF)){
        ret = new FslExtractor(source,mime);
    } else if(!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_APE)) {
        ret = new FslExtractor(source,mime);
    }

    if (ret != NULL) {
       // track the container format (mpeg, aac, wvm, etc)
       if (MEDIA_LOG) {//   #define MEDIA_LOG       1
            if (ret->mAnalyticsItem != NULL) {
              size_t ntracks = ret->countTracks();
              ret->mAnalyticsItem->setCString(kExtractorFormat,  ret->name());
              // tracks (size_t)
              ret->mAnalyticsItem->setInt32(kExtractorTracks,  ntracks);
              // metadata
              sp<MetaData> pMetaData = ret->getMetaData();
              if (pMetaData != NULL) {
                String8 xx = pMetaData->toString();
                // 'titl' -- but this verges into PII
                // 'mime'
                const char *mime = NULL;
                if (pMetaData->findCString(kKeyMIMEType, &mime)) {
                    ret->mAnalyticsItem->setCString(kExtractorMime,  mime);
                }
                // what else is interesting and not already available?
              }
            }
       }
    }

    return ret;
}



// static
void MediaExtractor::RegisterSniffer_l(SnifferFunc func) {
    for (List<SnifferFunc>::iterator it = gSniffers.begin();
         it != gSniffers.end(); ++it) {
        if (*it == func) {
            return;
        }
    }

    gSniffers.push_back(func);
}

// static
void MediaExtractor::RegisterDefaultSniffers() {
    Mutex::Autolock autoLock(gSnifferMutex);
    if (gSniffersRegistered) {
        return;
    }

    RegisterSniffer_l(SniffMPEG4);		//	@frameworks/av/media/libstagefright/MPEG4Extractor.cpp:5428
    RegisterSniffer_l(SniffMatroska);	//	@frameworks/av/media/libstagefright/matroska/MatroskaExtractor.cpp:1492
    RegisterSniffer_l(SniffOgg);		//	@frameworks/av/media/libstagefright/OggExtractor.cpp:1368
    RegisterSniffer_l(SniffWAV);		//	@frameworks/av/media/libstagefright/WAVExtractor.cpp:547
    RegisterSniffer_l(SniffFLAC);		//	@frameworks/av/media/libstagefright/FLACExtractor.cpp:846
    RegisterSniffer_l(SniffAMR);		//	@frameworks/av/media/libstagefright/AMRExtractor.cpp:341
    RegisterSniffer_l(SniffMPEG2TS);	//	@frameworks/av/media/libstagefright/mpeg2ts/MPEG2TSExtractor.cpp:649
    RegisterSniffer_l(SniffMP3);		//	@frameworks/av/media/libstagefright/MP3Extractor.cpp:669
    RegisterSniffer_l(SniffAAC);		//	frameworks/av/media/libstagefright/AACExtractor.cpp:340
    RegisterSniffer_l(SniffMPEG2PS);	//	@frameworks/av/media/libstagefright/mpeg2ts/MPEG2PSExtractor.cpp:753
    RegisterSniffer_l(SniffMidi);		//	@frameworks/av/media/libstagefright/MidiExtractor.cpp:310
    RegisterSniffer_l(SniffFSL);		//	@frameworks/av/media/libstagefright/FslInspector.cpp:560
    gSniffersRegistered = true;
}