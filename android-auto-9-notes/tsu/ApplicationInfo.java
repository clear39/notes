//  @   /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/base/core/java/android/content/pm/ApplicationInfo.java


public void initForUser(int userId) {
    uid = UserHandle.getUid(userId, UserHandle.getAppId(uid));

    if ("android".equals(packageName)) {
        dataDir = Environment.getDataSystemDirectory().getAbsolutePath();
        return;
    }

    deviceProtectedDataDir = Environment
            .getDataUserDePackageDirectory(volumeUuid, userId, packageName)
            .getAbsolutePath();
    credentialProtectedDataDir = Environment
            .getDataUserCePackageDirectory(volumeUuid, userId, packageName)
            .getAbsolutePath();

    /**
     * public static final boolean APPLY_DEFAULT_TO_DEVICE_PROTECTED_STORAGE = true;
     */
    if ((privateFlags & PRIVATE_FLAG_DEFAULT_TO_DEVICE_PROTECTED_STORAGE) != 0 && PackageManager.APPLY_DEFAULT_TO_DEVICE_PROTECTED_STORAGE) {
        dataDir = deviceProtectedDataDir;
    } else {
        dataDir = credentialProtectedDataDir;
    }
}