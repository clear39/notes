/**
 * 这里封装了谷歌组件，主要是软实现
 */ 
//  @frameworks/av/media/libstagefright/omx/SoftOMXPlugin.cpp
SoftOMXPlugin::SoftOMXPlugin() {

}


static const struct {
    const char *mName;
    const char *mLibNameSuffix;
    const char *mRole;
} kComponents[] = {
    // two choices for aac decoding.
    // configurable in media/libstagefright/data/media_codecs_google_audio.xml
    // default implementation
    { "OMX.google.aac.decoder", "aacdec", "audio_decoder.aac" },
    // alternate implementation
    { "OMX.google.xaac.decoder", "xaacdec", "audio_decoder.aac" },
    { "OMX.google.aac.encoder", "aacenc", "audio_encoder.aac" },
    { "OMX.google.amrnb.decoder", "amrdec", "audio_decoder.amrnb" },
    { "OMX.google.amrnb.encoder", "amrnbenc", "audio_encoder.amrnb" },
    { "OMX.google.amrwb.decoder", "amrdec", "audio_decoder.amrwb" },
    { "OMX.google.amrwb.encoder", "amrwbenc", "audio_encoder.amrwb" },
    { "OMX.google.h264.decoder", "avcdec", "video_decoder.avc" },
    { "OMX.google.h264.encoder", "avcenc", "video_encoder.avc" },
    { "OMX.google.hevc.decoder", "hevcdec", "video_decoder.hevc" },
    { "OMX.google.g711.alaw.decoder", "g711dec", "audio_decoder.g711alaw" },
    { "OMX.google.g711.mlaw.decoder", "g711dec", "audio_decoder.g711mlaw" },
    { "OMX.google.mpeg2.decoder", "mpeg2dec", "video_decoder.mpeg2" },
    { "OMX.google.h263.decoder", "mpeg4dec", "video_decoder.h263" },
    { "OMX.google.h263.encoder", "mpeg4enc", "video_encoder.h263" },
    { "OMX.google.mpeg4.decoder", "mpeg4dec", "video_decoder.mpeg4" },
    { "OMX.google.mpeg4.encoder", "mpeg4enc", "video_encoder.mpeg4" },
    { "OMX.google.mp3.decoder", "mp3dec", "audio_decoder.mp3" },
    { "OMX.google.vorbis.decoder", "vorbisdec", "audio_decoder.vorbis" },
    { "OMX.google.opus.decoder", "opusdec", "audio_decoder.opus" },
    { "OMX.google.vp8.decoder", "vpxdec", "video_decoder.vp8" },
    { "OMX.google.vp9.decoder", "vpxdec", "video_decoder.vp9" },
    { "OMX.google.vp8.encoder", "vpxenc", "video_encoder.vp8" },
    { "OMX.google.vp9.encoder", "vpxenc", "video_encoder.vp9" },
    { "OMX.google.raw.decoder", "rawdec", "audio_decoder.raw" },
    { "OMX.google.flac.decoder", "flacdec", "audio_decoder.flac" },
    { "OMX.google.flac.encoder", "flacenc", "audio_encoder.flac" },
    { "OMX.google.gsm.decoder", "gsmdec", "audio_decoder.gsm" },
};

/**
 * 通过 组件名称获取角色roles名称，在OmxStore中 解析xml可以得到组件名称
 */ 
OMX_ERRORTYPE SoftOMXPlugin::getRolesOfComponent(const char *name,Vector<String8> *roles) {
    for (size_t i = 0; i < kNumComponents; ++i) {
        if (strcmp(name, kComponents[i].mName)) {
            continue;
        }

        roles->clear();
        roles->push(String8(kComponents[i].mRole));

        return OMX_ErrorNone;
    }

    return OMX_ErrorInvalidComponentName;
}


/**
 * 通过组件名 name 加载对应的组件库（库名规则为 libstagefright_soft_[mLibNameSuffix].so）
 */ 
OMX_ERRORTYPE SoftOMXPlugin::makeComponentInstance(const char *name,const OMX_CALLBACKTYPE *callbacks,OMX_PTR appData,OMX_COMPONENTTYPE **component) {
    ALOGV("makeComponentInstance '%s'", name);

    for (size_t i = 0; i < kNumComponents; ++i) {
        if (strcmp(name, kComponents[i].mName)) {
            continue;
        }

        AString libName = "libstagefright_soft_";
        libName.append(kComponents[i].mLibNameSuffix);
        libName.append(".so");

        // RTLD_NODELETE means we keep the shared library around forever.
        // this eliminates thrashing during sequences like loading soundpools.
        // It also leaves the rest of the logic around the dlopen()/dlclose()
        // calls in this file unchanged.
        //
        // Implications of the change:
        // -- the codec process (where this happens) will have a slightly larger
        //    long-term memory footprint as it accumulates the loaded shared libraries.
        //    This is expected to be a small amount of memory.
        // -- plugin codecs can no longer (and never should have) depend on a
        //    free reset of any static data as the library would have crossed
        //    a dlclose/dlopen cycle.
        //

        void *libHandle = dlopen(libName.c_str(), RTLD_NOW|RTLD_NODELETE);

        if (libHandle == NULL) {
            ALOGE("unable to dlopen %s: %s", libName.c_str(), dlerror());

            return OMX_ErrorComponentNotFound;
        }

        typedef SoftOMXComponent *(*CreateSoftOMXComponentFunc)(const char *, const OMX_CALLBACKTYPE *,OMX_PTR, OMX_COMPONENTTYPE **);

        /**
         * 得到对应组件库的入口
         */ 
        CreateSoftOMXComponentFunc createSoftOMXComponent = (CreateSoftOMXComponentFunc)dlsym(libHandle, "_Z22createSoftOMXComponentPKcPK16OMX_CALLBACKTYPE" "PvPP17OMX_COMPONENTTYPE");

        if (createSoftOMXComponent == NULL) {
            dlclose(libHandle);
            libHandle = NULL;

            return OMX_ErrorComponentNotFound;
        }

        //执行调用组件入口函数
        sp<SoftOMXComponent> codec = (*createSoftOMXComponent)(name, callbacks, appData, component);

        if (codec == NULL) {
            dlclose(libHandle);
            libHandle = NULL;

            return OMX_ErrorInsufficientResources;
        }

        OMX_ERRORTYPE err = codec->initCheck();
        if (err != OMX_ErrorNone) {
            dlclose(libHandle);
            libHandle = NULL;

            return err;
        }

        codec->incStrong(this);
        codec->setLibHandle(libHandle);

        return OMX_ErrorNone;
    }

    return OMX_ErrorInvalidComponentName;
}
