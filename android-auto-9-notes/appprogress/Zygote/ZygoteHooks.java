//  @/work/workcodes/aosp-p9.x-auto-alpha/libcore/dalvik/src/main/java/dalvik/system/ZygoteHooks.java
public final class ZygoteHooks {

    /**
     * Called by the zygote when starting up. It marks the point when any thread
     * start should be an error, as only internal daemon threads are allowed there.
     */
    public static native void startZygoteNoThreadCreation();


    /**
     * Called by the zygote when startup is finished. It marks the point when it is
     * conceivable(可以想象的) that threads would be started again, e.g., restarting daemons.
     */
    public static native void stopZygoteNoThreadCreation();


}



//  @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/native/dalvik_system_ZygoteHooks.cc
static void ZygoteHooks_startZygoteNoThreadCreation(JNIEnv* env ATTRIBUTE_UNUSED,jclass klass ATTRIBUTE_UNUSED) {
  Runtime::Current()->SetZygoteNoThreadSection(true);
}

static void ZygoteHooks_stopZygoteNoThreadCreation(JNIEnv* env ATTRIBUTE_UNUSED,jclass klass ATTRIBUTE_UNUSED) {
  Runtime::Current()->SetZygoteNoThreadSection(false);
}


//  @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/runtime.h
void Runtime::SetZygoteNoThreadSection(bool val) {   // (Section)截面
    zygote_no_threads_ = val;
}


