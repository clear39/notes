// @/work/workcodes/aosp-p9.x-auto-alpha/libcore/dalvik/src/main/java/dalvik/system/DexFile.java
public final class DexFile {
    /**
     * Calls {@link #getDexOptNeeded(String, String, String, String, String, boolean, boolean)}
     * with a null class loader context.
     *
     * TODO(ngeoffray, calin): deprecate / remove.
     * @hide
     */
    public static int getDexOptNeeded(String fileName,String instructionSet, String compilerFilter, boolean newProfile, boolean downgrade)
        throws FileNotFoundException, IOException {
            return getDexOptNeeded(fileName, instructionSet, compilerFilter, null, newProfile, downgrade);
    }

    /**
     * Returns the VM's opinion of what kind of dexopt is needed to make the
     * apk/jar file up to date, where {@code targetMode} is used to indicate what
     * type of compilation the caller considers up-to-date, and {@code newProfile}
     * is used to indicate whether profile information has changed recently.
     *
     * @param fileName the absolute path to the apk/jar file to examine.
     * @param compilerFilter a compiler filter to use for what a caller considers up-to-date.
     * @param classLoaderContext a string encoding the class loader context the dex file
     *        is intended to have at runtime.
     * @param newProfile flag that describes whether a profile corresponding
     *        to the dex file has been recently updated and should be considered
     *        in the state of the file.
     * @param downgrade flag that describes if the purpose of dexopt is to downgrade the
     *        compiler filter. If set to false, will be evaluated as an upgrade request.
     * @return NO_DEXOPT_NEEDED, or DEX2OAT_*. See documentation
     *         of the particular status code for more information on its
     *         meaning. Returns a positive status code if the status refers to
     *         the oat file in the oat location. Returns a negative status
     *         code if the status refers to the oat file in the odex location.
     * @throws java.io.FileNotFoundException if fileName is not readable,
     *         not a file, or not present.
     * @throws java.io.IOException if fileName is not a valid apk/jar file or
     *         if problems occur while parsing it.
     * @throws java.lang.NullPointerException if fileName is null.
     *
     * @hide
     * @dalvik_system_DexFile.cc
     */
    public static native int getDexOptNeeded(String fileName,
            String instructionSet, String compilerFilter, String classLoaderContext,
            boolean newProfile, boolean downgrade)throws FileNotFoundException, IOException;
}


//  @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/native/dalvik_system_DexFile.cc
static jint DexFile_getDexOptNeeded(JNIEnv* env,
                                    jclass,
                                    jstring javaFilename,
                                    jstring javaInstructionSet,
                                    jstring javaTargetCompilerFilter,
                                    jstring javaClassLoaderContext,
                                    jboolean newProfile,
                                    jboolean downgrade) {
  ScopedUtfChars filename(env, javaFilename);
  if (env->ExceptionCheck()) {
    return -1;
  }

  ScopedUtfChars instruction_set(env, javaInstructionSet);// arm64 
  if (env->ExceptionCheck()) {
    return -1;
  }

  ScopedUtfChars target_compiler_filter(env, javaTargetCompilerFilter);//   speed
  if (env->ExceptionCheck()) {
    return -1;
  }

  //    @art/runtime/native/dalvik_system_DexFile.cc
  NullableScopedUtfChars class_loader_context(env, javaClassLoaderContext); 
  if (env->ExceptionCheck()) {
    return -1;
  }

  return GetDexOptNeeded(env,
                         filename.c_str(),
                         instruction_set.c_str(),
                         target_compiler_filter.c_str(),
                         class_loader_context.c_str(),// null
                         newProfile == JNI_TRUE, //false
                         downgrade == JNI_TRUE /**false */);
}

static jint GetDexOptNeeded(JNIEnv* env,
                            const char* filename,
                            const char* instruction_set,
                            const char* compiler_filter_name,
                            const char* class_loader_context,// null
                            bool profile_changed,
                            bool downgrade) {
  if ((filename == nullptr) || !OS::FileExists(filename)) {
    LOG(ERROR) << "DexFile_getDexOptNeeded file '" << filename << "' does not exist";
    ScopedLocalRef<jclass> fnfe(env, env->FindClass("java/io/FileNotFoundException"));
    const char* message = (filename == nullptr) ? "<empty file name>" : filename;
    env->ThrowNew(fnfe.get(), message);
    return -1;
  }

  const InstructionSet target_instruction_set = GetInstructionSetFromString(instruction_set);
  if (target_instruction_set == InstructionSet::kNone) {
    ScopedLocalRef<jclass> iae(env, env->FindClass("java/lang/IllegalArgumentException"));
    std::string message(StringPrintf("Instruction set %s is invalid.", instruction_set));
    env->ThrowNew(iae.get(), message.c_str());
    return -1;
  }

  //    @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/compiler_filter.h
  CompilerFilter::Filter filter;
  /** 根据 compiler_filter_name 字符串匹配获取 对应 枚举类型 */
  if (!CompilerFilter::ParseCompilerFilter(compiler_filter_name, &filter)) { //"speed"
    ScopedLocalRef<jclass> iae(env, env->FindClass("java/lang/IllegalArgumentException"));
    std::string message(StringPrintf("Compiler filter %s is invalid.", compiler_filter_name));
    env->ThrowNew(iae.get(), message.c_str());
    return -1;
  }

  std::unique_ptr<ClassLoaderContext> context = nullptr;
  if (class_loader_context != nullptr) {

    //  @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/class_loader_context.cc
    context = ClassLoaderContext::Create(class_loader_context);

    if (context == nullptr) {
      ScopedLocalRef<jclass> iae(env, env->FindClass("java/lang/IllegalArgumentException"));
      std::string message(StringPrintf("Class loader context '%s' is invalid.",class_loader_context));
      env->ThrowNew(iae.get(), message.c_str());
      return -1;
    }

  }

  // TODO: Verify the dex location is well formed, and throw an IOException if not? //  验证DEX位置是否正确

  //    @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/oat_file_assistant.cc
  OatFileAssistant oat_file_assistant(filename, target_instruction_set, false);

  // Always treat elements of the bootclasspath as up-to-date.
  if (oat_file_assistant.IsInBootClassPath()) {  //IsInBootClassPath 用于检测当前路径jar是否已经在BootClassPath中
    return OatFileAssistant::kNoDexOptNeeded;
  }

  return oat_file_assistant.GetDexOptNeeded(filter,profile_changed,downgrade,context.get());
}


                     
