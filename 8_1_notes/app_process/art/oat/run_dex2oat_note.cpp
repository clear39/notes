//	@frameworks/base/services/core/java/com/android/server/pm/PackageDexOptimizer.java
public class PackageDexOptimizer {


	//@frameworks/base/services/core/java/com/android/server/pm/PackageDexOptimizer.java:125
	/**
     * Performs dexopt on all code paths and libraries of the specified package for specified
     * instruction sets.
     *
     * <p>Calls to {@link com.android.server.pm.Installer#dexopt} on {@link #mInstaller} are
     * synchronized on {@link #mInstallLock}.
     */
    int performDexOpt(PackageParser.Package pkg, String[] sharedLibraries,
            String[] instructionSets, CompilerStats.PackageStats packageStats,
            PackageDexUsage.PackageUseInfo packageUseInfo, DexoptOptions options) {
        if (!canOptimizePackage(pkg)) {
            return DEX_OPT_SKIPPED;
        }
        synchronized (mInstallLock) {
            final long acquireTime = acquireWakeLockLI(pkg.applicationInfo.uid);
            try {
                return performDexOptLI(pkg, sharedLibraries, instructionSets,
                        packageStats, packageUseInfo, options);
            } finally {
                releaseWakeLockLI(acquireTime);
            }
        }
    }


	//@frameworks/base/services/core/java/com/android/server/pm/PackageDexOptimizer.java:203
	/**
     * Performs dexopt on all code paths of the given package.
     * It assumes the install lock is held.
     */
    @GuardedBy("mInstallLock")
    private int performDexOptLI(PackageParser.Package pkg, String[] sharedLibraries,
            String[] targetInstructionSets, CompilerStats.PackageStats packageStats,
            PackageDexUsage.PackageUseInfo packageUseInfo, DexoptOptions options) {
        final String[] instructionSets = targetInstructionSets != null ?
                targetInstructionSets : getAppDexInstructionSets(pkg.applicationInfo);
        final String[] dexCodeInstructionSets = getDexCodeInstructionSets(instructionSets);
        final List<String> paths = pkg.getAllCodePaths();
        final int sharedGid = UserHandle.getSharedAppGid(pkg.applicationInfo.uid);

        // Get the class loader context dependencies.
        // For each code path in the package, this array contains the class loader context that
        // needs to be passed to dexopt in order to ensure correct optimizations.
        boolean[] pathsWithCode = new boolean[paths.size()];
        pathsWithCode[0] = (pkg.applicationInfo.flags & ApplicationInfo.FLAG_HAS_CODE) != 0;
        for (int i = 1; i < paths.size(); i++) {
            pathsWithCode[i] = (pkg.splitFlags[i - 1] & ApplicationInfo.FLAG_HAS_CODE) != 0;
        }
        String[] classLoaderContexts = DexoptUtils.getClassLoaderContexts(
                pkg.applicationInfo, sharedLibraries, pathsWithCode);

        // Sanity check that we do not call dexopt with inconsistent data.
        if (paths.size() != classLoaderContexts.length) {
            String[] splitCodePaths = pkg.applicationInfo.getSplitCodePaths();
            throw new IllegalStateException("Inconsistent information "
                + "between PackageParser.Package and its ApplicationInfo. "
                + "pkg.getAllCodePaths=" + paths
                + " pkg.applicationInfo.getBaseCodePath=" + pkg.applicationInfo.getBaseCodePath()
                + " pkg.applicationInfo.getSplitCodePaths="
                + (splitCodePaths == null ? "null" : Arrays.toString(splitCodePaths)));
        }

        int result = DEX_OPT_SKIPPED;
        for (int i = 0; i < paths.size(); i++) {
            // Skip paths that have no code.
            if (!pathsWithCode[i]) {
                continue;
            }
            if (classLoaderContexts[i] == null) {
                throw new IllegalStateException("Inconsistent information in the "
                        + "package structure. A split is marked to contain code "
                        + "but has no dependency listed. Index=" + i + " path=" + paths.get(i));
            }

            // Append shared libraries with split dependencies for this split.
            String path = paths.get(i);
            if (options.getSplitName() != null) {
                // We are asked to compile only a specific split. Check that the current path is
                // what we are looking for.
                if (!options.getSplitName().equals(new File(path).getName())) {
                    continue;
                }
            }

            final boolean isUsedByOtherApps = options.isDexoptAsSharedLibrary()
                    || packageUseInfo.isUsedByOtherApps(path);
            final String compilerFilter = getRealCompilerFilter(pkg.applicationInfo,
                options.getCompilerFilter(), isUsedByOtherApps);
            final boolean profileUpdated = options.isCheckForProfileUpdates() &&
                isProfileUpdated(pkg, sharedGid, compilerFilter);

            // Get the dexopt flags after getRealCompilerFilter to make sure we get the correct
            // flags.
            final int dexoptFlags = getDexFlags(pkg, compilerFilter, options.isBootComplete());

            for (String dexCodeIsa : dexCodeInstructionSets) {
                int newResult = dexOptPath(pkg, path, dexCodeIsa, compilerFilter,
                        profileUpdated, classLoaderContexts[i], dexoptFlags, sharedGid,
                        packageStats, options.isDowngrade());
                // The end result is:
                //  - FAILED if any path failed,
                //  - PERFORMED if at least one path needed compilation,
                //  - SKIPPED when all paths are up to date
                if ((result != DEX_OPT_FAILED) && (newResult != DEX_OPT_SKIPPED)) {
                    result = newResult;
                }
            }
        }
        return result;
    }




	//	@frameworks/base/services/core/java/com/android/server/pm/PackageDexOptimizer.java:252
	/**
     * Performs dexopt on the {@code path} belonging to the package {@code pkg}.
     *
     * @return
     *      DEX_OPT_FAILED if there was any exception during dexopt
     *      DEX_OPT_PERFORMED if dexopt was performed successfully on the given path.
     *      DEX_OPT_SKIPPED if the path does not need to be deopt-ed.
     */
    @GuardedBy("mInstallLock")
    private int dexOptPath(PackageParser.Package pkg, String path, String isa,
            String compilerFilter, boolean profileUpdated, String sharedLibrariesPath,
            int dexoptFlags, int uid, CompilerStats.PackageStats packageStats, boolean downgrade) {
        int dexoptNeeded = getDexoptNeeded(path, isa, compilerFilter, profileUpdated, downgrade);
        if (Math.abs(dexoptNeeded) == DexFile.NO_DEXOPT_NEEDED) {
            return DEX_OPT_SKIPPED;
        }

        // TODO(calin): there's no need to try to create the oat dir over and over again,
        //              especially since it involve an extra installd call. We should create
        //              if (if supported) on the fly during the dexopt call.
        String oatDir = createOatDirIfSupported(pkg, isa);

        Log.i(TAG, "Running dexopt (dexoptNeeded=" + dexoptNeeded + ") on: " + path
                + " pkg=" + pkg.applicationInfo.packageName + " isa=" + isa
                + " dexoptFlags=" + printDexoptFlags(dexoptFlags)
                + " target-filter=" + compilerFilter + " oatDir=" + oatDir
                + " sharedLibraries=" + sharedLibrariesPath);

        try {
            long startTime = System.currentTimeMillis();

            // TODO: Consider adding 2 different APIs for primary and secondary dexopt.
            // installd only uses downgrade flag for secondary dex files and ignores it for
            // primary dex files.
            mInstaller.dexopt(path, uid, pkg.packageName, isa, dexoptNeeded, oatDir, dexoptFlags,
                    compilerFilter, pkg.volumeUuid, sharedLibrariesPath, pkg.applicationInfo.seInfo,
                    false /* downgrade*/);

            if (packageStats != null) {
                long endTime = System.currentTimeMillis();
                packageStats.setCompileTime(path, (int)(endTime - startTime));
            }
            return DEX_OPT_PERFORMED;
        } catch (InstallerException e) {
            Slog.w(TAG, "Failed to dexopt", e);
            return DEX_OPT_FAILED;
        }
    }
}

//	@frameworks/base/services/core/java/com/android/server/pm/Installer.java
public class Installer extends SystemService {
	private volatile IInstalld mInstalld;//	@frameworks/native/cmds/installd/binder/android/os/IInstalld.aidl:
	public void dexopt(String apkPath, int uid, @Nullable String pkgName, String instructionSet,
            int dexoptNeeded, @Nullable String outputPath, int dexFlags,
            String compilerFilter, @Nullable String volumeUuid, @Nullable String sharedLibraries,
            @Nullable String seInfo, boolean downgrade)
            throws InstallerException {
        assertValidInstructionSet(instructionSet);
        if (!checkBeforeRemote()) return;
        try {
            mInstalld.dexopt(apkPath, uid, pkgName, instructionSet, dexoptNeeded, outputPath,dexFlags, compilerFilter, volumeUuid, sharedLibraries, seInfo, downgrade);
        } catch (Exception e) {
            throw InstallerException.from(e);
        }
    }
}


//	@frameworks/native/cmds/installd/InstalldNativeService.cpp
binder::Status InstalldNativeService::dexopt(const std::string& apkPath, int32_t uid,
        const std::unique_ptr<std::string>& packageName, const std::string& instructionSet,
        int32_t dexoptNeeded, const std::unique_ptr<std::string>& outputPath, int32_t dexFlags,
        const std::string& compilerFilter, const std::unique_ptr<std::string>& uuid,
        const std::unique_ptr<std::string>& classLoaderContext,
        const std::unique_ptr<std::string>& seInfo, bool downgrade) {
    ENFORCE_UID(AID_SYSTEM);
    CHECK_ARGUMENT_UUID(uuid);
    if (packageName && *packageName != "*") {
        CHECK_ARGUMENT_PACKAGE_NAME(*packageName);
    }
    std::lock_guard<std::recursive_mutex> lock(mLock);

    const char* apk_path = apkPath.c_str();
    const char* pkgname = packageName ? packageName->c_str() : "*";
    const char* instruction_set = instructionSet.c_str();
    const char* oat_dir = outputPath ? outputPath->c_str() : nullptr;
    const char* compiler_filter = compilerFilter.c_str();
    const char* volume_uuid = uuid ? uuid->c_str() : nullptr;
    const char* class_loader_context = classLoaderContext ? classLoaderContext->c_str() : nullptr;
    const char* se_info = seInfo ? seInfo->c_str() : nullptr;
    int res = android::installd::dexopt(apk_path, uid, pkgname, instruction_set, dexoptNeeded,
            oat_dir, dexFlags, compiler_filter, volume_uuid, class_loader_context, se_info,
            downgrade);
    return res ? error(res, "Failed to dexopt") : ok();
}




//	@frameworks/native/cmds/installd/dexopt.cpp
int dexopt(const char* dex_path, uid_t uid, const char* pkgname, const char* instruction_set,
        int dexopt_needed, const char* oat_dir, int dexopt_flags, const char* compiler_filter,
        const char* volume_uuid, const char* class_loader_context, const char* se_info,
        bool downgrade) {
    CHECK(pkgname != nullptr);
    CHECK(pkgname[0] != 0);
    if ((dexopt_flags & ~DEXOPT_MASK) != 0) {
        LOG_FATAL("dexopt flags contains unknown fields\n");
    }

    if (!validate_dex_path_size(dex_path)) {
        return -1;
    }

    if (class_loader_context != nullptr && strlen(class_loader_context) > PKG_PATH_MAX) {
        LOG(ERROR) << "Class loader context exceeds the allowed size: " << class_loader_context;
        return -1;
    }

    bool is_public = (dexopt_flags & DEXOPT_PUBLIC) != 0;
    bool debuggable = (dexopt_flags & DEXOPT_DEBUGGABLE) != 0;
    bool boot_complete = (dexopt_flags & DEXOPT_BOOTCOMPLETE) != 0;
    bool profile_guided = (dexopt_flags & DEXOPT_PROFILE_GUIDED) != 0;
    bool is_secondary_dex = (dexopt_flags & DEXOPT_SECONDARY_DEX) != 0;

    // Check if we're dealing with a secondary dex file and if we need to compile it.
    std::string oat_dir_str;
    std::string dex_real_path;
    if (is_secondary_dex) {
        if (process_secondary_dex_dexopt(dex_path, pkgname, dexopt_flags, volume_uuid, uid,
                instruction_set, compiler_filter, &is_public, &dexopt_needed, &oat_dir_str,
                &dex_real_path,
                downgrade)) {
            oat_dir = oat_dir_str.c_str();
            dex_path = dex_real_path.c_str();
            if (dexopt_needed == NO_DEXOPT_NEEDED) {
                return 0;  // Nothing to do, report success.
            }
        } else {
            return -1;  // We had an error, logged in the process method.
        }
    } else {
        // Currently these flags are only use for secondary dex files.
        // Verify that they are not set for primary apks.
        CHECK((dexopt_flags & DEXOPT_STORAGE_CE) == 0);
        CHECK((dexopt_flags & DEXOPT_STORAGE_DE) == 0);
    }

    // Open the input file.
    unique_fd input_fd(open(dex_path, O_RDONLY, 0));
    if (input_fd.get() < 0) {
        ALOGE("installd cannot open '%s' for input during dexopt\n", dex_path);
        return -1;
    }

    // Create the output OAT file.
    char out_oat_path[PKG_PATH_MAX];
    Dex2oatFileWrapper out_oat_fd = open_oat_out_file(dex_path, oat_dir, is_public, uid,
            instruction_set, is_secondary_dex, out_oat_path);
    if (out_oat_fd.get() < 0) {
        return -1;
    }

    // Open vdex files.
    Dex2oatFileWrapper in_vdex_fd;
    Dex2oatFileWrapper out_vdex_fd;
    if (!open_vdex_files(dex_path, out_oat_path, dexopt_needed, instruction_set, is_public, uid,
            is_secondary_dex, profile_guided, &in_vdex_fd, &out_vdex_fd)) {
        return -1;
    }

    // Ensure that the oat dir and the compiler artifacts of secondary dex files have the correct
    // selinux context (we generate them on the fly during the dexopt invocation and they don't
    // fully inherit their parent context).
    // Note that for primary apk the oat files are created before, in a separate installd
    // call which also does the restorecon. TODO(calin): unify the paths.
    if (is_secondary_dex) {
        if (selinux_android_restorecon_pkgdir(oat_dir, se_info, uid,
                SELINUX_ANDROID_RESTORECON_RECURSE)) {
            LOG(ERROR) << "Failed to restorecon " << oat_dir;
            return -1;
        }
    }

    // Create a swap file if necessary.
    unique_fd swap_fd = maybe_open_dexopt_swap_file(out_oat_path);

    // Create the app image file if needed.
    Dex2oatFileWrapper image_fd =
            maybe_open_app_image(out_oat_path, profile_guided, is_public, uid, is_secondary_dex);

    // Open the reference profile if needed.
    Dex2oatFileWrapper reference_profile_fd = maybe_open_reference_profile(
            pkgname, dex_path, profile_guided, is_public, uid, is_secondary_dex);

    ALOGV("DexInv: --- BEGIN '%s' ---\n", dex_path);

    pid_t pid = fork();
    if (pid == 0) {
        /* child -- drop privileges before continuing */
        drop_capabilities(uid);

        SetDex2OatScheduling(boot_complete);
        if (flock(out_oat_fd.get(), LOCK_EX | LOCK_NB) != 0) {
            ALOGE("flock(%s) failed: %s\n", out_oat_path, strerror(errno));
            _exit(67);
        }

        run_dex2oat(input_fd.get(),
                    out_oat_fd.get(),
                    in_vdex_fd.get(),
                    out_vdex_fd.get(),
                    image_fd.get(),
                    dex_path,
                    out_oat_path,
                    swap_fd.get(),
                    instruction_set,
                    compiler_filter,
                    debuggable,
                    boot_complete,
                    reference_profile_fd.get(),
                    class_loader_context);
        _exit(68);   /* only get here on exec failure */
    } else {
        int res = wait_child(pid);
        if (res == 0) {
            ALOGV("DexInv: --- END '%s' (success) ---\n", dex_path);
        } else {
            ALOGE("DexInv: --- END '%s' --- status=0x%04x, process failed\n", dex_path, res);
            return res;
        }
    }

    update_out_oat_access_times(dex_path, out_oat_path);

    // We've been successful, don't delete output.
    out_oat_fd.SetCleanup(false);
    out_vdex_fd.SetCleanup(false);
    image_fd.SetCleanup(false);
    reference_profile_fd.SetCleanup(false);

    return 0;
}

//	@frameworks/native/cmds/installd/dexopt.cpp
static void run_dex2oat(int zip_fd, int oat_fd, int input_vdex_fd, int output_vdex_fd, int image_fd,
        const char* input_file_name, const char* output_file_name, int swap_fd,
        const char* instruction_set, const char* compiler_filter,
        bool debuggable, bool post_bootcomplete, int profile_fd, const char* class_loader_context) {
    static const unsigned int MAX_INSTRUCTION_SET_LEN = 7;

    if (strlen(instruction_set) >= MAX_INSTRUCTION_SET_LEN) {
        ALOGE("Instruction set %s longer than max length of %d",instruction_set, MAX_INSTRUCTION_SET_LEN);
        return;
    }

    // Get the relative path to the input file.
    const char* relative_input_file_name = get_location_from_path(input_file_name);

    char dex2oat_Xms_flag[kPropertyValueMax];
    bool have_dex2oat_Xms_flag = get_property("dalvik.vm.dex2oat-Xms", dex2oat_Xms_flag, NULL) > 0;

    char dex2oat_Xmx_flag[kPropertyValueMax];
    bool have_dex2oat_Xmx_flag = get_property("dalvik.vm.dex2oat-Xmx", dex2oat_Xmx_flag, NULL) > 0;

    char dex2oat_threads_buf[kPropertyValueMax];
    bool have_dex2oat_threads_flag = get_property(post_bootcomplete ? "dalvik.vm.dex2oat-threads" : "dalvik.vm.boot-dex2oat-threads",
                                                  dex2oat_threads_buf,
                                                  NULL) > 0;
    char dex2oat_threads_arg[kPropertyValueMax + 2];
    if (have_dex2oat_threads_flag) {
        sprintf(dex2oat_threads_arg, "-j%s", dex2oat_threads_buf);
    }

    char dex2oat_isa_features_key[kPropertyKeyMax];
    sprintf(dex2oat_isa_features_key, "dalvik.vm.isa.%s.features", instruction_set);
    char dex2oat_isa_features[kPropertyValueMax];
    bool have_dex2oat_isa_features = get_property(dex2oat_isa_features_key,
                                                  dex2oat_isa_features, NULL) > 0;

    char dex2oat_isa_variant_key[kPropertyKeyMax];
    sprintf(dex2oat_isa_variant_key, "dalvik.vm.isa.%s.variant", instruction_set);
    char dex2oat_isa_variant[kPropertyValueMax];
    bool have_dex2oat_isa_variant = get_property(dex2oat_isa_variant_key,
                                                 dex2oat_isa_variant, NULL) > 0;

    const char *dex2oat_norelocation = "-Xnorelocate";
    bool have_dex2oat_relocation_skip_flag = false;

    char dex2oat_flags[kPropertyValueMax];
    int dex2oat_flags_count = get_property("dalvik.vm.dex2oat-flags",
                                 dex2oat_flags, NULL) <= 0 ? 0 : split_count(dex2oat_flags);
    ALOGV("dalvik.vm.dex2oat-flags=%s\n", dex2oat_flags);

    // If we are booting without the real /data, don't spend time compiling.
    char vold_decrypt[kPropertyValueMax];
    bool have_vold_decrypt = get_property("vold.decrypt", vold_decrypt, "") > 0;
    bool skip_compilation = (have_vold_decrypt &&
                             (strcmp(vold_decrypt, "trigger_restart_min_framework") == 0 ||
                             (strcmp(vold_decrypt, "1") == 0)));

    bool generate_debug_info = property_get_bool("debug.generate-debug-info", false);

    char app_image_format[kPropertyValueMax];
    char image_format_arg[strlen("--image-format=") + kPropertyValueMax];
    bool have_app_image_format =
            image_fd >= 0 && get_property("dalvik.vm.appimageformat", app_image_format, NULL) > 0;
    if (have_app_image_format) {
        sprintf(image_format_arg, "--image-format=%s", app_image_format);
    }

    char dex2oat_large_app_threshold[kPropertyValueMax];
    bool have_dex2oat_large_app_threshold =
            get_property("dalvik.vm.dex2oat-very-large", dex2oat_large_app_threshold, NULL) > 0;
    char dex2oat_large_app_threshold_arg[strlen("--very-large-app-threshold=") + kPropertyValueMax];
    if (have_dex2oat_large_app_threshold) {
        sprintf(dex2oat_large_app_threshold_arg,
                "--very-large-app-threshold=%s",
                dex2oat_large_app_threshold);
    }

    static const char* DEX2OAT_BIN = "/system/bin/dex2oat";

    static const char* RUNTIME_ARG = "--runtime-arg";

    static const int MAX_INT_LEN = 12;      // '-'+10dig+'\0' -OR- 0x+8dig

    // clang FORTIFY doesn't let us use strlen in constant array bounds, so we
    // use arraysize instead.
    char zip_fd_arg[arraysize("--zip-fd=") + MAX_INT_LEN];
    char zip_location_arg[arraysize("--zip-location=") + PKG_PATH_MAX];
    char input_vdex_fd_arg[arraysize("--input-vdex-fd=") + MAX_INT_LEN];
    char output_vdex_fd_arg[arraysize("--output-vdex-fd=") + MAX_INT_LEN];
    char oat_fd_arg[arraysize("--oat-fd=") + MAX_INT_LEN];
    char oat_location_arg[arraysize("--oat-location=") + PKG_PATH_MAX];
    char instruction_set_arg[arraysize("--instruction-set=") + MAX_INSTRUCTION_SET_LEN];
    char instruction_set_variant_arg[arraysize("--instruction-set-variant=") + kPropertyValueMax];
    char instruction_set_features_arg[arraysize("--instruction-set-features=") + kPropertyValueMax];
    char dex2oat_Xms_arg[arraysize("-Xms") + kPropertyValueMax];
    char dex2oat_Xmx_arg[arraysize("-Xmx") + kPropertyValueMax];
    char dex2oat_compiler_filter_arg[arraysize("--compiler-filter=") + kPropertyValueMax];
    bool have_dex2oat_swap_fd = false;
    char dex2oat_swap_fd[arraysize("--swap-fd=") + MAX_INT_LEN];
    bool have_dex2oat_image_fd = false;
    char dex2oat_image_fd[arraysize("--app-image-fd=") + MAX_INT_LEN];
    size_t class_loader_context_size = arraysize("--class-loader-context=") + PKG_PATH_MAX;
    char class_loader_context_arg[class_loader_context_size];
    if (class_loader_context != nullptr) {
        snprintf(class_loader_context_arg, class_loader_context_size, "--class-loader-context=%s",class_loader_context);
    }

    sprintf(zip_fd_arg, "--zip-fd=%d", zip_fd);
    sprintf(zip_location_arg, "--zip-location=%s", relative_input_file_name);
    sprintf(input_vdex_fd_arg, "--input-vdex-fd=%d", input_vdex_fd);
    sprintf(output_vdex_fd_arg, "--output-vdex-fd=%d", output_vdex_fd);
    sprintf(oat_fd_arg, "--oat-fd=%d", oat_fd);
    sprintf(oat_location_arg, "--oat-location=%s", output_file_name);
    sprintf(instruction_set_arg, "--instruction-set=%s", instruction_set);
    sprintf(instruction_set_variant_arg, "--instruction-set-variant=%s", dex2oat_isa_variant);
    sprintf(instruction_set_features_arg, "--instruction-set-features=%s", dex2oat_isa_features);
    if (swap_fd >= 0) {
        have_dex2oat_swap_fd = true;
        sprintf(dex2oat_swap_fd, "--swap-fd=%d", swap_fd);
    }
    if (image_fd >= 0) {
        have_dex2oat_image_fd = true;
        sprintf(dex2oat_image_fd, "--app-image-fd=%d", image_fd);
    }

    if (have_dex2oat_Xms_flag) {
        sprintf(dex2oat_Xms_arg, "-Xms%s", dex2oat_Xms_flag);
    }
    if (have_dex2oat_Xmx_flag) {
        sprintf(dex2oat_Xmx_arg, "-Xmx%s", dex2oat_Xmx_flag);
    }

    // Compute compiler filter.

    bool have_dex2oat_compiler_filter_flag = false;
    if (skip_compilation) {
        strcpy(dex2oat_compiler_filter_arg, "--compiler-filter=extract");
        have_dex2oat_compiler_filter_flag = true;
        have_dex2oat_relocation_skip_flag = true;
    } else if (compiler_filter != nullptr) {
        if (strlen(compiler_filter) + strlen("--compiler-filter=") <
                    arraysize(dex2oat_compiler_filter_arg)) {
            sprintf(dex2oat_compiler_filter_arg, "--compiler-filter=%s", compiler_filter);
            have_dex2oat_compiler_filter_flag = true;
        } else {
            ALOGW("Compiler filter name '%s' is too large (max characters is %zu)",
                  compiler_filter,
                  kPropertyValueMax);
        }
    }

    if (!have_dex2oat_compiler_filter_flag) {
        char dex2oat_compiler_filter_flag[kPropertyValueMax];
        have_dex2oat_compiler_filter_flag = get_property("dalvik.vm.dex2oat-filter",
                                                         dex2oat_compiler_filter_flag, NULL) > 0;
        if (have_dex2oat_compiler_filter_flag) {
            sprintf(dex2oat_compiler_filter_arg,
                    "--compiler-filter=%s",
                    dex2oat_compiler_filter_flag);
        }
    }

    // Check whether all apps should be compiled debuggable.
    if (!debuggable) {
        char prop_buf[kPropertyValueMax];
        debuggable =
                (get_property("dalvik.vm.always_debuggable", prop_buf, "0") > 0) &&
                (prop_buf[0] == '1');
    }
    char profile_arg[strlen("--profile-file-fd=") + MAX_INT_LEN];
    if (profile_fd != -1) {
        sprintf(profile_arg, "--profile-file-fd=%d", profile_fd);
    }

    // Get the directory of the apk to pass as a base classpath directory.
    char base_dir[arraysize("--classpath-dir=") + PKG_PATH_MAX];
    std::string apk_dir(input_file_name);
    unsigned long dir_index = apk_dir.rfind('/');
    bool has_base_dir = dir_index != std::string::npos;
    if (has_base_dir) {
        apk_dir = apk_dir.substr(0, dir_index);
        sprintf(base_dir, "--classpath-dir=%s", apk_dir.c_str());
    }


    ALOGV("Running %s in=%s out=%s\n", DEX2OAT_BIN, relative_input_file_name, output_file_name);

    const char* argv[9  // program name, mandatory arguments and the final NULL
                     + (have_dex2oat_isa_variant ? 1 : 0)
                     + (have_dex2oat_isa_features ? 1 : 0)
                     + (have_dex2oat_Xms_flag ? 2 : 0)
                     + (have_dex2oat_Xmx_flag ? 2 : 0)
                     + (have_dex2oat_compiler_filter_flag ? 1 : 0)
                     + (have_dex2oat_threads_flag ? 1 : 0)
                     + (have_dex2oat_swap_fd ? 1 : 0)
                     + (have_dex2oat_image_fd ? 1 : 0)
                     + (have_dex2oat_relocation_skip_flag ? 2 : 0)
                     + (generate_debug_info ? 1 : 0)
                     + (debuggable ? 1 : 0)
                     + (have_app_image_format ? 1 : 0)
                     + dex2oat_flags_count
                     + (profile_fd == -1 ? 0 : 1)
                     + (class_loader_context != nullptr ? 1 : 0)
                     + (has_base_dir ? 1 : 0)
                     + (have_dex2oat_large_app_threshold ? 1 : 0)];
    int i = 0;
    argv[i++] = DEX2OAT_BIN;
    argv[i++] = zip_fd_arg;
    argv[i++] = zip_location_arg;
    argv[i++] = input_vdex_fd_arg;
    argv[i++] = output_vdex_fd_arg;
    argv[i++] = oat_fd_arg;
    argv[i++] = oat_location_arg;
    argv[i++] = instruction_set_arg;
    if (have_dex2oat_isa_variant) {
        argv[i++] = instruction_set_variant_arg;
    }
    if (have_dex2oat_isa_features) {
        argv[i++] = instruction_set_features_arg;
    }
    if (have_dex2oat_Xms_flag) {
        argv[i++] = RUNTIME_ARG;
        argv[i++] = dex2oat_Xms_arg;
    }
    if (have_dex2oat_Xmx_flag) {
        argv[i++] = RUNTIME_ARG;
        argv[i++] = dex2oat_Xmx_arg;
    }
    if (have_dex2oat_compiler_filter_flag) {
        argv[i++] = dex2oat_compiler_filter_arg;
    }
    if (have_dex2oat_threads_flag) {
        argv[i++] = dex2oat_threads_arg;
    }
    if (have_dex2oat_swap_fd) {
        argv[i++] = dex2oat_swap_fd;
    }
    if (have_dex2oat_image_fd) {
        argv[i++] = dex2oat_image_fd;
    }
    if (generate_debug_info) {
        argv[i++] = "--generate-debug-info";
    }
    if (debuggable) {
        argv[i++] = "--debuggable";
    }
    if (have_app_image_format) {
        argv[i++] = image_format_arg;
    }
    if (have_dex2oat_large_app_threshold) {
        argv[i++] = dex2oat_large_app_threshold_arg;
    }
    if (dex2oat_flags_count) {
        i += split(dex2oat_flags, argv + i);
    }
    if (have_dex2oat_relocation_skip_flag) {
        argv[i++] = RUNTIME_ARG;
        argv[i++] = dex2oat_norelocation;
    }
    if (profile_fd != -1) {
        argv[i++] = profile_arg;
    }
    if (has_base_dir) {
        argv[i++] = base_dir;
    }
    if (class_loader_context != nullptr) {
        argv[i++] = class_loader_context_arg;
    }

    // Do not add after dex2oat_flags, they should override others for debugging.
    argv[i] = NULL;

    execv(DEX2OAT_BIN, (char * const *)argv);
    ALOGE("execv(%s) failed: %s\n", DEX2OAT_BIN, strerror(errno));
}