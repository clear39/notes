//	@frameworks/av/media/libstagefright/FslExtractor.cpp
FslExtractor::FslExtractor(const sp<DataSource> &source,const char *mime)
    : mDataSource(source),
    mReader(new FslDataSourceReader(mDataSource)),
    mMime(strdup(mime)),
    bInit(false),
    mFileMetaData(new MetaData)
{
    memset(&mLibName,0,255);
    mLibHandle = NULL;
    IParser = NULL;
    parserHandle = NULL;
    mFileMetaData->setCString(kKeyMIMEType, mime);
    currentVideoTs = 0;
    currentAudioTs = 0;
    mVideoActived = false;
    bWaitForAudioStartTime = false;

    ALOGD("FslExtractor::FslExtractor mime=%s",mMime);
}


//	@frameworks/av/media/libstagefright/FslExtractor.cpp
FslDataSourceReader::FslDataSourceReader(const sp<DataSource> &source)
    :mDataSource(source),
    mOffset(0),
    mLength(0)
{
    off64_t size = 0;

    mIsLiveStreaming = (mDataSource->flags() & DataSource::kIsLiveSource);
    mIsStreaming = (mDataSource->flags() & DataSource::kIsCachingDataSource);

    if(!mIsLiveStreaming){
        status_t ret = mDataSource->getSize(&size);
        if(ret == OK)
            mLength = size;
        else if(ret > 0)
            mLength = ret;
    }else{
        mLength = 0;
    }

    ALOGV("FslDataSourceReader: mLength is  %" PRId64 "", mLength);

    bStopReading = false;
    memset(&mMaxBufferSize[0],0,MAX_TRACK_COUNT*sizeof(uint32_t));
}



sp<MetaData> FslExtractor::getMetaData()
{
    if(!bInit){
        status_t ret = OK;
        ret = Init();

        if(ret != OK)
            return NULL;
    }
    return mFileMetaData;
}



status_t FslExtractor::Init()
{
    status_t ret = OK;

    if(mReader == NULL)	//	FslDataSourceReader
        return UNKNOWN_ERROR;

    ALOGD("FslExtractor::Init BEGIN");
    memset (&fileOps, 0, sizeof(FslFileStream));
    fileOps.Open = appFileOpen;
    fileOps.Read= appReadFile;
    fileOps.Seek = appSeekFile;
    fileOps.Tell = appGetCurrentFilePos;
    fileOps.Size= appFileSize;
    fileOps.Close = appFileClose;
    fileOps.CheckAvailableBytes = appCheckAvailableBytes;
    fileOps.GetFlag = appGetFlag;

    memset (&memOps, 0, sizeof(ParserMemoryOps));
    memOps.Calloc = appCalloc;
    memOps.Malloc = appMalloc;
    memOps.Free= appFree;
    memOps.ReAlloc= appReAlloc;

    outputBufferOps.RequestBuffer = appRequestBuffer;
    outputBufferOps.ReleaseBuffer = appReleaseBuffer;
    ret = CreateParserInterface();
    if(ret != OK){
        ALOGE("FslExtractor create parser failed");
        return ret;
    }

    ret = ParseFromParser();

    ALOGD("FslExtractor::Init ret=%d",ret);

    if(ret == OK)
        bInit = true;
    return ret;
}

typedef struct{
    const char* name;
    const char* mime;
}fsl_mime_struct;
fsl_mime_struct mime_table[]={
    {"mp4",MEDIA_MIMETYPE_CONTAINER_MPEG4},
    {"mkv",MEDIA_MIMETYPE_CONTAINER_MATROSKA},
    {"avi",MEDIA_MIMETYPE_CONTAINER_AVI},
    {"asf",MEDIA_MIMETYPE_CONTAINER_ASF},
    {"flv",MEDIA_MIMETYPE_CONTAINER_FLV},
    {"mpg2",MEDIA_MIMETYPE_CONTAINER_MPEG2TS},
    {"mpg2",MEDIA_MIMETYPE_CONTAINER_MPEG2PS},
    {"rm",MEDIA_MIMETYPE_CONTAINER_RMVB},
    {"mp3",MEDIA_MIMETYPE_AUDIO_MPEG},
    {"aac",MEDIA_MIMETYPE_AUDIO_AAC_ADTS},
    {"ape",MEDIA_MIMETYPE_AUDIO_APE},
    {"flac",MEDIA_MIMETYPE_AUDIO_FLAC},
    {"dsf",MEDIA_MIMETYPE_CONTAINER_DSF},
};
status_t FslExtractor::GetLibraryName()
{
    const char * name = NULL;
    for (size_t i = 0; i < sizeof(mime_table) / sizeof(mime_table[0]); i++) {
        if (!strcmp((const char *)mMime, mime_table[i].mime)) {
            name = mime_table[i].name;
            break;
        }
    }
    if(name == NULL)
        return NAME_NOT_FOUND;

    strcpy(mLibName, "lib_");
    strcat(mLibName,name);
    strcat(mLibName,"_parser_arm11_elinux.3.0.so");

    ALOGD("GetLibraryName %s",mLibName);//	10-08 04:02:12.921  3253  5335 D FslExtractor: GetLibraryName lib_mp4_parser_arm11_elinux.3.0.so
    return OK;
}

status_t FslExtractor::CreateParserInterface()
{
    status_t ret = OK;
    int32 err = PARSER_SUCCESS;
    tFslParserQueryInterface  myQueryInterface;

    ret = GetLibraryName();
    if(ret != OK)
        return ret;

    do{
        mLibHandle = dlopen(mLibName, RTLD_NOW);
        if (mLibHandle == NULL){
            ret = UNKNOWN_ERROR;
            break;
        }
        ALOGD("load parser name %s",mLibName);
        myQueryInterface = (tFslParserQueryInterface)dlsym(mLibHandle, "FslParserQueryInterface");
        if(myQueryInterface == NULL){
            ret = UNKNOWN_ERROR;
            break;
        }

        IParser = new FslParserInterface;
        if(IParser == NULL){
            ret = UNKNOWN_ERROR;
            break;
        }

        err = myQueryInterface(PARSER_API_GET_VERSION_INFO, (void **)&IParser->getVersionInfo);
        if(err)
            break;

        if(!IParser->getVersionInfo){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        // create & delete
        err = myQueryInterface(PARSER_API_CREATE_PARSER, (void **)&IParser->createParser);
        if(err)
            break;

        if(!IParser->createParser){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_CREATE_PARSER2, (void **)&IParser->createParser2);
        if(err)
            ALOGW("IParser->createParser2 not found");

        err = myQueryInterface(PARSER_API_DELETE_PARSER, (void **)&IParser->deleteParser);
        if(err)
            break;

        if(!IParser->deleteParser){
            err = PARSER_ERR_INVALID_API;
            break;
        }
        //index init
        err = myQueryInterface(PARSER_API_INITIALIZE_INDEX, (void **)&IParser->initializeIndex);
        if(err)
            break;

        //movie properties
        err = myQueryInterface(PARSER_API_IS_MOVIE_SEEKABLE, (void **)&IParser->isSeekable);
        if(err)
            break;

        if(!IParser->isSeekable){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_GET_MOVIE_DURATION, (void **)&IParser->getMovieDuration);
        if(err)
            break;

        if(!IParser->getMovieDuration){
            err = PARSER_ERR_INVALID_API;
            break;
        }
        err = myQueryInterface(PARSER_API_GET_USER_DATA, (void **)&IParser->getUserData);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_META_DATA, (void **)&IParser->getMetaData);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_NUM_TRACKS, (void **)&IParser->getNumTracks);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_NUM_PROGRAMS, (void **)&IParser->getNumPrograms);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_PROGRAM_TRACKS, (void **)&IParser->getProgramTracks);
        if(err)
            break;

        if((!IParser->getNumTracks && !IParser->getNumPrograms)
            ||(IParser->getNumPrograms && !IParser->getProgramTracks))
        {
            ALOGE("Invalid API to get tracks or programs.");
            err = PARSER_ERR_INVALID_API;
            break;
        }

        //track properties
        err = myQueryInterface(PARSER_API_GET_TRACK_TYPE, (void **)&IParser->getTrackType);
        if(err)
            break;

        if(!IParser->getTrackType){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_GET_TRACK_DURATION, (void **)&IParser->getTrackDuration);
        if(err)
            break;
        if(!IParser->getTrackDuration){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_GET_LANGUAGE, (void **)&IParser->getLanguage);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_BITRATE, (void **)&IParser->getBitRate);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_DECODER_SPECIFIC_INFO, (void **)&IParser->getDecoderSpecificInfo);
        if(err)
            break;

        //ignore the result because it is new api and some parser did implement it.
        err = myQueryInterface(PARSER_API_GET_TRACK_EXT_TAG, (void **)&IParser->getTrackExtTag);
        if(err)
            err = PARSER_SUCCESS;

        //video properties
        err = myQueryInterface(PARSER_API_GET_VIDEO_FRAME_WIDTH, (void **)&IParser->getVideoFrameWidth);
        if(err)
            break;
        err = myQueryInterface(PARSER_API_GET_VIDEO_FRAME_HEIGHT, (void **)&IParser->getVideoFrameHeight);
        if(err)
            break;
        err = myQueryInterface(PARSER_API_GET_VIDEO_FRAME_RATE, (void **)&IParser->getVideoFrameRate);
        if(err)
            break;
        err = myQueryInterface(PARSER_API_GET_VIDEO_FRAME_ROTATION, (void **)&IParser->getVideoFrameRotation);
        if(err){
            IParser->getVideoFrameRotation = NULL;
            err = PARSER_SUCCESS;
        }

        err = myQueryInterface(PARSER_API_GET_VIDEO_COLOR_INFO, (void **)&IParser->getVideoColorInfo);
        if(err){
            IParser->getVideoColorInfo = NULL;
            err = PARSER_SUCCESS;
        }

        err = myQueryInterface(PARSER_API_GET_VIDEO_HDR_COLOR_INFO, (void **)&IParser->getVideoHDRColorInfo);
        if(err){
            IParser->getVideoHDRColorInfo = NULL;
            err = PARSER_SUCCESS;
        }

        err = myQueryInterface(PARSER_API_GET_VIDEO_DISPLAY_WIDTH, (void **)&IParser->getVideoDisplayWidth);
        if(err){
            IParser->getVideoDisplayWidth = NULL;
            err = PARSER_SUCCESS;
        }

        err = myQueryInterface(PARSER_API_GET_VIDEO_DISPLAY_HEIGHT, (void **)&IParser->getVideoDisplayHeight);
        if(err){
            IParser->getVideoDisplayHeight = NULL;
            err = PARSER_SUCCESS;
        }

        //audio properties
        err = myQueryInterface(PARSER_API_GET_AUDIO_NUM_CHANNELS, (void **)&IParser->getAudioNumChannels);
        if(err)
            break;
        if(!IParser->getAudioNumChannels){
            err = PARSER_ERR_INVALID_API;
            break;
        }
        err = myQueryInterface(PARSER_API_GET_AUDIO_SAMPLE_RATE, (void **)&IParser->getAudioSampleRate);
        if(err)
            break;
        if(!IParser->getAudioSampleRate){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_GET_AUDIO_BITS_PER_SAMPLE, (void **)&IParser->getAudioBitsPerSample);
        if(err)
            break;
        err = myQueryInterface(PARSER_API_GET_AUDIO_BLOCK_ALIGN, (void **)&IParser->getAudioBlockAlign);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_AUDIO_CHANNEL_MASK, (void **)&IParser->getAudioChannelMask);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_AUDIO_BITS_PER_FRAME, (void **)&IParser->getAudioBitsPerFrame);
        if(err)
            break;

        //subtitle properties
        err = myQueryInterface(PARSER_API_GET_TEXT_TRACK_WIDTH, (void **)&IParser->getTextTrackWidth);
        if(err)
            break;
        err = myQueryInterface(PARSER_API_GET_TEXT_TRACK_HEIGHT, (void **)&IParser->getTextTrackHeight);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_TEXT_TRACK_MIME, (void **)&IParser->getTextTrackMime);
        if(err)
            break;

        //track reading function
        err = myQueryInterface(PARSER_API_GET_READ_MODE, (void **)&IParser->getReadMode);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_SET_READ_MODE, (void **)&IParser->setReadMode);
        if(err)
            break;

        if(!IParser->getReadMode || !IParser->setReadMode){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_ENABLE_TRACK, (void **)&IParser->enableTrack);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_NEXT_SAMPLE, (void **)&IParser->getNextSample);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_NEXT_SYNC_SAMPLE, (void **)&IParser->getNextSyncSample);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_FILE_NEXT_SAMPLE, (void **)&IParser->getFileNextSample);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_FILE_NEXT_SYNC_SAMPLE, (void **)&IParser->getFileNextSyncSample);
        if(err)
            break;

        if(!IParser->getNextSample && !IParser->getFileNextSample){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        if(IParser->getFileNextSample && !IParser->enableTrack){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_SEEK, (void **)&IParser->seek);
        if(err)
            break;
        if(!IParser->seek){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_GET_SAMPLE_CRYPTO_INFO, (void **)&IParser->getSampleCryptoInfo);

        if(!IParser->getSampleCryptoInfo){
            err = PARSER_SUCCESS;
        }

    }while(0);


    if(err){
        ALOGW("FslExtractor::CreateParserInterface parser err=%d",err);
        ret = UNKNOWN_ERROR;
    }

    if(ret == OK){
        ALOGD("FslExtractor::CreateParserInterface success");
    }else{
        if(mLibHandle)
            dlclose(mLibHandle);
        mLibHandle = NULL;

        if(IParser != NULL)
            delete IParser;
        IParser = NULL;
        ALOGW("FslExtractor::CreateParserInterface failed,ret=%d",ret);
    }

    return ret;
}



status_t FslExtractor::ParseFromParser()
{
    int32 err = (int32)PARSER_SUCCESS;
    uint32 flag = FLAG_H264_NO_CONVERT | FLAG_OUTPUT_PTS | FLAG_ID3_FORMAT_NON_UTF8 | FLAG_OUTPUT_H264_SEI_POS_DATA;

    uint32 trackCnt = 0;
    bool bLive = mReader->isLiveStreaming();
    ALOGI("Core parser %s \n", IParser->getVersionInfo());

    if(IParser->createParser2){
        if(mReader->isStreaming())
            flag |= FILE_FLAG_READ_IN_SEQUENCE;

        if(bLive){
            flag |= FILE_FLAG_NON_SEEKABLE;
            flag |= FILE_FLAG_READ_IN_SEQUENCE;
        }

        err = IParser->createParser2(flag,
                &fileOps,
                &memOps,
                &outputBufferOps,
                (void *)mReader,
                &parserHandle);
        ALOGD("createParser2 flag=%x,err=%d\n",flag,err);
    }else{
        err = IParser->createParser(bLive,
                &fileOps,
                &memOps,
                &outputBufferOps,
                (void *)mReader,
                &parserHandle);
        ALOGD("createParser flag=%x,err=%d\n",flag,err);
    }

    if(PARSER_SUCCESS !=  err)
    {
        ALOGE("fail to create the parser: %d\n", err);
        return UNKNOWN_ERROR;
    }
    if(mReader->isStreaming() || !strcasecmp(mMime, MEDIA_MIMETYPE_CONTAINER_MPEG2TS)
        || !strcasecmp(mMime, MEDIA_MIMETYPE_CONTAINER_MPEG2PS))
        mReadMode = PARSER_READ_MODE_FILE_BASED;
    else
        mReadMode = PARSER_READ_MODE_TRACK_BASED;

    err = IParser->setReadMode(parserHandle, mReadMode);
    if(PARSER_SUCCESS != err)
    {
        ALOGW("fail to set read mode to track mode\n");
        mReadMode = PARSER_READ_MODE_FILE_BASED;
        err = IParser->setReadMode(parserHandle, mReadMode);
        if(PARSER_SUCCESS != err)
        {
            ALOGE("fail to set read mode to file mode\n");
            return UNKNOWN_ERROR;
        }
    }

    if ((NULL == IParser->getNextSample && PARSER_READ_MODE_TRACK_BASED == mReadMode)
            || (NULL == IParser->getFileNextSample && PARSER_READ_MODE_FILE_BASED == mReadMode)){
        ALOGE("get next sample did not exist");
        return UNKNOWN_ERROR;
    }

    err = IParser->getNumTracks(parserHandle, &trackCnt);
    if(err)
        return UNKNOWN_ERROR;

    mNumTracks = trackCnt;
    if(IParser->initializeIndex){
        err = IParser->initializeIndex(parserHandle);
    }

    ALOGI("mReadMode=%d,mNumTracks=%u",mReadMode,mNumTracks);
    err = IParser->isSeekable(parserHandle,&bSeekable);
    if(err)
        return UNKNOWN_ERROR;

    ALOGI("bSeekable %d", bSeekable);

    err = IParser->getMovieDuration(parserHandle, (uint64 *)&mMovieDuration);
    if(err)
        return UNKNOWN_ERROR;

    err = ParseMetaData();
    if(err)
        return UNKNOWN_ERROR;

    err = ParseMediaFormat();
    if(err)
        return UNKNOWN_ERROR;
    return OK;
}


status_t FslExtractor::ParseMetaData()
{
    struct KeyMap {
        uint32_t tag;
        UserDataID key;
    };
    const KeyMap kKeyMap[] = {
        { kKeyTitle, USER_DATA_TITLE },
        { kKeyGenre, USER_DATA_GENRE },
        { kKeyArtist, USER_DATA_ARTIST },
        { kKeyYear, USER_DATA_YEAR },
        { kKeyAlbum, USER_DATA_ALBUM },
        { kKeyComposer, USER_DATA_COMPOSER },
        { kKeyWriter, USER_DATA_MOVIEWRITER },
        { kKeyCDTrackNumber, USER_DATA_TRACKNUMBER },
        { kKeyLocation, USER_DATA_LOCATION},
        //{ (char *)"totaltracknumber", USER_DATA_TOTALTRACKNUMBER},
        { kKeyDiscNumber, USER_DATA_DISCNUMBER},
        { kKeyYear, USER_DATA_CREATION_DATE},//map data to year for id3 parser & mp4 parser.
        { kKeyCompilation, USER_DATA_COMPILATION},
        { kKeyAlbumArtist, USER_DATA_ALBUMARTIST},
        { kKeyAuthor, USER_DATA_AUTHOR},
        { kKeyEncoderDelay,USER_DATA_AUD_ENC_DELAY},
        { kKeyEncoderPadding,USER_DATA_AUD_ENC_PADDING},
        { kKeyDate, USER_DATA_MP4_CREATION_TIME},//only get from mp4 parser.
    };
    uint32_t kNumMapEntries = sizeof(kKeyMap) / sizeof(kKeyMap[0]);

    if (IParser->getMetaData){

        uint8 *metaData = NULL;
        uint32 metaDataSize = 0;
        UserDataFormat userDataFormat;

        for (uint32_t i = 0; i < kNumMapEntries; ++i) {
            userDataFormat = USER_DATA_FORMAT_UTF8;
            IParser->getMetaData(parserHandle, kKeyMap[i].key, &userDataFormat, &metaData, &metaDataSize);

            if((metaData != NULL) && ((int32_t)metaDataSize > 0) && USER_DATA_FORMAT_UTF8 == userDataFormat)
            {
                if(metaDataSize > MAX_USER_DATA_STRING_LENGTH)
                    metaDataSize = MAX_USER_DATA_STRING_LENGTH;

                mFileMetaData->setCString(kKeyMap[i].tag, (const char*)metaData);
                ALOGI("FslParser Key: %d\t format=%d,size=%d,Value: %s\n",kKeyMap[i].key,userDataFormat,(int)metaDataSize,metaData);
            }else if((metaData != NULL) && ((int32)metaDataSize > 0) && USER_DATA_FORMAT_INT_LE == userDataFormat){
                if(metaDataSize == 4)
                    mFileMetaData->setInt32(kKeyMap[i].tag, *(int32*)metaData);
                ALOGI("FslParser Key2: %d\t format=%d,size=%d,Value: %d\n", kKeyMap[i].key,userDataFormat,(int)metaDataSize,*(int32*)metaData);
            }else if((metaData != NULL) && ((int32)metaDataSize > 0) && USER_DATA_FORMAT_UINT_LE == userDataFormat){
                if(USER_DATA_MP4_CREATION_TIME == kKeyMap[i].key && metaDataSize == 8){
                    uint64 data = *(uint64*)metaData;
                    String8 str;
                    if(ConvertMp4TimeToString(data,&str)){
                        mFileMetaData->setCString(kKeyMap[i].tag, str.string());
                        ALOGI("FslParser kKeyDate=%s",str.string());
                    }

                }
            }
        }

        //capture fps
        userDataFormat = USER_DATA_FORMAT_FLOAT32_BE;
        IParser->getMetaData(parserHandle, USER_DATA_CAPTURE_FPS, &userDataFormat, &metaData,&metaDataSize);
        if(4 == metaDataSize && metaData){
            char tmp[20] = {0};
            uint32 len = 0;
            uint32 value= 0;
            float data = 0.0;
            value += *metaData << 24;
            value += *(metaData+1) << 16;
            value += *(metaData+2) << 8;
            value += *(metaData+3);
            data = *(float *)&value;
            len = sprintf((char*)&tmp, "%f", data);
            ALOGI("get fps=%s,len=%u",tmp,len);
            mFileMetaData->setFloat(kKeyCaptureFramerate, data);
        }

        userDataFormat = USER_DATA_FORMAT_JPEG;
        IParser->getMetaData(parserHandle, USER_DATA_ARTWORK, &userDataFormat, &metaData, &metaDataSize);
        if(metaData && metaDataSize)
        {
            mFileMetaData->setData(kKeyAlbumArt, MetaData::TYPE_NONE,metaData, metaDataSize);
        }

        metaData = NULL;
        metaDataSize = 0;
        //pssh
        userDataFormat = USER_DATA_FORMAT_UTF8;
        IParser->getMetaData(parserHandle, USER_DATA_PSSH, &userDataFormat, &metaData, &metaDataSize);
        if(metaData && metaDataSize)
        {
            mFileMetaData->setData(kKeyPssh, 'pssh', metaData, metaDataSize);
        }
    }
    return OK;
}



status_t FslExtractor::ParseMediaFormat()
{
    uint32 trackCountToCheck = mNumTracks;
    uint32 programCount = 0;
    uint32 trackCountInOneProgram = 0;
    uint32_t index=0;
    uint32_t i = 0;
    uint32_t defaultProgram = 0;
    uint32 * pProgramTrackTable = NULL;
    int32 err = (int32)PARSER_SUCCESS;
    status_t ret = OK;
    MediaType trackType;
    uint32 decoderType;
    uint32 decoderSubtype;
    uint64 sSeekPosTmp = 0;
    ALOGD("FslExtractor::ParseMediaFormat BEGIN");
    if(IParser->getNumPrograms && IParser->getProgramTracks){
        err = IParser->getNumPrograms(parserHandle, &programCount);
        if(PARSER_SUCCESS !=  err || programCount == 0)
            return UNKNOWN_ERROR;

        err = IParser->getProgramTracks(parserHandle, defaultProgram, &trackCountInOneProgram, &pProgramTrackTable);
        if(PARSER_SUCCESS !=  err || trackCountInOneProgram == 0 || pProgramTrackTable == 0)
            return UNKNOWN_ERROR;

        trackCountToCheck = trackCountInOneProgram;
    }

    for(index = 0; index < trackCountToCheck; index ++){
        if(pProgramTrackTable)
            i = pProgramTrackTable[index];
        else
            i = index;

        err = IParser->getTrackType(parserHandle,i,(uint32 *)&trackType, &decoderType, &decoderSubtype);
        if(err)
            continue;

        if(trackType == MEDIA_VIDEO)
            ret = ParseVideo(i,decoderType,decoderSubtype);
        else if(trackType == MEDIA_AUDIO)
            ret = ParseAudio(i,decoderType,decoderSubtype);
        else if(trackType == MEDIA_TEXT)
            ret = ParseText(i,decoderType,decoderSubtype);

        if(ret)
            continue;

        err = IParser->seek(parserHandle, i, &sSeekPosTmp, SEEK_FLAG_NO_LATER);
        if(err)
            return UNKNOWN_ERROR;
    }

    return OK;
}


status_t FslExtractor::ParseAudio(uint32 index, uint32 type,uint32 subtype)
{
    int32 err = (int32)PARSER_SUCCESS;
    uint32_t i = 0;
    uint32 bitrate = 0;
    uint32 channel = 0;
    uint32 samplerate = 0;
    uint32 bitPerSample = 0;
    uint32 bitsPerFrame = 0;
    uint32 audioBlockAlign = 0;
    uint32 audioChannelMask = 0;
    const char* mime = NULL;
    uint64_t duration = 0;
    uint8 * decoderSpecificInfo = NULL;
    uint32 decoderSpecificInfoSize = 0;
    uint8 language[8];
    size_t sourceIndex = 0;
    int32_t encoderDelay = 0;
    int32_t encoderPadding = 0;

    ALOGD("ParseAudio index=%u,type=%u,subtype=%u",index,type,subtype);
    for(i = 0; i < sizeof(audio_mime_table)/sizeof(codec_mime_struct); i++){
        if (type == audio_mime_table[i].type){
            if((audio_mime_table[i].subtype > 0) && (subtype == (audio_mime_table[i].subtype))){
                mime = audio_mime_table[i].mime;
                break;
            }else if(audio_mime_table[i].subtype == 0){
                mime = audio_mime_table[i].mime;
                break;
            }
        }
    }

    if(mime == NULL)
        return UNKNOWN_ERROR;

    err = IParser->getTrackDuration(parserHandle, index,(uint64 *)&duration);
    if(err)
        return UNKNOWN_ERROR;

    if(IParser->getDecoderSpecificInfo){
        err = IParser->getDecoderSpecificInfo(parserHandle, index, &decoderSpecificInfo, &decoderSpecificInfoSize);
        if(err)
            return UNKNOWN_ERROR;
    }

    err = IParser->getBitRate(parserHandle, index, &bitrate);
    if(err)
        return UNKNOWN_ERROR;

    err = IParser->getAudioNumChannels(parserHandle, index, &channel);
    if(err)
        return UNKNOWN_ERROR;

    err = IParser->getAudioSampleRate(parserHandle, index, &samplerate);
    if(err)
        return UNKNOWN_ERROR;

    if(IParser->getAudioBitsPerSample){
        err = IParser->getAudioBitsPerSample(parserHandle, index, &bitPerSample);
        if(err)
            return UNKNOWN_ERROR;
    }
    if(IParser->getAudioBitsPerFrame){
        err = IParser->getAudioBitsPerFrame(parserHandle, index, &bitsPerFrame);
        if(err)
            return UNKNOWN_ERROR;
    }

    if(IParser->getAudioBlockAlign){
        err = IParser->getAudioBlockAlign(parserHandle, index, &audioBlockAlign);//wma & adpcm
        if(err)
            return UNKNOWN_ERROR;
    }

    if(IParser->getAudioChannelMask){
        err = IParser->getAudioChannelMask(parserHandle, index, &audioChannelMask);//not use
        if(err)
            return UNKNOWN_ERROR;
    }
    if(IParser->getLanguage) {
        memset(language, 0, sizeof(language)/sizeof(language[0]));
        err = IParser->getLanguage(parserHandle, index, &language[0]);
        ALOGI("audio track %u, lanuage: %s\n", index, language);
    }
    else
        strcpy((char*)&language, "unknown");

    sp<MetaData> meta = new MetaData;

    // switch to google.aac.decoder for m4a clips to pass testDecodeM4a, MA-8801
    const char *containerMime;
    mFileMetaData->findCString(kKeyMIMEType, &containerMime);
    if(type == AUDIO_AAC && subtype != AUDIO_ER_BSAC && !strcmp(containerMime, MEDIA_MIMETYPE_CONTAINER_MPEG4) && mNumTracks == 1){
        mime = MEDIA_MIMETYPE_AUDIO_AAC;
    }
    meta->setCString(kKeyMIMEType, mime);
    meta->setInt32(kKeyTrackID, index);

    if(decoderSpecificInfoSize > 0 && decoderSpecificInfo != NULL){
        ALOGI("audio codec data size=%u",decoderSpecificInfoSize);
        if(type == AUDIO_AAC){
            addESDSFromCodecPrivate(meta,true,decoderSpecificInfo,decoderSpecificInfoSize);
            ALOGI("add esds metadata for aac audio size=%u",decoderSpecificInfoSize);
        }else if(type == AUDIO_VORBIS){
            //TODO:
            //meta->setData(kKeyVorbisInfo, 0, decoderSpecificInfo, decoderSpecificInfoSize);
            if(OK != addVorbisCodecInfo(meta,decoderSpecificInfo,decoderSpecificInfoSize))
                ALOGE("add vorbis codec info error");
        }else if(type == AUDIO_OPUS){
            int64_t defaultValue = 0;
            meta->setData(kKeyOpusHeader, 0, decoderSpecificInfo, decoderSpecificInfoSize);
            meta->setInt64(kKeyOpusCodecDelay, defaultValue);
            meta->setInt64(kKeyOpusSeekPreRoll, defaultValue);
        }else{
            meta->setData(kKeyCodecData, 0, decoderSpecificInfo, decoderSpecificInfoSize);
        }
    }

    if(type == AUDIO_PCM && (subtype == AUDIO_PCM_S16BE || subtype == AUDIO_PCM_S24BE || subtype == AUDIO_PCM_S32BE ))
        meta->setInt32(kKeyIsEndianBig, 1);

    size_t max_size = MAX_AUDIO_BUFFER_SIZE;//16*1024
    if(type == AUDIO_APE) {
        max_size = 262144; //enlarge buffer size to 256*1024 for ape audio
        meta->setInt32(kKeyMaxInputSize, max_size);
    }
    else if(type == AUDIO_MP3){
        max_size = 8192;
        meta->setInt32(kKeyMaxInputSize, max_size);
    }
    else if(type == AUDIO_DSD){
        const int DSD_BLOCK_SIZE = 4096;
        const int DSD_CHANNEL_NUM_MAX = 6;
        max_size = DSD_BLOCK_SIZE * DSD_CHANNEL_NUM_MAX;
    }

    if(type == AUDIO_WMA){
        int32_t wmaType = 0;
        switch(subtype){
            case AUDIO_WMA1:
                wmaType = OMX_AUDIO_WMAFormat7;
                break;
            case AUDIO_WMA2:
                wmaType = OMX_AUDIO_WMAFormat8;
                break;
            case AUDIO_WMA3:
                wmaType = OMX_AUDIO_WMAFormat9;
                break;
            case AUDIO_WMALL:
                wmaType = OMX_AUDIO_WMAFormatLL;
                break;
            default:
                break;
        }
        meta->setInt32(kKeySubFormat, wmaType);
        ALOGI("WMA subtype=%u",wmaType);
    }


    /*
      stagefright's mediaextractor doesn't read meta data bitPerSample from
      file, fslExtractor shall be same with it, otherwise cts NativeDecoderTest will fail.
      This cts uses aac&vorbis tracks, acodec needs bitPerSample for wma&ape tracks,
      so just block aac&vorbis from passing bitPerSample.
      */
    if(bitPerSample > 0 && type != AUDIO_AAC && type != AUDIO_VORBIS)
        meta->setInt32(kKeyBitPerSample,bitPerSample);
    if(audioBlockAlign > 0)
        meta->setInt32(kKeyAudioBlockAlign,audioBlockAlign);
    if(bitsPerFrame > 0)
        meta->setInt32(kKeyBitsPerFrame,bitsPerFrame);
    if(type == AUDIO_AAC) {
        if(subtype == AUDIO_AAC_ADTS)
            meta->setInt32(kKeyIsADTS, true);
        else if (subtype == AUDIO_AAC_ADIF)
            meta->setInt32(kKeyIsADIF, true);
    }else if(bitrate > 0)
        meta->setInt32(kKeyBitRate, bitrate);

    if(type == AUDIO_AMR){
        if(subtype == AUDIO_AMR_NB){
            channel = 1;
            samplerate = 8000;
        }else if(subtype == AUDIO_AMR_WB){
            channel = 1;
            samplerate = 16000;
        }
    }

    if(type == AUDIO_AC3 && samplerate == 0)
        samplerate = 44100; // invalid samplerate will lead to findMatchingCodecs fail

    meta->setInt64(kKeyDuration, duration);
    meta->setInt32(kKeyChannelCount, channel);
    meta->setInt32(kKeySampleRate, samplerate);
    meta->setCString(kKeyMediaLanguage, (const char*)&language);

    if(mFileMetaData->findInt32(kKeyEncoderDelay,&encoderDelay))
        meta->setInt32(kKeyEncoderDelay, encoderDelay);
    if(mFileMetaData->findInt32(kKeyEncoderPadding,&encoderPadding))
        meta->setInt32(kKeyEncoderPadding, encoderPadding);

    ParseTrackExtMetadata(index,meta);

#if 0//test
    if(type == AUDIO_MP3) {
        meta->setInt32(kKeyEncoderDelay, 576);
        meta->setInt32(kKeyEncoderPadding, 1908);

    }
#endif
    ALOGI("ParseAudio channel=%d,sampleRate=%d,bitRate=%d,bitPerSample=%d,audioBlockAlign=%d",
        (int)channel,(int)samplerate,(int)bitrate,(int)bitPerSample,(int)audioBlockAlign);
    mTracks.push();
    sourceIndex = mTracks.size() - 1;
    TrackInfo *trackInfo = &mTracks.editItemAt(sourceIndex);
    trackInfo->mTrackNum = index;
    trackInfo->mExtractor = this;
    trackInfo->bCodecInfoSent = false;
    trackInfo->mMeta = meta;
    trackInfo->bPartial = false;
    trackInfo->buffer = NULL;
    trackInfo->outTs = 0;
    trackInfo->syncFrame = 0;
    trackInfo->mSource = NULL;
    trackInfo->max_input_size = max_size;
    trackInfo->type = MEDIA_AUDIO;
    trackInfo->bIsNeedConvert = (type == AUDIO_PCM && bitPerSample!= 16);
    trackInfo->bitPerSample = bitPerSample;
    trackInfo->bMp4Encrypted = false;
    trackInfo->bMkvEncrypted = false;

    if(meta->hasData(kKeyCryptoKey)){
        if(!strcmp(mMime, MEDIA_MIMETYPE_CONTAINER_MPEG4)){
            uint32_t type;
            const void *data;
            size_t size;
            int32 mode;
            int32 iv_size;
            if(meta->findData(kKeyCryptoKey,&type,&data,&size) && data && size <=16)
                memcpy(&trackInfo->default_kid,data,size);
            if(meta->findInt32(kKeyCryptoMode,&mode))
                trackInfo->default_isEncrypted = mode;
            if(meta->findInt32(kKeyCryptoDefaultIVSize,&iv_size))
                trackInfo->default_iv_size = iv_size;

            trackInfo->bMp4Encrypted = true;
        }
    }

    mReader->AddBufferReadLimitation(index,max_size);
    ALOGI("add audio track index=%u,sourceIndex=%zu,mime=%s",index,sourceIndex,mime);
    return OK;
}






size_t FslExtractor::countTracks()
{
    status_t ret = OK;
    if(!bInit){
        ret = Init();

        if(ret != OK)
            return 0;
    }

    return mTracks.size();
}




sp<IMediaSource> FslExtractor::getTrack(size_t index)
{
    sp<FslMediaSource> source;
    sp<MetaData> meta;
    if (index >= mTracks.size()) {
        return NULL;
    }
    ALOGD("FslExtractor::getTrack index=%zu",index);

    if (index >= mTracks.size()) {
        return NULL;
    }
    TrackInfo *trackInfo = &mTracks.editItemAt(index);

    meta = trackInfo->mMeta;
    source = new FslMediaSource(this,index,meta);

    trackInfo->mSource = source;
    ALOGE("getTrack source string cnt=%d",source->getStrongCount());

    return source;
}



sp<MetaData> FslExtractor::getTrackMetaData(size_t index, uint32_t flags) {
    if(!bInit){
        status_t ret = OK;
        ret = Init();

        if(ret != OK)
            return NULL;
    }
    if(flags){
        ;//
    }
    return mTracks.itemAt(index).mMeta;
}





status_t FslExtractor::ActiveTrack(uint32 index)
{
    uint64 seekPos = 0;
    Mutex::Autolock autoLock(mLock);
    bool seek = true;

    TrackInfo *trackInfo = &mTracks.editItemAt(index);
    if(trackInfo == NULL)
        return UNKNOWN_ERROR;
    trackInfo->bCodecInfoSent = false;
    if(trackInfo->type == MEDIA_VIDEO){
        seekPos = currentVideoTs;
        mVideoActived = true;
    }else if(trackInfo->type == MEDIA_AUDIO)
        seekPos = currentAudioTs;
    else if(currentVideoTs > 0)
        seekPos = currentVideoTs;
    else
        seekPos = currentAudioTs;

    IParser->enableTrack(parserHandle,trackInfo->mTrackNum, TRUE);

    if(trackInfo->type == MEDIA_TEXT || trackInfo->type == MEDIA_AUDIO){
        if(isTrackModeParser())
            seek = true;
        else
            seek = false;
    }

    if(seek)
        IParser->seek(parserHandle, trackInfo->mTrackNum, &seekPos, SEEK_FLAG_NO_LATER);

    ALOGD("start track %d",trackInfo->mTrackNum);
    return OK;
}