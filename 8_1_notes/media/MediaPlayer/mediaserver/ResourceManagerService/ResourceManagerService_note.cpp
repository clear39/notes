//	@frameworks/av/include/media/IResourceManagerService.h
// IResourceManagerService 客户端被封装在 MediaCodec.cpp中
class IResourceManagerService: public IInterface
{
public:
    DECLARE_META_INTERFACE(ResourceManagerService);

    virtual void config(const Vector<MediaResourcePolicy> &policies) = 0;

    virtual void addResource(
            int pid,
            int64_t clientId,
            const sp<IResourceManagerClient> client,
            const Vector<MediaResource> &resources) = 0;

    virtual void removeResource(int pid, int64_t clientId) = 0;

    virtual bool reclaimResource(
            int callingPid,
            const Vector<MediaResource> &resources) = 0;
};





//		@frameworks/av/services/mediaresourcemanager/ResourceManagerService.cpp

static char const *ResourceManagerService::getServiceName() { return "media.resource_manager"; }

ResourceManagerService::ResourceManagerService(): ResourceManagerService(new ProcessInfo()) {}

ResourceManagerService::ResourceManagerService(sp<ProcessInfoInterface> processInfo)
    : mProcessInfo(processInfo),
      mServiceLog(new ServiceLog()),
      mSupportsMultipleSecureCodecs(true),
      mSupportsSecureWithNonSecureCodec(true) {}


//目前没有看到调用接口
void ResourceManagerService::config(const Vector<MediaResourcePolicy> &policies) {
    String8 log = String8::format("config(%s)", getString(policies).string());
    mServiceLog->add(log);

    Mutex::Autolock lock(mLock);
    for (size_t i = 0; i < policies.size(); ++i) {
        String8 type = policies[i].mType;
        String8 value = policies[i].mValue;
        if (type == kPolicySupportsMultipleSecureCodecs) {
            mSupportsMultipleSecureCodecs = (value == "true");
        } else if (type == kPolicySupportsSecureWithNonSecureCodec) {
            mSupportsSecureWithNonSecureCodec = (value == "true");
        }
    }
}



void ResourceManagerService::addResource(int pid,int64_t clientId,const sp<IResourceManagerClient> client,const Vector<MediaResource> &resources) {
    String8 log = String8::format("addResource(pid %d, clientId %lld, resources %s)",pid, (long long) clientId, getString(resources).string());
    mServiceLog->add(log);

    Mutex::Autolock lock(mLock);
    if (!mProcessInfo->isValidPid(pid)) {
        ALOGE("Rejected addResource call with invalid pid.");
        return;
    }
    ResourceInfos& infos = getResourceInfosForEdit(pid, mMap);
    ResourceInfo& info = getResourceInfoForEdit(clientId, client, infos);
    // TODO: do the merge instead of append.
    info.resources.appendVector(resources);
    if (info.deathNotifier == nullptr) {
        info.deathNotifier = new DeathNotifier(this, pid, clientId);
        IInterface::asBinder(client)->linkToDeath(info.deathNotifier);
    }
    notifyResourceGranted(pid, resources);
}