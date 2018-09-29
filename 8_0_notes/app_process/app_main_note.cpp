//  frameworks/base/cmds/app_process/app_main.cpp

static size_t computeArgBlockSize(int argc, char* const argv[]) {
    // TODO: This assumes that all arguments are allocated in
    // contiguous memory. There isn't any documented guarantee
    // that this is the case, but this is how the kernel does it
    // (see fs/exec.c).
    //
    // Also note that this is a constant for "normal" android apps.
    // Since they're forked from zygote, the size of their command line
    // is the size of the zygote command line.
    //
    // We change the process name of the process by over-writing
    // the start of the argument block (argv[0]) with the new name of
    // the process, so we'd mysteriously start getting truncated process
    // names if the zygote command line decreases in size.
    uintptr_t start = reinterpret_cast<uintptr_t>(argv[0]);
    uintptr_t end = reinterpret_cast<uintptr_t>(argv[argc - 1]);
    end += strlen(argv[argc - 1]) + 1;
    return (end - start);
}


int main(int argc, char* const argv[])
{
    /**
    *打印启动参数
    */
    if (!LOG_NDEBUG) {
      String8 argv_String;
      for (int i = 0; i < argc; ++i) {
        argv_String.append("\"");
        argv_String.append(argv[i]);
        argv_String.append("\" ");
      }
      ALOGV("app_process main with argv: %s", argv_String.string());
    }


    //computeArgBlockSize计算一下argv[] 总共大小
    AppRuntime runtime(argv[0], computeArgBlockSize(argc, argv));
    // Process command line arguments
    // ignore argv[0]
    argc--;
    argv++;

    // Everything up to '--' or first non '-' arg goes to the vm.
    //
    // The first argument after the VM args is the "parent dir", which
    // is currently unused.
    //
    // After the parent dir, we expect one or more the following internal
    // arguments :
    //
    // --zygote : Start in zygote mode
    // --start-system-server : Start the system server.
    // --application : Start in application (stand alone, non zygote) mode.
    // --nice-name : The nice name for this process.
    //
    // For non zygote starts, these arguments will be followed by
    // the main class name. All remaining arguments are passed to
    // the main method of this class.
    //
    // For zygote starts, all remaining arguments are passed to the zygote.
    // main function.
    //
    // Note that we must copy argument string values since we will rewrite the
    // entire argument block when we apply the nice name to argv0.
    //
    // As an exception to the above rule, anything in "spaced commands"
    // goes to the vm even though it has a space in it.
    const char* spaced_commands[] = { "-cp", "-classpath" };
    // Allow "spaced commands" to be succeeded by exactly 1 argument (regardless of -s).
    bool known_command = false;

    int i;
    for (i = 0; i < argc; i++) {
        if (known_command == true) {
          runtime.addOption(strdup(argv[i]));
          ALOGV("app_process main add known option '%s'", argv[i]);
          known_command = false;
          continue;
        }

        for (int j = 0; j < static_cast<int>(sizeof(spaced_commands) / sizeof(spaced_commands[0])); ++j) {
          if (strcmp(argv[i], spaced_commands[j]) == 0) {
            known_command = true;
            ALOGV("app_process main found known command '%s'", argv[i]);
          }
        }

        if (argv[i][0] != '-') {
            break;
        }
        if (argv[i][1] == '-' && argv[i][2] == 0) {
            ++i; // Skip --.
            break;
        }

        runtime.addOption(strdup(argv[i]));
        ALOGV("app_process main add option '%s'", argv[i]);
    }

    // Parse runtime arguments.  Stop at first unrecognized option.
    bool zygote = false;
    bool startSystemServer = false;
    bool application = false;
    String8 niceName;
    String8 className;

    ++i;  // Skip unused "parent dir" argument.
    while (i < argc) {
        const char* arg = argv[i++];
        if (strcmp(arg, "--zygote") == 0) {
            zygote = true;
            niceName = ZYGOTE_NICE_NAME;
        } else if (strcmp(arg, "--start-system-server") == 0) {
            startSystemServer = true;
        } else if (strcmp(arg, "--application") == 0) {
            application = true;
        } else if (strncmp(arg, "--nice-name=", 12) == 0) {
            niceName.setTo(arg + 12);
        } else if (strncmp(arg, "--", 2) != 0) {
            className.setTo(arg);
            break;
        } else {
            --i;
            break;
        }
    }

    // 注意这里只要zygote把argv参数加入到args中，应用APK没有，而是保存到AppRuntime中
    Vector<String8> args;
    //如果className不为空
    if (!className.isEmpty()) {//普通应用
        // We're not in zygote mode, the only argument we need to pass
        // to RuntimeInit is the application argument.
        //
        // The Remainder of args get passed to startup class main(). Make
        // copies of them before we overwrite them with the process name.
        args.add(application ? String8("application") : String8("tool"));
        //注意这里 只有应用APK 将参数设置到AppRuntime的集合中
        // 如果application为true，则className为android.app.ActivityThread
        runtime.setClassNameAndArgs(className, argc - i, argv + i);//设置AppRuntime中的 String8 mClassName成员以及其他参数

        if (!LOG_NDEBUG) {
          String8 restOfArgs;
          char* const* argv_new = argv + i;
          int argc_new = argc - i;
          for (int k = 0; k < argc_new; ++k) {
            restOfArgs.append("\"");
            restOfArgs.append(argv_new[k]);
            restOfArgs.append("\" ");
          }
          //打印启动类名，以及启动参数
          ALOGV("Class name = %s, args = %s", className.string(), restOfArgs.string());
        }
    } else {// zygote 和 systemserver 程序启动
        // We're in zygote mode.
        maybeCreateDalvikCache(); //创建"/data/dalvik-cache/arm"目录

        if (startSystemServer) {
            args.add(String8("start-system-server"));
        }

        char prop[PROP_VALUE_MAX];
        //static const char ABI_LIST_PROPERTY[] = "ro.product.cpu.abilist32"    /   "ro.product.cpu.abilist64"
        if (property_get(ABI_LIST_PROPERTY, prop, NULL) == 0) {
            LOG_ALWAYS_FATAL("app_process: Unable to determine ABI list from property %s.",ABI_LIST_PROPERTY);
            return 11;
        }

        String8 abiFlag("--abi-list=");
        abiFlag.append(prop);
        args.add(abiFlag);//    添加--abi-list=

        // In zygote mode, pass all remaining arguments to the zygote
        // main() method.
        for (; i < argc; ++i) {
            args.add(String8(argv[i]));
        }
    }

    if (!niceName.isEmpty()) {
        runtime.setArgv0(niceName.string(), true /* setProcName */);
    }

    if (zygote) {
        runtime.start("com.android.internal.os.ZygoteInit", args, zygote);
    } else if (className) {
        runtime.start("com.android.internal.os.RuntimeInit", args, zygote);
    } else {
        fprintf(stderr, "Error: no class name or --zygote supplied.\n");
        app_usage();
        LOG_ALWAYS_FATAL("app_process: no class name or --zygote supplied.");
    }
}


static void maybeCreateDalvikCache() {
#if defined(__aarch64__)
    static const char kInstructionSet[] = "arm64";
#elif defined(__x86_64__)
    static const char kInstructionSet[] = "x86_64";
#elif defined(__arm__)
    static const char kInstructionSet[] = "arm";
#elif defined(__i386__)
    static const char kInstructionSet[] = "x86";
#elif defined (__mips__) && !defined(__LP64__)
    static const char kInstructionSet[] = "mips";
#elif defined (__mips__) && defined(__LP64__)
    static const char kInstructionSet[] = "mips64";
#else
#error "Unknown instruction set"
#endif

    const char* androidRoot = getenv("ANDROID_DATA");// androidRoot == /data
    LOG_ALWAYS_FATAL_IF(androidRoot == NULL, "ANDROID_DATA environment variable unset");

    char dalvikCacheDir[PATH_MAX]; 
    // dalvikCacheDir == "/data/dalvik-cache/arm"目录
    const int numChars = snprintf(dalvikCacheDir, PATH_MAX,"%s/dalvik-cache/%s", androidRoot, kInstructionSet);
    LOG_ALWAYS_FATAL_IF((numChars >= PATH_MAX || numChars < 0),"Error constructing dalvik cache : %s", strerror(errno));

    int result = mkdir(dalvikCacheDir, 0711);//创建"/data/dalvik-cache/arm"目录
    LOG_ALWAYS_FATAL_IF((result < 0 && errno != EEXIST),"Error creating cache dir %s : %s", dalvikCacheDir, strerror(errno));

    // We always perform these steps because the directory might
    // already exist, with wider permissions and a different owner
    // than we'd like.
    result = chown(dalvikCacheDir, AID_ROOT, AID_ROOT);//可能目录已经存在 int chown(const char *path, uid_t owner, gid_t group);
    LOG_ALWAYS_FATAL_IF((result < 0), "Error changing dalvik-cache ownership : %s", strerror(errno));

    result = chmod(dalvikCacheDir, 0711);
    LOG_ALWAYS_FATAL_IF((result < 0),"Error changing dalvik-cache permissions : %s", strerror(errno));
}



