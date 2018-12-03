//	@frameworks/av/media/libstagefright/MediaCodec.cpp:37:#include <media/IResourceManagerService.h>
//从记录来看MediaCodec.cpp中用到,具体作用后面再研究




//		@frameworks/av/services/mediaresourcemanager/ResourceManagerService.cpp

static char const *ResourceManagerService::getServiceName() { return "media.resource_manager"; }

ResourceManagerService::ResourceManagerService(): ResourceManagerService(new ProcessInfo()) {}

ResourceManagerService::ResourceManagerService(sp<ProcessInfoInterface> processInfo)
    : mProcessInfo(processInfo),
      mServiceLog(new ServiceLog()),
      mSupportsMultipleSecureCodecs(true),
      mSupportsSecureWithNonSecureCodec(true) {}