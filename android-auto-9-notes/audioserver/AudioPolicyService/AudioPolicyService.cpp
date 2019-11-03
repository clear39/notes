//  @ /work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/services/audiopolicy/service/AudioPolicyService.cpp
AudioPolicyService::AudioPolicyService()
    : BnAudioPolicyService(), mpAudioPolicyDev(NULL), mpAudioPolicy(NULL),
      mAudioPolicyManager(NULL), mAudioPolicyClient(NULL), mPhoneState(AUDIO_MODE_INVALID)
{
    //这里啥事没干
}

void AudioPolicyService::onFirstRef()
{
    {
        Mutex::Autolock _l(mLock);

        /***
         * 启动三个线程
         */ 
        // start tone playback thread
        mTonePlaybackThread = new AudioCommandThread(String8("ApmTone"), this);
        // start audio commands thread
        mAudioCommandThread = new AudioCommandThread(String8("ApmAudio"), this);
        // start output activity command thread
        mOutputCommandThread = new AudioCommandThread(String8("ApmOutput"), this);

        /***
         * 封装 毁掉给 AudioPolicyService 的接口
         * 
         * @    frameworks/av/services/audiopolicy/service/AudioPolicyClientImpl.cpp
         */ 
        mAudioPolicyClient = new AudioPolicyClient(this);
        //  @   /work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/services/audiopolicy/manager/AudioPolicyFactory.cpp
        /***
         * createAudioPolicyManager 只是new AudioPolicyManager 类并将 mAudioPolicyClient 传递给AudioPolicyManager
         * AudioPolicyClient 用于 AudioPolicyManager 回调 AudioPolicyService 的接口封装
         * 
         * extern "C" AudioPolicyInterface* createAudioPolicyManager(AudioPolicyClientInterface *clientInterface)
         * ---> new AudioPolicyManager(clientInterface);
         * 
         */ 
        mAudioPolicyManager = createAudioPolicyManager(mAudioPolicyClient);
    }

    /**
     * 
     */ 
    // load audio processing modules
    sp<AudioPolicyEffects> audioPolicyEffects = new AudioPolicyEffects();
    {
        Mutex::Autolock _l(mLock);
        mAudioPolicyEffects = audioPolicyEffects;
    }

    /**
     * 由于多用户管理，和ActivityManagerService交互
     */ 
    mUidPolicy = new UidPolicy(this);
    mUidPolicy->registerSelf();
}



