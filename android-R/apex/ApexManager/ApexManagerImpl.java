

//	@	frameworks/base/services/core/java/com/android/server/pm/ApexManager.java
static class ApexManagerImpl extends ApexManager {

	ApexManagerImpl(Context context, IApexService apexService) {
        mContext = context;
        mApexService = apexService;
    }
    
}