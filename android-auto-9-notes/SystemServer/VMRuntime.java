//  @/work/workcodes/aosp-p9.x-auto-alpha/libcore/libart/src/main/java/dalvik/system/VMRuntime.java
public final class VMRuntime {
    /**
     * Returns the VM's instruction set.
     */
    public native String vmInstructionSet();
}

//  @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/native/dalvik_system_VMRuntime.cc
static jstring VMRuntime_vmInstructionSet(JNIEnv* env, jobject) {
    //  GetInstructionSet   @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/runtime.h
    //  InstructionSet  @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/arch/instruction_set.h
    InstructionSet isa = Runtime::Current()->GetInstructionSet();  //获取指令集
    const char* isa_string = GetInstructionSetString(isa);  //  @art/runtime/arch/instruction_set.cc
    return env->NewStringUTF(isa_string);
}
