/***
 * AudioCommandThread 类是 AudioPolicyService内部类，为了方便单独弄出来
 * 实现代码在 @ /work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/services/audiopolicy/service/AudioPolicyService.cpp 
 */ 


// Thread(false) 中 表示线程是c层启动
AudioPolicyService::AudioCommandThread::AudioCommandThread(String8 name,const wp<AudioPolicyService>& service)
    : Thread(false), mName(name), mService(service)
{
    mpToneGenerator = NULL;
}


void AudioPolicyService::AudioCommandThread::onFirstRef()
{
    run(mName.string(), ANDROID_PRIORITY_AUDIO);//启动线程执行
}