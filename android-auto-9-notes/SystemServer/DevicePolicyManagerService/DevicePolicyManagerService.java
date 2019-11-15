//  @  frameworks/base/services/devicepolicy/java/com/android/server/devicepolicy/DevicePolicyManagerService.java
 

public class DevicePolicyManagerService extends BaseIDevicePolicyManager {

    private File getPolicyFileDirectory(@UserIdInt int userId) {
        /***
         * 如果为 UserHandle.USER_SYSTEM 用户返回路径 /data/system/
         * 如果为非 UserHandle.USER_SYSTEM 用户返回路径 /data/system/userId
         */
        return userId == UserHandle.USER_SYSTEM
                ? new File(mInjector.getDevicePolicyFilePathForSystemUser())
                : mInjector.environmentGetUserSystemDirectory(userId);
    }
    
}

