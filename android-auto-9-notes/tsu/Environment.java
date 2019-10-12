//  @   /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/base/core/java/android/os/Environment.java

/** {@hide} */
public static File getDataUserDePackageDirectory(String volumeUuid, int userId, String packageName) {
    // TODO: keep consistent with installd
    return new File(getDataUserDeDirectory(volumeUuid, userId), packageName);
}


/** {@hide} */
public static File getDataUserDeDirectory(String volumeUuid, int userId) {
    return new File(getDataUserDeDirectory(volumeUuid), String.valueOf(userId));
}

/** {@hide} */
public static File getDataUserDeDirectory(String volumeUuid) {
    return new File(getDataDirectory(volumeUuid), "user_de");
}


/**
 * Return the user data directory.
 */
public static File getDataDirectory() {
    return DIR_ANDROID_DATA;  //    /data
}