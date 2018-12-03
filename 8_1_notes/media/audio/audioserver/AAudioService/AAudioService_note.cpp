class AAudioService : public BinderService<AAudioService>,public BnAAudioService,public aaudio::AAudioServiceInterface
{}


//	@frameworks/av/services/oboeservice/AAudioService.cpp
android::AAudioService::AAudioService()  : BnAAudioService() {
    mAudioClient.clientUid = getuid();   // TODO consider using geteuid()
    mAudioClient.clientPid = getpid();
    mAudioClient.packageName = String16("");
    AAudioClientTracker::getInstance().setAAudioService(this);
}
