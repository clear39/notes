public class PackageManagerService extends IPackageManager.Stub implements PackageSender {


   // SystemServer 入口
   // 
   public static PackageManagerService main(Context context, Installer installer,
            boolean factoryTest, boolean onlyCore) {
        // Self-check for initial settings.
        PackageManagerServiceCompilerMapping.checkProperties();

        PackageManagerService m = new PackageManagerService(context, installer,factoryTest, onlyCore);
        m.enableSystemUserPackages();
        ServiceManager.addService("package", m);
        final PackageManagerNative pmn = m.new PackageManagerNative();
        ServiceManager.addService("package_native", pmn);
        return m;
    }

   public PackageManagerService(Context context, Installer installer,
            boolean factoryTest, boolean onlyCore) {
        LockGuard.installLock(mPackages, LockGuard.INDEX_PACKAGES);
        Trace.traceBegin(TRACE_TAG_PACKAGE_MANAGER, "create package manager");
        EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_START,
                SystemClock.uptimeMillis());

        if (mSdkVersion <= 0) {
            Slog.w(TAG, "**** ro.build.version.sdk not set!");
        }

        mContext = context;

        mFactoryTest = factoryTest;
        mOnlyCore = onlyCore;
        mMetrics = new DisplayMetrics();
        mInstaller = installer;

        // Create sub-components that provide services / data. Order here is important.
        synchronized (mInstallLock) {
        synchronized (mPackages) {
            // Expose private service for system components to use.
            LocalServices.addService(
                    PackageManagerInternal.class, new PackageManagerInternalImpl());
                    
            sUserManager = new UserManagerService(context, this,
                    new UserDataPreparer(mInstaller, mInstallLock, mContext, mOnlyCore), mPackages);
            mPermissionManager = PermissionManagerService.create(context,
                    new DefaultPermissionGrantedCallback() {
                        @Override
                        public void onDefaultRuntimePermissionsGranted(int userId) {
                            synchronized(mPackages) {
                                mSettings.onDefaultRuntimePermissionsGrantedLPr(userId);
                            }
                        }
                    }, mPackages /*externalLock*/);
            mDefaultPermissionPolicy = mPermissionManager.getDefaultPermissionGrantPolicy();
            mSettings = new Settings(mPermissionManager.getPermissionSettings(), mPackages);
        }
        }
        mSettings.addSharedUserLPw("android.uid.system", Process.SYSTEM_UID,
                ApplicationInfo.FLAG_SYSTEM, ApplicationInfo.PRIVATE_FLAG_PRIVILEGED);
        mSettings.addSharedUserLPw("android.uid.phone", RADIO_UID,
                ApplicationInfo.FLAG_SYSTEM, ApplicationInfo.PRIVATE_FLAG_PRIVILEGED);
        mSettings.addSharedUserLPw("android.uid.log", LOG_UID,
                ApplicationInfo.FLAG_SYSTEM, ApplicationInfo.PRIVATE_FLAG_PRIVILEGED);
        mSettings.addSharedUserLPw("android.uid.nfc", NFC_UID,
                ApplicationInfo.FLAG_SYSTEM, ApplicationInfo.PRIVATE_FLAG_PRIVILEGED);
        mSettings.addSharedUserLPw("android.uid.bluetooth", BLUETOOTH_UID,
                ApplicationInfo.FLAG_SYSTEM, ApplicationInfo.PRIVATE_FLAG_PRIVILEGED);
        mSettings.addSharedUserLPw("android.uid.shell", SHELL_UID,
                ApplicationInfo.FLAG_SYSTEM, ApplicationInfo.PRIVATE_FLAG_PRIVILEGED);
        mSettings.addSharedUserLPw("android.uid.se", SE_UID,
                ApplicationInfo.FLAG_SYSTEM, ApplicationInfo.PRIVATE_FLAG_PRIVILEGED);

        String separateProcesses = SystemProperties.get("debug.separate_processes");
        if (separateProcesses != null && separateProcesses.length() > 0) {
            if ("*".equals(separateProcesses)) {
                mDefParseFlags = PackageParser.PARSE_IGNORE_PROCESSES;
                mSeparateProcesses = null;
                Slog.w(TAG, "Running with debug.separate_processes: * (ALL)");
            } else {
                mDefParseFlags = 0;
                mSeparateProcesses = separateProcesses.split(",");
                Slog.w(TAG, "Running with debug.separate_processes: "
                        + separateProcesses);
            }
        } else {
            mDefParseFlags = 0;
            mSeparateProcesses = null;
        }

        mPackageDexOptimizer = new PackageDexOptimizer(installer, mInstallLock, context,
                "*dexopt*");
        DexManager.Listener dexManagerListener = DexLogger.getListener(this,
                installer, mInstallLock);
        mDexManager = new DexManager(mContext, this, mPackageDexOptimizer, installer, mInstallLock,
                dexManagerListener);
        mArtManagerService = new ArtManagerService(mContext, this, installer, mInstallLock);
        mMoveCallbacks = new MoveCallbacks(FgThread.get().getLooper());

        mOnPermissionChangeListeners = new OnPermissionChangeListeners(
                FgThread.get().getLooper());

        getDefaultDisplayMetrics(context, mMetrics);

        Trace.traceBegin(TRACE_TAG_PACKAGE_MANAGER, "get system config");
        SystemConfig systemConfig = SystemConfig.getInstance();
        mAvailableFeatures = systemConfig.getAvailableFeatures();
        Trace.traceEnd(TRACE_TAG_PACKAGE_MANAGER);

        mProtectedPackages = new ProtectedPackages(mContext);

        synchronized (mInstallLock) {
        // writer
        synchronized (mPackages) {
            mHandlerThread = new ServiceThread(TAG,
                    Process.THREAD_PRIORITY_BACKGROUND, true /*allowIo*/);
            mHandlerThread.start();
            mHandler = new PackageHandler(mHandlerThread.getLooper());
            mProcessLoggingHandler = new ProcessLoggingHandler();
            Watchdog.getInstance().addThread(mHandler, WATCHDOG_TIMEOUT);
            mInstantAppRegistry = new InstantAppRegistry(this);

            ArrayMap<String, String> libConfig = systemConfig.getSharedLibraries();
            final int builtInLibCount = libConfig.size();
            for (int i = 0; i < builtInLibCount; i++) {
                String name = libConfig.keyAt(i);
                String path = libConfig.valueAt(i);
                addSharedLibraryLPw(path, null, name, SharedLibraryInfo.VERSION_UNDEFINED,
                        SharedLibraryInfo.TYPE_BUILTIN, PLATFORM_PACKAGE_NAME, 0);
            }

            SELinuxMMAC.readInstallPolicy();

            Trace.traceBegin(TRACE_TAG_PACKAGE_MANAGER, "loadFallbacks");
            FallbackCategoryProvider.loadFallbacks();
            Trace.traceEnd(TRACE_TAG_PACKAGE_MANAGER);

            Trace.traceBegin(TRACE_TAG_PACKAGE_MANAGER, "read user settings");
            mFirstBoot = !mSettings.readLPw(sUserManager.getUsers(false));
            Trace.traceEnd(TRACE_TAG_PACKAGE_MANAGER);

            // Clean up orphaned packages for which the code path doesn't exist
            // and they are an update to a system app - caused by bug/32321269
            final int packageSettingCount = mSettings.mPackages.size();
            for (int i = packageSettingCount - 1; i >= 0; i--) {
                PackageSetting ps = mSettings.mPackages.valueAt(i);
                if (!isExternal(ps) && (ps.codePath == null || !ps.codePath.exists())
                        && mSettings.getDisabledSystemPkgLPr(ps.name) != null) {
                    mSettings.mPackages.removeAt(i);
                    mSettings.enableSystemPackageLPw(ps.name);
                }
            }

            if (mFirstBoot) {
                requestCopyPreoptedFiles();
            }

            String customResolverActivity = Resources.getSystem().getString(
                    R.string.config_customResolverActivity);
            if (TextUtils.isEmpty(customResolverActivity)) {
                customResolverActivity = null;
            } else {
                mCustomResolverComponentName = ComponentName.unflattenFromString(
                        customResolverActivity);
            }

            long startTime = SystemClock.uptimeMillis();

            EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_SYSTEM_SCAN_START,
                    startTime);

            final String bootClassPath = System.getenv("BOOTCLASSPATH");
            final String systemServerClassPath = System.getenv("SYSTEMSERVERCLASSPATH");

            if (bootClassPath == null) {
                Slog.w(TAG, "No BOOTCLASSPATH found!");
            }

            if (systemServerClassPath == null) {
                Slog.w(TAG, "No SYSTEMSERVERCLASSPATH found!");
            }

            File frameworkDir = new File(Environment.getRootDirectory(), "framework");

            final VersionInfo ver = mSettings.getInternalVersion();
            mIsUpgrade = !Build.FINGERPRINT.equals(ver.fingerprint);
            if (mIsUpgrade) {
                logCriticalInfo(Log.INFO,
                        "Upgrading from " + ver.fingerprint + " to " + Build.FINGERPRINT);
            }

            // when upgrading from pre-M, promote system app permissions from install to runtime
            mPromoteSystemApps =
                    mIsUpgrade && ver.sdkVersion <= Build.VERSION_CODES.LOLLIPOP_MR1;

            // When upgrading from pre-N, we need to handle package extraction like first boot,
            // as there is no profiling data available.
            mIsPreNUpgrade = mIsUpgrade && ver.sdkVersion < Build.VERSION_CODES.N;

            mIsPreNMR1Upgrade = mIsUpgrade && ver.sdkVersion < Build.VERSION_CODES.N_MR1;

            // save off the names of pre-existing system packages prior to scanning; we don't
            // want to automatically grant runtime permissions for new system apps
            if (mPromoteSystemApps) {
                Iterator<PackageSetting> pkgSettingIter = mSettings.mPackages.values().iterator();
                while (pkgSettingIter.hasNext()) {
                    PackageSetting ps = pkgSettingIter.next();
                    if (isSystemApp(ps)) {
                        mExistingSystemPackages.add(ps.name);
                    }
                }
            }

            mCacheDir = preparePackageParserCache(mIsUpgrade);

            if(DEBUG_PACKAGE_SCANNING){
              Slog.d(TAG,"mCacheDir : " + mCacheDir);
            }

            // Set flag to monitor and not change apk file paths when
            // scanning install directories.
            int scanFlags = SCAN_BOOTING | SCAN_INITIAL;

            if (mIsUpgrade || mFirstBoot) {
                scanFlags = scanFlags | SCAN_FIRST_BOOT_OR_UPGRADE;
            }

            // Collect vendor/product overlay packages. (Do this before scanning any apps.)
            // For security and version matching reason, only consider
            // overlay packages if they reside in the right directory.
            scanDirTracedLI(new File(VENDOR_OVERLAY_DIR),
                    mDefParseFlags
                    | PackageParser.PARSE_IS_SYSTEM_DIR,
                    scanFlags
                    | SCAN_AS_SYSTEM
                    | SCAN_AS_VENDOR,
                    0);
            scanDirTracedLI(new File(PRODUCT_OVERLAY_DIR),
                    mDefParseFlags
                    | PackageParser.PARSE_IS_SYSTEM_DIR,
                    scanFlags
                    | SCAN_AS_SYSTEM
                    | SCAN_AS_PRODUCT,
                    0);

            mParallelPackageParserCallback.findStaticOverlayPackages();

            // Find base frameworks (resource packages without code).
            scanDirTracedLI(frameworkDir,
                    mDefParseFlags
                    | PackageParser.PARSE_IS_SYSTEM_DIR,
                    scanFlags
                    | SCAN_NO_DEX
                    | SCAN_AS_SYSTEM
                    | SCAN_AS_PRIVILEGED,
                    0);

            // Collect privileged system packages.
            final File privilegedAppDir = new File(Environment.getRootDirectory(), "priv-app");
            scanDirTracedLI(privilegedAppDir,
                    mDefParseFlags
                    | PackageParser.PARSE_IS_SYSTEM_DIR,
                    scanFlags
                    | SCAN_AS_SYSTEM
                    | SCAN_AS_PRIVILEGED,
                    0);

            // Collect ordinary system packages.
            final File systemAppDir = new File(Environment.getRootDirectory(), "app");
            scanDirTracedLI(systemAppDir,
                    mDefParseFlags
                    | PackageParser.PARSE_IS_SYSTEM_DIR,
                    scanFlags
                    | SCAN_AS_SYSTEM,
                    0);

            // Collect privileged vendor packages.
            File privilegedVendorAppDir = new File(Environment.getVendorDirectory(), "priv-app");
            try {
                privilegedVendorAppDir = privilegedVendorAppDir.getCanonicalFile();
            } catch (IOException e) {
                // failed to look up canonical path, continue with original one
            }
            scanDirTracedLI(privilegedVendorAppDir,
                    mDefParseFlags
                    | PackageParser.PARSE_IS_SYSTEM_DIR,
                    scanFlags
                    | SCAN_AS_SYSTEM
                    | SCAN_AS_VENDOR
                    | SCAN_AS_PRIVILEGED,
                    0);

            // Collect ordinary vendor packages.
            File vendorAppDir = new File(Environment.getVendorDirectory(), "app");
            try {
                vendorAppDir = vendorAppDir.getCanonicalFile();
            } catch (IOException e) {
                // failed to look up canonical path, continue with original one
            }
            scanDirTracedLI(vendorAppDir,
                    mDefParseFlags
                    | PackageParser.PARSE_IS_SYSTEM_DIR,
                    scanFlags
                    | SCAN_AS_SYSTEM
                    | SCAN_AS_VENDOR,
                    0);

            // Collect privileged odm packages. /odm is another vendor partition
            // other than /vendor.
            File privilegedOdmAppDir = new File(Environment.getOdmDirectory(),
                        "priv-app");
            try {
                privilegedOdmAppDir = privilegedOdmAppDir.getCanonicalFile();
            } catch (IOException e) {
                // failed to look up canonical path, continue with original one
            }
            scanDirTracedLI(privilegedOdmAppDir,
                    mDefParseFlags
                    | PackageParser.PARSE_IS_SYSTEM_DIR,
                    scanFlags
                    | SCAN_AS_SYSTEM
                    | SCAN_AS_VENDOR
                    | SCAN_AS_PRIVILEGED,
                    0);

            // Collect ordinary odm packages. /odm is another vendor partition
            // other than /vendor.
            File odmAppDir = new File(Environment.getOdmDirectory(), "app");
            try {
                odmAppDir = odmAppDir.getCanonicalFile();
            } catch (IOException e) {
                // failed to look up canonical path, continue with original one
            }
            scanDirTracedLI(odmAppDir,
                    mDefParseFlags
                    | PackageParser.PARSE_IS_SYSTEM_DIR,
                    scanFlags
                    | SCAN_AS_SYSTEM
                    | SCAN_AS_VENDOR,
                    0);

            // Collect all OEM packages.
            final File oemAppDir = new File(Environment.getOemDirectory(), "app");
            scanDirTracedLI(oemAppDir,
                    mDefParseFlags
                    | PackageParser.PARSE_IS_SYSTEM_DIR,
                    scanFlags
                    | SCAN_AS_SYSTEM
                    | SCAN_AS_OEM,
                    0);

            // Collected privileged product packages.
            File privilegedProductAppDir = new File(Environment.getProductDirectory(), "priv-app");
            try {
                privilegedProductAppDir = privilegedProductAppDir.getCanonicalFile();
            } catch (IOException e) {
                // failed to look up canonical path, continue with original one
            }
            scanDirTracedLI(privilegedProductAppDir,
                    mDefParseFlags
                    | PackageParser.PARSE_IS_SYSTEM_DIR,
                    scanFlags
                    | SCAN_AS_SYSTEM
                    | SCAN_AS_PRODUCT
                    | SCAN_AS_PRIVILEGED,
                    0);

            // Collect ordinary product packages.
            File productAppDir = new File(Environment.getProductDirectory(), "app");
            try {
                productAppDir = productAppDir.getCanonicalFile();
            } catch (IOException e) {
                // failed to look up canonical path, continue with original one
            }
            scanDirTracedLI(productAppDir,
                    mDefParseFlags
                    | PackageParser.PARSE_IS_SYSTEM_DIR,
                    scanFlags
                    | SCAN_AS_SYSTEM
                    | SCAN_AS_PRODUCT,
                    0);

            // Prune any system packages that no longer exist.
            final List<String> possiblyDeletedUpdatedSystemApps = new ArrayList<>();
            // Stub packages must either be replaced with full versions in the /data
            // partition or be disabled.
            final List<String> stubSystemApps = new ArrayList<>();
            if (!mOnlyCore) {
                // do this first before mucking with mPackages for the "expecting better" case
                final Iterator<PackageParser.Package> pkgIterator = mPackages.values().iterator();
                while (pkgIterator.hasNext()) {
                    final PackageParser.Package pkg = pkgIterator.next();
                    if (pkg.isStub) {
                        stubSystemApps.add(pkg.packageName);
                    }
                }

                final Iterator<PackageSetting> psit = mSettings.mPackages.values().iterator();
                while (psit.hasNext()) {
                    PackageSetting ps = psit.next();

                    /*
                     * If this is not a system app, it can't be a
                     * disable system app.
                     */
                    if ((ps.pkgFlags & ApplicationInfo.FLAG_SYSTEM) == 0) {
                        continue;
                    }

                    /*
                     * If the package is scanned, it's not erased.
                     */
                    final PackageParser.Package scannedPkg = mPackages.get(ps.name);
                    if (scannedPkg != null) {
                        /*
                         * If the system app is both scanned and in the
                         * disabled packages list, then it must have been
                         * added via OTA. Remove it from the currently
                         * scanned package so the previously user-installed
                         * application can be scanned.
                         */
                        if (mSettings.isDisabledSystemPackageLPr(ps.name)) {
                            logCriticalInfo(Log.WARN,
                                    "Expecting better updated system app for " + ps.name
                                    + "; removing system app.  Last known"
                                    + " codePath=" + ps.codePathString
                                    + ", versionCode=" + ps.versionCode
                                    + "; scanned versionCode=" + scannedPkg.getLongVersionCode());
                            removePackageLI(scannedPkg, true);
                            mExpectingBetter.put(ps.name, ps.codePath);
                        }

                        continue;
                    }

                    if (!mSettings.isDisabledSystemPackageLPr(ps.name)) {
                        psit.remove();
                        logCriticalInfo(Log.WARN, "System package " + ps.name
                                + " no longer exists; it's data will be wiped");
                        // Actual deletion of code and data will be handled by later
                        // reconciliation step
                    } else {
                        // we still have a disabled system package, but, it still might have
                        // been removed. check the code path still exists and check there's
                        // still a package. the latter can happen if an OTA keeps the same
                        // code path, but, changes the package name.
                        final PackageSetting disabledPs =
                                mSettings.getDisabledSystemPkgLPr(ps.name);
                        if (disabledPs.codePath == null || !disabledPs.codePath.exists()
                                || disabledPs.pkg == null) {
                            possiblyDeletedUpdatedSystemApps.add(ps.name);
                        }
                    }
                }
            }

            //delete tmp files
            deleteTempPackageFiles();

            final int cachedSystemApps = PackageParser.sCachedPackageReadCount.get();

            // Remove any shared userIDs that have no associated packages
            mSettings.pruneSharedUsersLPw();
            final long systemScanTime = SystemClock.uptimeMillis() - startTime;
            final int systemPackagesCount = mPackages.size();
            Slog.i(TAG, "Finished scanning system apps. Time: " + systemScanTime
                    + " ms, packageCount: " + systemPackagesCount
                    + " , timePerPackage: "
                    + (systemPackagesCount == 0 ? 0 : systemScanTime / systemPackagesCount)
                    + " , cached: " + cachedSystemApps);
            if (mIsUpgrade && systemPackagesCount > 0) {
                MetricsLogger.histogram(null, "ota_package_manager_system_app_avg_scan_time",
                        ((int) systemScanTime) / systemPackagesCount);
            }
            if (!mOnlyCore) {
                EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_DATA_SCAN_START,
                        SystemClock.uptimeMillis());
                scanDirTracedLI(sAppInstallDir, 0, scanFlags | SCAN_REQUIRE_KNOWN, 0);

                scanDirTracedLI(sDrmAppPrivateInstallDir, mDefParseFlags
                        | PackageParser.PARSE_FORWARD_LOCK,
                        scanFlags | SCAN_REQUIRE_KNOWN, 0);

                // Remove disable package settings for updated system apps that were
                // removed via an OTA. If the update is no longer present, remove the
                // app completely. Otherwise, revoke their system privileges.
                for (String deletedAppName : possiblyDeletedUpdatedSystemApps) {
                    PackageParser.Package deletedPkg = mPackages.get(deletedAppName);
                    mSettings.removeDisabledSystemPackageLPw(deletedAppName);
                    final String msg;
                    if (deletedPkg == null) {
                        // should have found an update, but, we didn't; remove everything
                        msg = "Updated system package " + deletedAppName
                                + " no longer exists; removing its data";
                        // Actual deletion of code and data will be handled by later
                        // reconciliation step
                    } else {
                        // found an update; revoke system privileges
                        msg = "Updated system package + " + deletedAppName
                                + " no longer exists; revoking system privileges";

                        // Don't do anything if a stub is removed from the system image. If
                        // we were to remove the uncompressed version from the /data partition,
                        // this is where it'd be done.

                        final PackageSetting deletedPs = mSettings.mPackages.get(deletedAppName);
                        deletedPkg.applicationInfo.flags &= ~ApplicationInfo.FLAG_SYSTEM;
                        deletedPs.pkgFlags &= ~ApplicationInfo.FLAG_SYSTEM;
                    }
                    logCriticalInfo(Log.WARN, msg);
                }

                /*
                 * Make sure all system apps that we expected to appear on
                 * the userdata partition actually showed up. If they never
                 * appeared, crawl back and revive the system version.
                 */
                for (int i = 0; i < mExpectingBetter.size(); i++) {
                    final String packageName = mExpectingBetter.keyAt(i);
                    if (!mPackages.containsKey(packageName)) {
                        final File scanFile = mExpectingBetter.valueAt(i);

                        logCriticalInfo(Log.WARN, "Expected better " + packageName
                                + " but never showed up; reverting to system");

                        final @ParseFlags int reparseFlags;
                        final @ScanFlags int rescanFlags;
                        if (FileUtils.contains(privilegedAppDir, scanFile)) {
                            reparseFlags =
                                    mDefParseFlags |
                                    PackageParser.PARSE_IS_SYSTEM_DIR;
                            rescanFlags =
                                    scanFlags
                                    | SCAN_AS_SYSTEM
                                    | SCAN_AS_PRIVILEGED;
                        } else if (FileUtils.contains(systemAppDir, scanFile)) {
                            reparseFlags =
                                    mDefParseFlags |
                                    PackageParser.PARSE_IS_SYSTEM_DIR;
                            rescanFlags =
                                    scanFlags
                                    | SCAN_AS_SYSTEM;
                        } else if (FileUtils.contains(privilegedVendorAppDir, scanFile)
                                || FileUtils.contains(privilegedOdmAppDir, scanFile)) {
                            reparseFlags =
                                    mDefParseFlags |
                                    PackageParser.PARSE_IS_SYSTEM_DIR;
                            rescanFlags =
                                    scanFlags
                                    | SCAN_AS_SYSTEM
                                    | SCAN_AS_VENDOR
                                    | SCAN_AS_PRIVILEGED;
                        } else if (FileUtils.contains(vendorAppDir, scanFile)
                                || FileUtils.contains(odmAppDir, scanFile)) {
                            reparseFlags =
                                    mDefParseFlags |
                                    PackageParser.PARSE_IS_SYSTEM_DIR;
                            rescanFlags =
                                    scanFlags
                                    | SCAN_AS_SYSTEM
                                    | SCAN_AS_VENDOR;
                        } else if (FileUtils.contains(oemAppDir, scanFile)) {
                            reparseFlags =
                                    mDefParseFlags |
                                    PackageParser.PARSE_IS_SYSTEM_DIR;
                            rescanFlags =
                                    scanFlags
                                    | SCAN_AS_SYSTEM
                                    | SCAN_AS_OEM;
                        } else if (FileUtils.contains(privilegedProductAppDir, scanFile)) {
                            reparseFlags =
                                    mDefParseFlags |
                                    PackageParser.PARSE_IS_SYSTEM_DIR;
                            rescanFlags =
                                    scanFlags
                                    | SCAN_AS_SYSTEM
                                    | SCAN_AS_PRODUCT
                                    | SCAN_AS_PRIVILEGED;
                        } else if (FileUtils.contains(productAppDir, scanFile)) {
                            reparseFlags =
                                    mDefParseFlags |
                                    PackageParser.PARSE_IS_SYSTEM_DIR;
                            rescanFlags =
                                    scanFlags
                                    | SCAN_AS_SYSTEM
                                    | SCAN_AS_PRODUCT;
                        } else {
                            Slog.e(TAG, "Ignoring unexpected fallback path " + scanFile);
                            continue;
                        }

                        mSettings.enableSystemPackageLPw(packageName);

                        try {
                            scanPackageTracedLI(scanFile, reparseFlags, rescanFlags, 0, null);
                        } catch (PackageManagerException e) {
                            Slog.e(TAG, "Failed to parse original system package: "
                                    + e.getMessage());
                        }
                    }
                }

                // Uncompress and install any stubbed system applications.
                // This must be done last to ensure all stubs are replaced or disabled.
                decompressSystemApplications(stubSystemApps, scanFlags);

                final int cachedNonSystemApps = PackageParser.sCachedPackageReadCount.get()
                                - cachedSystemApps;

                final long dataScanTime = SystemClock.uptimeMillis() - systemScanTime - startTime;
                final int dataPackagesCount = mPackages.size() - systemPackagesCount;
                Slog.i(TAG, "Finished scanning non-system apps. Time: " + dataScanTime
                        + " ms, packageCount: " + dataPackagesCount
                        + " , timePerPackage: "
                        + (dataPackagesCount == 0 ? 0 : dataScanTime / dataPackagesCount)
                        + " , cached: " + cachedNonSystemApps);
                if (mIsUpgrade && dataPackagesCount > 0) {
                    MetricsLogger.histogram(null, "ota_package_manager_data_app_avg_scan_time",
                            ((int) dataScanTime) / dataPackagesCount);
                }
            }
            mExpectingBetter.clear();

            // Resolve the storage manager.
            mStorageManagerPackage = getStorageManagerPackageName();

            // Resolve protected action filters. Only the setup wizard is allowed to
            // have a high priority filter for these actions.
            mSetupWizardPackage = getSetupWizardPackageName();
            if (mProtectedFilters.size() > 0) {
                if (DEBUG_FILTERS && mSetupWizardPackage == null) {
                    Slog.i(TAG, "No setup wizard;"
                        + " All protected intents capped to priority 0");
                }
                for (ActivityIntentInfo filter : mProtectedFilters) {
                    if (filter.activity.info.packageName.equals(mSetupWizardPackage)) {
                        if (DEBUG_FILTERS) {
                            Slog.i(TAG, "Found setup wizard;"
                                + " allow priority " + filter.getPriority() + ";"
                                + " package: " + filter.activity.info.packageName
                                + " activity: " + filter.activity.className
                                + " priority: " + filter.getPriority());
                        }
                        // skip setup wizard; allow it to keep the high priority filter
                        continue;
                    }
                    if (DEBUG_FILTERS) {
                        Slog.i(TAG, "Protected action; cap priority to 0;"
                                + " package: " + filter.activity.info.packageName
                                + " activity: " + filter.activity.className
                                + " origPrio: " + filter.getPriority());
                    }
                    filter.setPriority(0);
                }
            }

            mSystemTextClassifierPackage = getSystemTextClassifierPackageName();

            mDeferProtectedFilters = false;
            mProtectedFilters.clear();

            // Now that we know all of the shared libraries, update all clients to have
            // the correct library paths.
            updateAllSharedLibrariesLPw(null);

            for (SharedUserSetting setting : mSettings.getAllSharedUsersLPw()) {
                // NOTE: We ignore potential failures here during a system scan (like
                // the rest of the commands above) because there's precious little we
                // can do about it. A settings error is reported, though.
                final List<String> changedAbiCodePath =
                        adjustCpuAbisForSharedUserLPw(setting.packages, null /*scannedPackage*/);
                if (changedAbiCodePath != null && changedAbiCodePath.size() > 0) {
                    for (int i = changedAbiCodePath.size() - 1; i >= 0; --i) {
                        final String codePathString = changedAbiCodePath.get(i);
                        try {
                            mInstaller.rmdex(codePathString,
                                    getDexCodeInstructionSet(getPreferredInstructionSet()));
                        } catch (InstallerException ignored) {
                        }
                    }
                }
                // Adjust seInfo to ensure apps which share a sharedUserId are placed in the same
                // SELinux domain.
                setting.fixSeInfoLocked();
            }

            // Now that we know all the packages we are keeping,
            // read and update their last usage times.
            mPackageUsage.read(mPackages);
            mCompilerStats.read();

            EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_SCAN_END,
                    SystemClock.uptimeMillis());
            Slog.i(TAG, "Time to scan packages: "
                    + ((SystemClock.uptimeMillis()-startTime)/1000f)
                    + " seconds");

            // If the platform SDK has changed since the last time we booted,
            // we need to re-grant app permission to catch any new ones that
            // appear.  This is really a hack, and means that apps can in some
            // cases get permissions that the user didn't initially explicitly
            // allow...  it would be nice to have some better way to handle
            // this situation.
            final boolean sdkUpdated = (ver.sdkVersion != mSdkVersion);
            if (sdkUpdated) {
                Slog.i(TAG, "Platform changed from " + ver.sdkVersion + " to "
                        + mSdkVersion + "; regranting permissions for internal storage");
            }
            mPermissionManager.updateAllPermissions(
                    StorageManager.UUID_PRIVATE_INTERNAL, sdkUpdated, mPackages.values(),
                    mPermissionCallback);
            ver.sdkVersion = mSdkVersion;

            // If this is the first boot or an update from pre-M, and it is a normal
            // boot, then we need to initialize the default preferred apps across
            // all defined users.
            if (!onlyCore && (mPromoteSystemApps || mFirstBoot)) {
                for (UserInfo user : sUserManager.getUsers(true)) {
                    mSettings.applyDefaultPreferredAppsLPw(this, user.id);
                    applyFactoryDefaultBrowserLPw(user.id);
                    primeDomainVerificationsLPw(user.id);
                }
            }

            // Prepare storage for system user really early during boot,
            // since core system apps like SettingsProvider and SystemUI
            // can't wait for user to start
            final int storageFlags;
            if (StorageManager.isFileEncryptedNativeOrEmulated()) {
                storageFlags = StorageManager.FLAG_STORAGE_DE;
            } else {
                storageFlags = StorageManager.FLAG_STORAGE_DE | StorageManager.FLAG_STORAGE_CE;
            }
            List<String> deferPackages = reconcileAppsDataLI(StorageManager.UUID_PRIVATE_INTERNAL,
                    UserHandle.USER_SYSTEM, storageFlags, true /* migrateAppData */,
                    true /* onlyCoreApps */);
            mPrepareAppDataFuture = SystemServerInitThreadPool.get().submit(() -> {
                TimingsTraceLog traceLog = new TimingsTraceLog("SystemServerTimingAsync",
                        Trace.TRACE_TAG_PACKAGE_MANAGER);
                traceLog.traceBegin("AppDataFixup");
                try {
                    mInstaller.fixupAppData(StorageManager.UUID_PRIVATE_INTERNAL,
                            StorageManager.FLAG_STORAGE_DE | StorageManager.FLAG_STORAGE_CE);
                } catch (InstallerException e) {
                    Slog.w(TAG, "Trouble fixing GIDs", e);
                }
                traceLog.traceEnd();

                traceLog.traceBegin("AppDataPrepare");
                if (deferPackages == null || deferPackages.isEmpty()) {
                    return;
                }
                int count = 0;
                for (String pkgName : deferPackages) {
                    PackageParser.Package pkg = null;
                    synchronized (mPackages) {
                        PackageSetting ps = mSettings.getPackageLPr(pkgName);
                        if (ps != null && ps.getInstalled(UserHandle.USER_SYSTEM)) {
                            pkg = ps.pkg;
                        }
                    }
                    if (pkg != null) {
                        synchronized (mInstallLock) {
                            prepareAppDataAndMigrateLIF(pkg, UserHandle.USER_SYSTEM, storageFlags,
                                    true /* maybeMigrateAppData */);
                        }
                        count++;
                    }
                }
                traceLog.traceEnd();
                Slog.i(TAG, "Deferred reconcileAppsData finished " + count + " packages");
            }, "prepareAppData");

            // If this is first boot after an OTA, and a normal boot, then
            // we need to clear code cache directories.
            // Note that we do *not* clear the application profiles. These remain valid
            // across OTAs and are used to drive profile verification (post OTA) and
            // profile compilation (without waiting to collect a fresh set of profiles).
            if (mIsUpgrade && !onlyCore) {
                Slog.i(TAG, "Build fingerprint changed; clearing code caches");
                for (int i = 0; i < mSettings.mPackages.size(); i++) {
                    final PackageSetting ps = mSettings.mPackages.valueAt(i);
                    if (Objects.equals(StorageManager.UUID_PRIVATE_INTERNAL, ps.volumeUuid)) {
                        // No apps are running this early, so no need to freeze
                        clearAppDataLIF(ps.pkg, UserHandle.USER_ALL,
                                StorageManager.FLAG_STORAGE_DE | StorageManager.FLAG_STORAGE_CE
                                        | Installer.FLAG_CLEAR_CODE_CACHE_ONLY);
                    }
                }
                ver.fingerprint = Build.FINGERPRINT;
            }

            checkDefaultBrowser();

            // clear only after permissions and other defaults have been updated
            mExistingSystemPackages.clear();
            mPromoteSystemApps = false;

            // All the changes are done during package scanning.
            ver.databaseVersion = Settings.CURRENT_DATABASE_VERSION;

            // can downgrade to reader
            Trace.traceBegin(TRACE_TAG_PACKAGE_MANAGER, "write settings");
            mSettings.writeLPr();
            Trace.traceEnd(TRACE_TAG_PACKAGE_MANAGER);
            EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_READY,
                    SystemClock.uptimeMillis());

            if (!mOnlyCore) {
                mRequiredVerifierPackage = getRequiredButNotReallyRequiredVerifierLPr();
                mRequiredInstallerPackage = getRequiredInstallerLPr();
                mRequiredUninstallerPackage = getRequiredUninstallerLPr();
                mIntentFilterVerifierComponent = getIntentFilterVerifierComponentNameLPr();
                if (mIntentFilterVerifierComponent != null) {
                    mIntentFilterVerifier = new IntentVerifierProxy(mContext,
                            mIntentFilterVerifierComponent);
                } else {
                    mIntentFilterVerifier = null;
                }
                mServicesSystemSharedLibraryPackageName = getRequiredSharedLibraryLPr(
                        PackageManager.SYSTEM_SHARED_LIBRARY_SERVICES,
                        SharedLibraryInfo.VERSION_UNDEFINED);
                mSharedSystemSharedLibraryPackageName = getRequiredSharedLibraryLPr(
                        PackageManager.SYSTEM_SHARED_LIBRARY_SHARED,
                        SharedLibraryInfo.VERSION_UNDEFINED);
            } else {
                mRequiredVerifierPackage = null;
                mRequiredInstallerPackage = null;
                mRequiredUninstallerPackage = null;
                mIntentFilterVerifierComponent = null;
                mIntentFilterVerifier = null;
                mServicesSystemSharedLibraryPackageName = null;
                mSharedSystemSharedLibraryPackageName = null;
            }

            mInstallerService = new PackageInstallerService(context, this);
            final Pair<ComponentName, String> instantAppResolverComponent =
                    getInstantAppResolverLPr();
            if (instantAppResolverComponent != null) {
                if (DEBUG_INSTANT) {
                    Slog.d(TAG, "Set ephemeral resolver: " + instantAppResolverComponent);
                }
                mInstantAppResolverConnection = new InstantAppResolverConnection(
                        mContext, instantAppResolverComponent.first,
                        instantAppResolverComponent.second);
                mInstantAppResolverSettingsComponent =
                        getInstantAppResolverSettingsLPr(instantAppResolverComponent.first);
            } else {
                mInstantAppResolverConnection = null;
                mInstantAppResolverSettingsComponent = null;
            }
            updateInstantAppInstallerLocked(null);

            // Read and update the usage of dex files.
            // Do this at the end of PM init so that all the packages have their
            // data directory reconciled.
            // At this point we know the code paths of the packages, so we can validate
            // the disk file and build the internal cache.
            // The usage file is expected to be small so loading and verifying it
            // should take a fairly small time compare to the other activities (e.g. package
            // scanning).
            final Map<Integer, List<PackageInfo>> userPackages = new HashMap<>();
            final int[] currentUserIds = UserManagerService.getInstance().getUserIds();
            for (int userId : currentUserIds) {
                userPackages.put(userId, getInstalledPackages(/*flags*/ 0, userId).getList());
            }
            mDexManager.load(userPackages);
            if (mIsUpgrade) {
                MetricsLogger.histogram(null, "ota_package_manager_init_time",
                        (int) (SystemClock.uptimeMillis() - startTime));
            }
        } // synchronized (mPackages)
        } // synchronized (mInstallLock)

        // Now after opening every single application zip, make sure they
        // are all flushed.  Not really needed, but keeps things nice and
        // tidy.
        Trace.traceBegin(TRACE_TAG_PACKAGE_MANAGER, "GC");
        Runtime.getRuntime().gc();
        Trace.traceEnd(TRACE_TAG_PACKAGE_MANAGER);

        // The initial scanning above does many calls into installd while
        // holding the mPackages lock, but we're mostly interested in yelling
        // once we have a booted system.
        mInstaller.setWarnIfHeld(mPackages);

        Trace.traceEnd(TRACE_TAG_PACKAGE_MANAGER);
    }


    private void enableSystemUserPackages() {
        if (!UserManager.isSplitSystemUser()) {
            return;
        }
        // For system user, enable apps based on the following conditions:
        // - app is whitelisted or belong to one of these groups:
        //   -- system app which has no launcher icons
        //   -- system app which has INTERACT_ACROSS_USERS permission
        //   -- system IME app
        // - app is not in the blacklist
        AppsQueryHelper queryHelper = new AppsQueryHelper(this);
        Set<String> enableApps = new ArraySet<>();
        enableApps.addAll(queryHelper.queryApps(AppsQueryHelper.GET_NON_LAUNCHABLE_APPS
                | AppsQueryHelper.GET_APPS_WITH_INTERACT_ACROSS_USERS_PERM
                | AppsQueryHelper.GET_IMES, /* systemAppsOnly */ true, UserHandle.SYSTEM));
        ArraySet<String> wlApps = SystemConfig.getInstance().getSystemUserWhitelistedApps();
        enableApps.addAll(wlApps);
        enableApps.addAll(queryHelper.queryApps(AppsQueryHelper.GET_REQUIRED_FOR_SYSTEM_USER,/* systemAppsOnly */ false, UserHandle.SYSTEM));
        ArraySet<String> blApps = SystemConfig.getInstance().getSystemUserBlacklistedApps();
        enableApps.removeAll(blApps);
        Log.i(TAG, "Applications installed for system user: " + enableApps);
        List<String> allAps = queryHelper.queryApps(0, /* systemAppsOnly */ false,UserHandle.SYSTEM);
        final int allAppsSize = allAps.size();
        synchronized (mPackages) {
            for (int i = 0; i < allAppsSize; i++) {
                String pName = allAps.get(i);
                PackageSetting pkgSetting = mSettings.mPackages.get(pName);
                // Should not happen, but we shouldn't be failing if it does
                if (pkgSetting == null) {
                    continue;
                }
                boolean install = enableApps.contains(pName);
                if (pkgSetting.getInstalled(UserHandle.USER_SYSTEM) != install) {
                    Log.i(TAG, (install ? "Installing " : "Uninstalling ") + pName + " for system user");
                    pkgSetting.setInstalled(install, UserHandle.USER_SYSTEM);
                }
            }
            scheduleWritePackageRestrictionsLocked(UserHandle.USER_SYSTEM);
        }
    }



   public boolean hasSystemFeature(String name, int version) {
        // allow instant applications
        synchronized (mAvailableFeatures) {
            final FeatureInfo feat = mAvailableFeatures.get(name); //PackageManagerService 启动时，从系统配置文件中读取，默认version 为 0
            if (feat == null) {
                return false;
            } else {
                return feat.version >= version;
            }
        }
    }




    private void scanDirTracedLI(File dir, final int parseFlags, int scanFlags, long currentTime) {
        Trace.traceBegin(TRACE_TAG_PACKAGE_MANAGER, "scanDir [" + dir.getAbsolutePath() + "]");
        try {
            scanDirLI(dir, parseFlags, scanFlags, currentTime);
        } finally {
            Trace.traceEnd(TRACE_TAG_PACKAGE_MANAGER);
        }
    }


     

    private void scanDirLI(File dir, int parseFlags, int scanFlags, long currentTime) {
        final File[] files = dir.listFiles();
        if (ArrayUtils.isEmpty(files)) {
            Log.d(TAG, "No files in app dir " + dir);
            return;
        }

        if (DEBUG_PACKAGE_SCANNING) {
            Log.d(TAG, "Scanning app dir " + dir + " scanFlags=" + scanFlags + " flags=0x" + Integer.toHexString(parseFlags));
        }
        ParallelPackageParser parallelPackageParser = new ParallelPackageParser(mSeparateProcesses, mOnlyCore, mMetrics, mCacheDir,mParallelPackageParserCallback);

        // Submit files for parsing in parallel
        int fileCount = 0;
        for (File file : files) {
            final boolean isPackage = (isApkFile(file) || file.isDirectory()) && !PackageInstallerService.isStageName(file.getName());
            if (!isPackage) {
                // Ignore entries which are not packages
                continue;
            }
            parallelPackageParser.submit(file, parseFlags);
            fileCount++;
        }

        // Process results one by one
        for (; fileCount > 0; fileCount--) {
            ParallelPackageParser.ParseResult parseResult = parallelPackageParser.take();
            Throwable throwable = parseResult.throwable;
            int errorCode = PackageManager.INSTALL_SUCCEEDED;

            if (throwable == null) {
                // Static shared libraries have synthetic package names
                if (parseResult.pkg.applicationInfo.isStaticSharedLibrary()) {
                    renameStaticSharedLibraryPackage(parseResult.pkg);
                }
                try {
                    if (errorCode == PackageManager.INSTALL_SUCCEEDED) {
                        scanPackageLI(parseResult.pkg, parseResult.scanFile, parseFlags, scanFlags,currentTime, null);
                    }
                } catch (PackageManagerException e) {
                    errorCode = e.error;
                    Slog.w(TAG, "Failed to scan " + parseResult.scanFile + ": " + e.getMessage());
                }
            } else if (throwable instanceof PackageParser.PackageParserException) {
                PackageParser.PackageParserException e = (PackageParser.PackageParserException) throwable;
                errorCode = e.error;
                Slog.w(TAG, "Failed to parse " + parseResult.scanFile + ": " + e.getMessage());
            } else {
                throw new IllegalStateException("Unexpected exception occurred while parsing "  + parseResult.scanFile, throwable);
            }

            // Delete invalid userdata apps
            if ((parseFlags & PackageParser.PARSE_IS_SYSTEM) == 0 &&  errorCode == PackageManager.INSTALL_FAILED_INVALID_APK) {
                logCriticalInfo(Log.WARN, "Deleting invalid package at " + parseResult.scanFile);
                removeCodePathLI(parseResult.scanFile);
            }
        }
        parallelPackageParser.close();
    }


    public static boolean isStageName(String name) {
        final boolean isFile = name.startsWith("vmdl") && name.endsWith(".tmp");
        final boolean isContainer = name.startsWith("smdl") && name.endsWith(".tmp");
        final boolean isLegacyContainer = name.startsWith("smdl2tmp");
        return isFile || isContainer || isLegacyContainer;
    }


      /**
     *  Scans a package and returns the newly parsed package.
     *  @throws PackageManagerException on a parse error.
     */
    private PackageParser.Package scanPackageLI(PackageParser.Package pkg, File scanFile,final int policyFlags, int scanFlags, long currentTime, @Nullable UserHandle user)  throws PackageManagerException {
        // If the package has children and this is the first dive in the function
        // we scan the package with the SCAN_CHECK_ONLY flag set to see whether all
        // packages (parent and children) would be successfully scanned before the
        // actual scan since scanning mutates internal state and we want to atomically
        // install the package and its children.
        if ((scanFlags & SCAN_CHECK_ONLY) == 0) {
            if (pkg.childPackages != null && pkg.childPackages.size() > 0) {
                scanFlags |= SCAN_CHECK_ONLY;
            }
        } else {
            scanFlags &= ~SCAN_CHECK_ONLY;
        }

        // Scan the parent
        PackageParser.Package scannedPkg = scanPackageInternalLI(pkg, scanFile, policyFlags,scanFlags, currentTime, user);

        // Scan the children
        final int childCount = (pkg.childPackages != null) ? pkg.childPackages.size() : 0;
        for (int i = 0; i < childCount; i++) {
            PackageParser.Package childPackage = pkg.childPackages.get(i);
            scanPackageInternalLI(childPackage, scanFile, policyFlags, scanFlags,currentTime, user);
        }


        if ((scanFlags & SCAN_CHECK_ONLY) != 0) {
            return scanPackageLI(pkg, scanFile, policyFlags, scanFlags, currentTime, user);
        }

        return scannedPkg;
    }



      /**
     *  Scans a package and returns the newly parsed package.
     *  @throws PackageManagerException on a parse error.
     */
    private PackageParser.Package scanPackageInternalLI(PackageParser.Package pkg, File scanFile,int policyFlags, int scanFlags, long currentTime, @Nullable UserHandle user) throws PackageManagerException {
        PackageSetting ps = null;
        PackageSetting updatedPkg;
        // reader
        synchronized (mPackages) {
            // Look to see if we already know about this package.
            String oldName = mSettings.getRenamedPackageLPr(pkg.packageName);
            if (pkg.mOriginalPackages != null && pkg.mOriginalPackages.contains(oldName)) {
                // This package has been renamed to its original name.  Let's
                // use that.
                ps = mSettings.getPackageLPr(oldName);
            }
            // If there was no original package, see one for the real package name.
            if (ps == null) {
                ps = mSettings.getPackageLPr(pkg.packageName);
            }
            // Check to see if this package could be hiding/updating a system
            // package.  Must look for it either under the original or real
            // package name depending on our state.
            updatedPkg = mSettings.getDisabledSystemPkgLPr(ps != null ? ps.name : pkg.packageName);
            if (DEBUG_INSTALL && updatedPkg != null) Slog.d(TAG, "updatedPkg = " + updatedPkg);

            // If this is a package we don't know about on the system partition, we
            // may need to remove disabled child packages on the system partition
            // or may need to not add child packages if the parent apk is updated
            // on the data partition and no longer defines this child package.
            if ((policyFlags & PackageParser.PARSE_IS_SYSTEM) != 0) {
                // If this is a parent package for an updated system app and this system
                // app got an OTA update which no longer defines some of the child packages
                // we have to prune them from the disabled system packages.
                PackageSetting disabledPs = mSettings.getDisabledSystemPkgLPr(pkg.packageName);
                if (disabledPs != null) {
                    final int scannedChildCount = (pkg.childPackages != null) ? pkg.childPackages.size() : 0;
                    final int disabledChildCount = disabledPs.childPackageNames != null ? disabledPs.childPackageNames.size() : 0;
                    for (int i = 0; i < disabledChildCount; i++) {
                        String disabledChildPackageName = disabledPs.childPackageNames.get(i);
                        boolean disabledPackageAvailable = false;
                        for (int j = 0; j < scannedChildCount; j++) {
                            PackageParser.Package childPkg = pkg.childPackages.get(j);
                            if (childPkg.packageName.equals(disabledChildPackageName)) {
                                disabledPackageAvailable = true;
                                break;
                            }
                         }
                         if (!disabledPackageAvailable) {
                             mSettings.removeDisabledSystemPackageLPw(disabledChildPackageName);
                         }
                    }
                }
            }
        }

        final boolean isUpdatedPkg = updatedPkg != null;
        final boolean isUpdatedSystemPkg = isUpdatedPkg && (policyFlags & PackageParser.PARSE_IS_SYSTEM) != 0;
        boolean isUpdatedPkgBetter = false;
        // First check if this is a system package that may involve an update
        if (isUpdatedSystemPkg) {
            // If new package is not located in "/system/priv-app" (e.g. due to an OTA),
            // it needs to drop FLAG_PRIVILEGED.
            if (locationIsPrivileged(scanFile)) {
                updatedPkg.pkgPrivateFlags |= ApplicationInfo.PRIVATE_FLAG_PRIVILEGED;
            } else {
                updatedPkg.pkgPrivateFlags &= ~ApplicationInfo.PRIVATE_FLAG_PRIVILEGED;
            }

            if (ps != null && !ps.codePath.equals(scanFile)) {
                // The path has changed from what was last scanned...  check the
                // version of the new path against what we have stored to determine
                // what to do.
                if (DEBUG_INSTALL) Slog.d(TAG, "Path changing from " + ps.codePath);
                if (pkg.mVersionCode <= ps.versionCode) {
                    // The system package has been updated and the code path does not match
                    // Ignore entry. Skip it.
                    if (DEBUG_INSTALL) Slog.i(TAG, "Package " + ps.name + " at " + scanFile
                            + " ignored: updated version " + ps.versionCode
                            + " better than this " + pkg.mVersionCode);
                    if (!updatedPkg.codePath.equals(scanFile)) {
                        Slog.w(PackageManagerService.TAG, "Code path for hidden system pkg "
                                + ps.name + " changing from " + updatedPkg.codePathString
                                + " to " + scanFile);
                        updatedPkg.codePath = scanFile;
                        updatedPkg.codePathString = scanFile.toString();
                        updatedPkg.resourcePath = scanFile;
                        updatedPkg.resourcePathString = scanFile.toString();
                    }
                    updatedPkg.pkg = pkg;
                    updatedPkg.versionCode = pkg.mVersionCode;

                    // Update the disabled system child packages to point to the package too.
                    final int childCount = updatedPkg.childPackageNames != null
                            ? updatedPkg.childPackageNames.size() : 0;
                    for (int i = 0; i < childCount; i++) {
                        String childPackageName = updatedPkg.childPackageNames.get(i);
                        PackageSetting updatedChildPkg = mSettings.getDisabledSystemPkgLPr(
                                childPackageName);
                        if (updatedChildPkg != null) {
                            updatedChildPkg.pkg = pkg;
                            updatedChildPkg.versionCode = pkg.mVersionCode;
                        }
                    }
                } else {
                    // The current app on the system partition is better than
                    // what we have updated to on the data partition; switch
                    // back to the system partition version.
                    // At this point, its safely assumed that package installation for
                    // apps in system partition will go through. If not there won't be a working
                    // version of the app
                    // writer
                    synchronized (mPackages) {
                        // Just remove the loaded entries from package lists.
                        mPackages.remove(ps.name);
                    }

                    logCriticalInfo(Log.WARN, "Package " + ps.name + " at " + scanFile
                            + " reverting from " + ps.codePathString
                            + ": new version " + pkg.mVersionCode
                            + " better than installed " + ps.versionCode);

                    InstallArgs args = createInstallArgsForExisting(packageFlagsToInstallFlags(ps),
                            ps.codePathString, ps.resourcePathString, getAppDexInstructionSets(ps));
                    synchronized (mInstallLock) {
                        args.cleanUpResourcesLI();
                    }
                    synchronized (mPackages) {
                        mSettings.enableSystemPackageLPw(ps.name);
                    }
                    isUpdatedPkgBetter = true;
                }
            }
        }

        String resourcePath = null;
        String baseResourcePath = null;
        if ((policyFlags & PackageParser.PARSE_FORWARD_LOCK) != 0 && !isUpdatedPkgBetter) {
            if (ps != null && ps.resourcePathString != null) {
                resourcePath = ps.resourcePathString;
                baseResourcePath = ps.resourcePathString;
            } else {
                // Should not happen at all. Just log an error.
                Slog.e(TAG, "Resource path not set for package " + pkg.packageName);
            }
        } else {
            resourcePath = pkg.codePath;
            baseResourcePath = pkg.baseCodePath;
        }

        // Set application objects path explicitly.
        pkg.setApplicationVolumeUuid(pkg.volumeUuid);
        pkg.setApplicationInfoCodePath(pkg.codePath);
        pkg.setApplicationInfoBaseCodePath(pkg.baseCodePath);
        pkg.setApplicationInfoSplitCodePaths(pkg.splitCodePaths);
        pkg.setApplicationInfoResourcePath(resourcePath);
        pkg.setApplicationInfoBaseResourcePath(baseResourcePath);
        pkg.setApplicationInfoSplitResourcePaths(pkg.splitCodePaths);

        // throw an exception if we have an update to a system application, but, it's not more
        // recent than the package we've already scanned
        if (isUpdatedSystemPkg && !isUpdatedPkgBetter) {
            throw new PackageManagerException(Log.WARN, "Package " + ps.name + " at "
                    + scanFile + " ignored: updated version " + ps.versionCode
                    + " better than this " + pkg.mVersionCode);
        }

        if (isUpdatedPkg) {
            // An updated system app will not have the PARSE_IS_SYSTEM flag set
            // initially
            policyFlags |= PackageParser.PARSE_IS_SYSTEM;

            // An updated privileged app will not have the PARSE_IS_PRIVILEGED
            // flag set initially
            if ((updatedPkg.pkgPrivateFlags & ApplicationInfo.PRIVATE_FLAG_PRIVILEGED) != 0) {
                policyFlags |= PackageParser.PARSE_IS_PRIVILEGED;
            }
        }

        // Verify certificates against what was last scanned
        collectCertificatesLI(ps, pkg, scanFile, policyFlags);

        /*
         * A new system app appeared, but we already had a non-system one of the
         * same name installed earlier.
         */
        boolean shouldHideSystemApp = false;
        if (!isUpdatedPkg && ps != null
                && (policyFlags & PackageParser.PARSE_IS_SYSTEM_DIR) != 0 && !isSystemApp(ps)) {
            /*
             * Check to make sure the signatures match first. If they don't,
             * wipe the installed application and its data.
             */
            if (compareSignatures(ps.signatures.mSignatures, pkg.mSignatures)
                    != PackageManager.SIGNATURE_MATCH) {
                logCriticalInfo(Log.WARN, "Package " + ps.name + " appeared on system, but"
                        + " signatures don't match existing userdata copy; removing");
                try (PackageFreezer freezer = freezePackage(pkg.packageName,
                        "scanPackageInternalLI")) {
                    deletePackageLIF(pkg.packageName, null, true, null, 0, null, false, null);
                }
                ps = null;
            } else {
                /*
                 * If the newly-added system app is an older version than the
                 * already installed version, hide it. It will be scanned later
                 * and re-added like an update.
                 */
                if (pkg.mVersionCode <= ps.versionCode) {
                    shouldHideSystemApp = true;
                    logCriticalInfo(Log.INFO, "Package " + ps.name + " appeared at " + scanFile
                            + " but new version " + pkg.mVersionCode + " better than installed "
                            + ps.versionCode + "; hiding system");
                } else {
                    /*
                     * The newly found system app is a newer version that the
                     * one previously installed. Simply remove the
                     * already-installed application and replace it with our own
                     * while keeping the application data.
                     */
                    logCriticalInfo(Log.WARN, "Package " + ps.name + " at " + scanFile
                            + " reverting from " + ps.codePathString + ": new version "
                            + pkg.mVersionCode + " better than installed " + ps.versionCode);
                    InstallArgs args = createInstallArgsForExisting(packageFlagsToInstallFlags(ps), ps.codePathString, ps.resourcePathString, getAppDexInstructionSets(ps));
                    synchronized (mInstallLock) {
                        args.cleanUpResourcesLI();
                    }
                }
            }
        }

        // The apk is forward locked (not public) if its code and resources
        // are kept in different files. (except for app in either system or
        // vendor path).
        // TODO grab this value from PackageSettings
        if ((policyFlags & PackageParser.PARSE_IS_SYSTEM_DIR) == 0) {
            if (ps != null && !ps.codePath.equals(ps.resourcePath)) {
                policyFlags |= PackageParser.PARSE_FORWARD_LOCK;
            }
        }

        final int userId = ((user == null) ? 0 : user.getIdentifier());
        if (ps != null && ps.getInstantApp(userId)) {
            scanFlags |= SCAN_AS_INSTANT_APP;
        }

        // Note that we invoke the following method only if we are about to unpack an application
        PackageParser.Package scannedPkg = scanPackageLI(pkg, policyFlags, scanFlags | SCAN_UPDATE_SIGNATURE, currentTime, user);

        /*
         * If the system app should be overridden by a previously installed
         * data, hide the system app now and let the /data/app scan pick it up
         * again.
         */
        if (shouldHideSystemApp) {
            synchronized (mPackages) {
                mSettings.disableSystemPackageLPw(pkg.packageName, true);
            }
        }

        return scannedPkg;
    }




     /**
     * Create args that describe an existing installed package. Typically used
     * when cleaning up old installs, or used as a move source.
     */
    private InstallArgs createInstallArgsForExisting(int installFlags, String codePath,String resourcePath, String[] instructionSets) {
        final boolean isInAsec;
        if (installOnExternalAsec(installFlags)) {
            /* Apps on SD card are always in ASEC containers. */
            isInAsec = true;
        } else if (installForwardLocked(installFlags)   && !codePath.startsWith(mDrmAppPrivateInstallDir.getAbsolutePath())) {
            /*
             * Forward-locked apps are only in ASEC containers if they're the
             * new style
             */
            isInAsec = true;
        } else {
            isInAsec = false;
        }

        if (isInAsec) {
            return new AsecInstallArgs(codePath, instructionSets, installOnExternalAsec(installFlags), installForwardLocked(installFlags));
        } else {
            return new FileInstallArgs(codePath, resourcePath, instructionSets);
        }
    }

}
