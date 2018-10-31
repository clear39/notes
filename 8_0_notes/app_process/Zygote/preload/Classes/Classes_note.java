//	@libcore/ojluni/src/main/java/java/lang/Class.java

public final class Class<T> implements java.io.Serializable,GenericDeclaration,Type,AnnotatedElement {
	public static Class<?> forName(String className) throws ClassNotFoundException {
        return forName(className, true, VMStack.getCallingClassLoader());
    }

    public static Class<?> forName(String name, boolean initialize,ClassLoader loader) throws ClassNotFoundException {
        if (loader == null) {
            loader = BootClassLoader.getInstance();
        }
        Class<?> result;
        try {
            result = classForName(name, initialize, loader);//	native
        } catch (ClassNotFoundException e) {
            Throwable cause = e.getCause();
            if (cause instanceof LinkageError) {
                throw (LinkageError) cause;
            }
            throw e;
        }
        return result;
    }



}


//	@art/runtime/native/java_lang_Class.cc
// "name" is in "binary name" format, e.g. "dalvik.system.Debug$1".
static jclass Class_classForName(JNIEnv* env, jclass, jstring javaName, jboolean initialize,jobject javaLoader) {
  ScopedFastNativeObjectAccess soa(env);
  ScopedUtfChars name(env, javaName);
  if (name.c_str() == nullptr) {
    return nullptr;
  }

  // We need to validate and convert the name (from x.y.z to x/y/z).  This
  // is especially handy for array types, since we want to avoid
  // auto-generating bogus array classes.
  if (!IsValidBinaryClassName(name.c_str())) {
    soa.Self()->ThrowNewExceptionF("Ljava/lang/ClassNotFoundException;","Invalid name: %s", name.c_str());
    return nullptr;
  }

  std::string descriptor(DotToDescriptor(name.c_str()));
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader(hs.NewHandle(soa.Decode<mirror::ClassLoader>(javaLoader)));
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  Handle<mirror::Class> c(hs.NewHandle(class_linker->FindClass(soa.Self(), descriptor.c_str(), class_loader)));
  if (c == nullptr) {
    ScopedLocalRef<jthrowable> cause(env, env->ExceptionOccurred());
    env->ExceptionClear();
    jthrowable cnfe = reinterpret_cast<jthrowable>(env->NewObject(WellKnownClasses::java_lang_ClassNotFoundException,
                       WellKnownClasses::java_lang_ClassNotFoundException_init,
                       javaName,cause.get()));
    if (cnfe != nullptr) {
      // Make sure allocation didn't fail with an OOME.
      env->Throw(cnfe);
    }
    return nullptr;
  }
  if (initialize) {
    class_linker->EnsureInitialized(soa.Self(), c, true, true);
  }
  return soa.AddLocalReference<jclass>(c.Get());
}



//	@art/runtime/native/scoped_fast_native_object_access-inl.h
inline ScopedFastNativeObjectAccess::ScopedFastNativeObjectAccess(JNIEnv* env)
    : ScopedObjectAccessAlreadyRunnable(env) {
  Locks::mutator_lock_->AssertSharedHeld(Self());
  DCHECK((*Self()->GetManagedStack()->GetTopQuickFrame())->IsAnnotatedWithFastNative());
  // Don't work with raw objects in non-runnable states.
  DCHECK_EQ(Self()->GetState(), kRunnable);
}


//	@art/runtime/scoped_thread_state_change.cc

