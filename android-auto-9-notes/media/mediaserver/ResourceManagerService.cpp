//  @frameworks/av/services/mediaresourcemanager/ResourceManagerService.cpp

//  ProcessInfo @frameworks/av/media/utils/ProcessInfo.cpp
ResourceManagerService::ResourceManagerService(): ResourceManagerService(new ProcessInfo()) {}


/**
 * ProcessInfo 封装了 访问 processinfo 服务接口 ，
 *   processinfo 服务 实现在ActivityManagerService.java 中 ,作用主要是提供个外部查找进程的状态和omj值
 * 
 * @    frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java:2735:            ServiceManager.addService("processinfo", new ProcessInfoService(this));
 * 
 */ 
ResourceManagerService::ResourceManagerService(sp<ProcessInfoInterface> processInfo)
    : mProcessInfo(processInfo),
      mServiceLog(new ServiceLog()),    //  mServiceLog日志保存，dumpsys 命令输出
      mSupportsMultipleSecureCodecs(true),
      mSupportsSecureWithNonSecureCodec(true),
      mCpuBoostCount(0) {}



//配置 mSupportsMultipleSecureCodecs 和 mSupportsSecureWithNonSecureCodec
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

struct ResourceInfo {
    int64_t clientId;
    sp<IResourceManagerClient> client;
    sp<IBinder::DeathRecipient> deathNotifier;
    Vector<MediaResource> resources;
    bool cpuBoost;
};

typedef Vector<ResourceInfo> ResourceInfos;
typedef KeyedVector<int, ResourceInfos> PidResourceInfosMap;

void ResourceManagerService::addResource(
        int pid,
        int64_t clientId,
        const sp<IResourceManagerClient> client,
        const Vector<MediaResource> &resources) {
    String8 log = String8::format("addResource(pid %d, clientId %lld, resources %s)",pid, (long long) clientId, getString(resources).string());
    mServiceLog->add(log);

    Mutex::Autolock lock(mLock);
    //isValidPid 判断进程 pid  是否为本身或者为调用者本省，这里主要防止进程访问非本省资源
    if (!mProcessInfo->isValidPid(pid)) {
        ALOGE("Rejected addResource call with invalid pid.");
        return;
    }

    // 通过pid在mMap中查询 对应 ResourceInfos，如果没有找到，则进行添加一个ResourceInfos(这是集合)再返回
    ResourceInfos& infos = getResourceInfosForEdit(pid, mMap);
    //通过 clientId 在 ResourceInfos集合中查询，如果没有同样创建一个再返回
    ResourceInfo& info = getResourceInfoForEdit(clientId, client, infos);  
    // TODO: do the merge instead of append.
    info.resources.appendVector(resources);//添加resources

    for (size_t i = 0; i < resources.size(); ++i) {
        if (resources[i].mType == MediaResource::kCpuBoost && !info.cpuBoost) {
            info.cpuBoost = true;
            // Request it on every new instance of kCpuBoost, as the media.codec
            // could have died, if we only do it the first time subsequent instances
            // never gets the boost（促进）.

            //  @frameworks/av/media/utils/SchedulingPolicyService.cpp
            if (requestCpusetBoost(true, this) != OK) {
                ALOGW("couldn't request cpuset boost");
            }
            mCpuBoostCount++;
        }
    }
    if (info.deathNotifier == nullptr) {
        info.deathNotifier = new DeathNotifier(this, pid, clientId);
        IInterface::asBinder(client)->linkToDeath(info.deathNotifier);
    }
    notifyResourceGranted(pid, resources);
}

//  @frameworks/av/media/utils/SchedulingPolicyService.cpp
int requestCpusetBoost(bool enable, const sp<IInterface> &client)
{
    int ret;
    sMutex.lock();
    /**
     * scheduling_policy 服务， 该服务是有SystemServer.java中添加的
     * 具体实现:    @frameworks/base/services/core/java/com/android/server/os/SchedulingPolicyService.java
     * 作用:     
     */ 
    sp<ISchedulingPolicyService> sps = sSchedulingPolicyService;  // Scheduling(行程安排)
    sMutex.unlock();
    if (sps == 0) {
        sp<IBinder> binder = defaultServiceManager()->checkService(_scheduling_policy);
        if (binder == 0) {
            return DEAD_OBJECT;
        }
        sps = interface_cast<ISchedulingPolicyService>(binder);
        sMutex.lock();
        sSchedulingPolicyService = sps;
        sMutex.unlock();
    }
    ret = sps->requestCpusetBoost(enable, client);
    if (ret != DEAD_OBJECT) {
        return ret;
    }
    ALOGW("SchedulingPolicyService died");
    sMutex.lock();
    sSchedulingPolicyService.clear();
    sMutex.unlock();
    return ret;
}