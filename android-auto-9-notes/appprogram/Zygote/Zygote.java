//  @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/base/core/java/com/android/internal/os/Zygote.java
public final class Zygote {
    /** Called for some security initialization before any fork. */
    native static void nativeSecurityInit();
}


//  @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/base/core/jni/com_android_internal_os_Zygote.cpp
static void com_android_internal_os_Zygote_nativeSecurityInit(JNIEnv*, jclass) {
    // security_getenforce is not allowed on app process. Initialize and cache the value before
    // zygote forks.
    g_is_security_enforced = security_getenforce();
}