
// @ frameworks/base/services/core/java/com/android/server/pm/ApexManager.java
abstract class ApexManager {
	static ApexManager create(Context systemContext) {
        if (ApexProperties.updatable().orElse(false)) {
            try {
                return new ApexManagerImpl(systemContext, IApexService.Stub.asInterface(
                        ServiceManager.getServiceOrThrow("apexservice")));
            } catch (ServiceManager.ServiceNotFoundException e) {
                throw new IllegalStateException("Required service apexservice not available");
            }
        } else {
            return new ApexManagerFlattenedApex();
        }
    }
}