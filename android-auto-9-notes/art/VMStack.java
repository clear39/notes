public final class VMStack {
    native public static ClassLoader getCallingClassLoader();
}



//  @   /work/workcodes/aosp-p9.x-auto-ga/art/runtime/native/dalvik_system_VMStack.cc
// Returns the defining class loader of the caller's caller.
static jobject VMStack_getCallingClassLoader(JNIEnv* env, jclass) {
    ScopedFastNativeObjectAccess soa(env);
    NthCallerVisitor visitor(soa.Self(), 2);
    visitor.WalkStack();
    if (UNLIKELY(visitor.caller == nullptr)) {
      // The caller is an attached native thread.
      return nullptr;
    }
    return soa.AddLocalReference<jobject>(visitor.caller->GetDeclaringClass()->GetClassLoader());
  }
