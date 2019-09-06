class AAudioClientTracker : public android::Singleton<AAudioClientTracker>{

}


//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/oboeservice/AAudioClientTracker.cpp
AAudioClientTracker::AAudioClientTracker()
        : Singleton<AAudioClientTracker>() {
}
/**
 * 再 AAudioService 构造函数中调用
 */ 
void setAAudioService(android::AAudioService *aaudioService) {
    mAAudioService = aaudioService;
}