class AAudioService :
    public BinderService<AAudioService>,
    public BnAAudioService,
    public aaudio::AAudioServiceInterface
{


}


/***
 * // dumpsys media.aaudio
 * 
 * frameworks/av/media/libaaudio/src/binding/IAAudioService.h:36:#define AAUDIO_SERVICE_NAME  "media.aaudio"
 * 
 * 对应接口 /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/media/libaaudio/src/binding/IAAudioService.cpp
 * 
 * 
 * 
 */ 

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/oboeservice/AAudioService.cpp
android::AAudioService::AAudioService()
    : BnAAudioService() {
    mAudioClient.clientUid = getuid();   // TODO consider using geteuid()
    mAudioClient.clientPid = getpid();
    mAudioClient.packageName = String16("");
    AAudioClientTracker::getInstance().setAAudioService(this);
}


