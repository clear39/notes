//	@frameworks/av/services/mediaextractor/MediaExtractorService.cpp
sp<IMediaExtractor> MediaExtractorService::makeExtractor(const sp<IDataSource> &remoteSource, const char *mime) {
    ALOGV("@@@ MediaExtractorService::makeExtractor for %s", mime);

    sp<DataSource> localSource = DataSource::CreateFromIDataSource(remoteSource);

    sp<IMediaExtractor> ret = MediaExtractor::CreateFromService(localSource, mime);

    ALOGV("extractor service created %p (%s)", ret.get(), ret == NULL ? "" : ret->name());

    if (ret != NULL) {
        registerMediaExtractor(ret, localSource, mime);
    }

    return ret;
}


sp<DataSource> DataSource::CreateFromIDataSource(const sp<IDataSource> &source) {
    return new TinyCacheSource(new CallbackDataSource(source));
}


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
    value = property_get_int32("media.fsl_codec.flag", 0);
    if(value & 0x01)
        use_fsl = true;

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
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AMR_NB)
            || !strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AMR_WB)) {
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





//  @frameworks/av/media/libmedia/IMediaExtractor.cpp
void registerMediaExtractor(const sp<IMediaExtractor> &extractor,const sp<DataSource> &source,const char *mime) {
    ExtractorInstance ex;
    ex.mime = mime == NULL ? "NULL" : mime;
    ex.name = extractor->name();
    ex.sourceDescription = source->toString();
    ex.owner = IPCThreadState::self()->getCallingPid();
    ex.extractor = extractor;

    {
        Mutex::Autolock lock(sExtractorsLock);
        if (sExtractors.size() > 10) {
            sExtractors.resize(10);
        }
        sExtractors.push_front(ex);//   static Vector<ExtractorInstance> sExtractors;
    }
}















sp<IDataSource> MediaExtractorService::makeIDataSource(int fd, int64_t offset, int64_t length)
{
    sp<DataSource> source = DataSource::CreateFromFd(fd, offset, length);
    return source.get() != nullptr ? source->asIDataSource() : nullptr;
}



sp<DataSource> DataSource::CreateFromFd(int fd, int64_t offset, int64_t length) {
    sp<FileSource> source = new FileSource(fd, offset, length);
    return source->initCheck() != OK ? nullptr : source;
}


// nameForFd @frameworks/av/media/libstagefright/Utils.cpp
AString nameForFd(int fd) {
    const size_t SIZE = 256;
    char buffer[SIZE];
    AString result;
    snprintf(buffer, SIZE, "/proc/%d/fd/%d", getpid(), fd);
    struct stat s;
    if (lstat(buffer, &s) == 0) {
        if ((s.st_mode & S_IFMT) == S_IFLNK) {
            char linkto[256];
            int len = readlink(buffer, linkto, sizeof(linkto));
            if(len > 0) {
                if(len > 255) {
                    linkto[252] = '.';
                    linkto[253] = '.';
                    linkto[254] = '.';
                    linkto[255] = 0;
                } else {
                    linkto[len] = 0;
                }
                result.append(linkto);
            }
        } else {
            result.append("unexpected type for ");
            result.append(buffer);
        }
    } else {
        result.append("couldn't open ");
        result.append(buffer);
    }
    return result;
}

FileSource::FileSource(int fd, int64_t offset, int64_t length)
    : mFd(fd),
      mOffset(offset),
      mLength(length),
      mName("<null>"),
      mDecryptHandle(NULL),
      mDrmManagerClient(NULL),
      mDrmBufOffset(0),
      mDrmBufSize(0),
      mDrmBuf(NULL) {
    ALOGV("fd=%d (%s), offset=%lld, length=%lld", fd, nameForFd(fd).c_str(), (long long) offset, (long long) length);

    if (mOffset < 0) {
        mOffset = 0;
    }
    if (mLength < 0) {
        mLength = 0;
    }
    if (mLength > INT64_MAX - mOffset) {
        mLength = INT64_MAX - mOffset;
    }
    struct stat s;
    if (fstat(fd, &s) == 0) {
        if (mOffset > s.st_size) {
            mOffset = s.st_size;
            mLength = 0;
        }
        if (mOffset + mLength > s.st_size) {
            mLength = s.st_size - mOffset;
        }
    }
    if (mOffset != offset || mLength != length) {
        ALOGW("offset/length adjusted from %lld/%lld to %lld/%lld",
                (long long) offset, (long long) length,
                (long long) mOffset, (long long) mLength);
    }

    mName = String8::format( "FileSource(fd(%s), %lld, %lld)", nameForFd(fd).c_str(), (long long) mOffset, (long long) mLength);

}