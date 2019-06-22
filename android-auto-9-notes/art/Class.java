
public class Class{
    public static Class<?> forName(String className)throws ClassNotFoundException { 
        return forName(className, true, VMStack.getCallingClassLoader());
    }


    public static Class<?> forName(String name, boolean initialize,ClassLoader loader) throws ClassNotFoundException
    {
        if (loader == null) {
            loader = BootClassLoader.getInstance();  // @   /work/workcodes/aosp-p9.x-auto-ga/libcore/ojluni/src/main/java/java/lang/ClassLoader.java
        }
        Class<?> result;
        try {
            result = classForName(name, initialize, loader);
        } catch (ClassNotFoundException e) {
            Throwable cause = e.getCause();
            if (cause instanceof LinkageError) {
                throw (LinkageError) cause;
            }
            throw e;
        }
        return result;
    }

    /** Called after security checks have been made. */
    @FastNative
    static native Class<?> classForName(String className, boolean shouldInitialize,ClassLoader classLoader) throws ClassNotFoundException;
    
}


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
  if (!IsValidBinaryClassName(name.c_str())) {  // art/libdexfile/dex/descriptors_names.cc
    soa.Self()->ThrowNewExceptionF("Ljava/lang/ClassNotFoundException;","Invalid name: %s", name.c_str());
    return nullptr;
  }

  // DotToDescriptor 将 . 替换成 /
  std::string descriptor(DotToDescriptor(name.c_str()));
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader(hs.NewHandle(soa.Decode<mirror::ClassLoader>(javaLoader)));
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  Handle<mirror::Class> c(hs.NewHandle(class_linker->FindClass(soa.Self(), descriptor.c_str(), class_loader)));
  if (c == nullptr) {
    ScopedLocalRef<jthrowable> cause(env, env->ExceptionOccurred());
    env->ExceptionClear();
    jthrowable cnfe = reinterpret_cast<jthrowable>(
        env->NewObject(WellKnownClasses::java_lang_ClassNotFoundException,
                       WellKnownClasses::java_lang_ClassNotFoundException_init,
                       javaName,
                       cause.get()));
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









//  @   /work/workcodes/aosp-p9.x-auto-ga/art/runtime/native/java_lang_Class.cc
static jobject Class_newInstance(JNIEnv* env, jobject javaThis) {
    ScopedFastNativeObjectAccess soa(env);
    StackHandleScope<4> hs(soa.Self());
    Handle<mirror::Class> klass = hs.NewHandle(DecodeClass(soa, javaThis));
    if (UNLIKELY(klass->GetPrimitiveType() != 0 || klass->IsInterface() || klass->IsArrayClass() || klass->IsAbstract())) {
      soa.Self()->ThrowNewExceptionF("Ljava/lang/InstantiationException;",
                                     "%s cannot be instantiated",
                                     klass->PrettyClass().c_str());
      return nullptr;
    }
    auto caller = hs.NewHandle<mirror::Class>(nullptr);
    // Verify that we can access the class.
    if (!klass->IsPublic()) {
      caller.Assign(GetCallingClass(soa.Self(), 1));
      if (caller != nullptr && !caller->CanAccess(klass.Get())) {
        soa.Self()->ThrowNewExceptionF(
            "Ljava/lang/IllegalAccessException;", "%s is not accessible from %s",
            klass->PrettyClass().c_str(), caller->PrettyClass().c_str());
        return nullptr;
      }
    }
    ArtMethod* constructor = klass->GetDeclaredConstructor(
        soa.Self(),
        ScopedNullHandle<mirror::ObjectArray<mirror::Class>>(),
        kRuntimePointerSize);
    if (UNLIKELY(constructor == nullptr) || ShouldBlockAccessToMember(constructor, soa.Self())) {
      soa.Self()->ThrowNewExceptionF("Ljava/lang/InstantiationException;",
                                     "%s has no zero argument constructor",
                                     klass->PrettyClass().c_str());
      return nullptr;
    }
    // Invoke the string allocator to return an empty string for the string class.
    if (klass->IsStringClass()) {
      gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
      ObjPtr<mirror::Object> obj = mirror::String::AllocEmptyString<true>(soa.Self(), allocator_type);
      if (UNLIKELY(soa.Self()->IsExceptionPending())) {
        return nullptr;
      } else {
        return soa.AddLocalReference<jobject>(obj);
      }
    }
    auto receiver = hs.NewHandle(klass->AllocObject(soa.Self()));
    if (UNLIKELY(receiver == nullptr)) {
      soa.Self()->AssertPendingOOMException();
      return nullptr;
    }
    // Verify that we can access the constructor.
    auto* declaring_class = constructor->GetDeclaringClass();
    if (!constructor->IsPublic()) {
      if (caller == nullptr) {
        caller.Assign(GetCallingClass(soa.Self(), 1));
      }
      if (UNLIKELY(caller != nullptr && !VerifyAccess(receiver.Get(),
                                                            declaring_class,
                                                            constructor->GetAccessFlags(),
                                                            caller.Get()))) {
        soa.Self()->ThrowNewExceptionF(
            "Ljava/lang/IllegalAccessException;", "%s is not accessible from %s",
            constructor->PrettyMethod().c_str(), caller->PrettyClass().c_str());
        return nullptr;
      }
    }
    // Ensure that we are initialized.
    if (UNLIKELY(!declaring_class->IsInitialized())) {
      if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(
          soa.Self(), hs.NewHandle(declaring_class), true, true)) {
        soa.Self()->AssertPendingException();
        return nullptr;
      }
    }
    // Invoke the constructor.
    JValue result;
    uint32_t args[1] = { static_cast<uint32_t>(reinterpret_cast<uintptr_t>(receiver.Get())) };
    constructor->Invoke(soa.Self(), args, sizeof(args), &result, "V");
    if (UNLIKELY(soa.Self()->IsExceptionPending())) {
      return nullptr;
    }
    // Constructors are ()V methods, so we shouldn't touch the result of InvokeMethod.
    return soa.AddLocalReference<jobject>(receiver.Get());
  }