//  @/work/workcodes/aosp-p9.x-auto-alpha/libcore/libart/src/main/java/dalvik/system/VMRuntime.java
public final class VMRuntime {
     /**
     * Returns whether the VM is running in 64-bit mode.
     */
    @FastNative
    public native boolean is64Bit();
}


//  @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/native/dalvik_system_VMRuntime.cc
static jboolean VMRuntime_is64Bit(JNIEnv*, jobject) {
    bool is64BitMode = (sizeof(void*) == sizeof(uint64_t));
    return is64BitMode ? JNI_TRUE : JNI_FALSE;
}
  