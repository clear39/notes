/**
 * Provides hooks for the zygote to call back into the runtime to perform
 * parent or child specific initialization..
 *
 * @hide
 */
public final class ZygoteHooks {
     /**
     * Called by the zygote prior to every fork. Each call to {@code preFork}
     * is followed by a matching call to {@link #postForkChild(int, String)} on the child
     * process and {@link #postForkCommon()} on both the parent and the child
     * process. {@code postForkCommon} is called after {@code postForkChild} in
     * the child process.
     */
    public void preFork() {
        Daemons.stop();
        waitUntilAllThreadsStopped();
        token = nativePreFork();
    }

    /**
     * We must not fork until we're single-threaded again. Wait until /proc shows we're
     * down to just one thread.
     */
    private static void waitUntilAllThreadsStopped() {
        File tasks = new File("/proc/self/task");
        // All Java daemons are stopped already. We're just waiting for their OS counterparts to
        // finish as well. This shouldn't take much time so spinning is ok here.
        while (tasks.list().length > 1) {
          Thread.yield();
        }
    }


    private static native long nativePreFork();


     private static native void nativePostForkChild(long token, int debugFlags,boolean isSystemServer, String instructionSet);



    /**
     * Called by the zygote in both the parent and child processes after
     * every fork. In the child process, this method is called after
     * {@code postForkChild}.
     */
    public void postForkCommon() {
        Daemons.startPostZygoteFork();
    }
}


//  art/runtime/native/dalvik_system_ZygoteHooks.cc

static jlong ZygoteHooks_nativePreFork(JNIEnv* env, jclass) {
  Runtime* runtime = Runtime::Current();
  CHECK(runtime->IsZygote()) << "runtime instance not started with -Xzygote";

  runtime->PreZygoteFork();

  if (Trace::GetMethodTracingMode() != TracingMode::kTracingInactive) {
    // Tracing active, pause it.
    Trace::Pause();
  }

  // Grab thread before fork potentially makes Thread::pthread_key_self_ unusable.
  return reinterpret_cast<jlong>(ThreadForEnv(env));
}




static void ZygoteHooks_nativePostForkChild(JNIEnv* env,jclass, jlong token,jint debug_flags,jboolean is_system_server,jstring instruction_set) {
  Thread* thread = reinterpret_cast<Thread*>(token);
  // Our system thread ID, etc, has changed so reset Thread state.
  thread->InitAfterFork();
  EnableDebugFeatures(debug_flags);

  // Update tracing.
  if (Trace::GetMethodTracingMode() != TracingMode::kTracingInactive) {
    Trace::TraceOutputMode output_mode = Trace::GetOutputMode();
    Trace::TraceMode trace_mode = Trace::GetMode();
    size_t buffer_size = Trace::GetBufferSize();

    // Just drop it.
    Trace::Abort();

    // Only restart if it was streaming mode.
    // TODO: Expose buffer size, so we can also do file mode.
    if (output_mode == Trace::TraceOutputMode::kStreaming) {
      static constexpr size_t kMaxProcessNameLength = 100;
      char name_buf[kMaxProcessNameLength] = {};
      int rc = pthread_getname_np(pthread_self(), name_buf, kMaxProcessNameLength);
      std::string proc_name;

      if (rc == 0) {
          // On success use the pthread name.
          proc_name = name_buf;
      }

      if (proc_name.empty() || proc_name == "zygote" || proc_name == "zygote64") {
        // Either no process name, or the name hasn't been changed, yet. Just use pid.
        pid_t pid = getpid();
        proc_name = StringPrintf("%u", static_cast<uint32_t>(pid));
      }

      std::string trace_file = StringPrintf("/data/misc/trace/%s.trace.bin", proc_name.c_str());
      Trace::Start(trace_file.c_str(),
                   -1,
                   buffer_size,
                   0,   // TODO: Expose flags.
                   output_mode,trace_mode,0);  // TODO: Expose interval.
      if (thread->IsExceptionPending()) {
        ScopedObjectAccess soa(env);
        thread->ClearException();
      }
    }
  }

  if (instruction_set != nullptr && !is_system_server) {
    ScopedUtfChars isa_string(env, instruction_set);
    InstructionSet isa = GetInstructionSetFromString(isa_string.c_str());
    Runtime::NativeBridgeAction action = Runtime::NativeBridgeAction::kUnload;
    if (isa != kNone && isa != kRuntimeISA) {
      action = Runtime::NativeBridgeAction::kInitialize;
    }
    Runtime::Current()->InitNonZygoteOrPostFork(env, is_system_server, action, isa_string.c_str());
  } else {
    Runtime::Current()->InitNonZygoteOrPostFork(env, is_system_server, Runtime::NativeBridgeAction::kUnload, nullptr);
  }
}   