/**
 * Parser for package files (APKs) on disk. This supports apps packaged either
 * as a single "monolithic" APK, or apps packaged as a "cluster" of multiple
 * APKs in a single directory.
 * <p>
 * Apps packaged as multiple APKs always consist of a single "base" APK (with a
 * {@code null} split name) and zero or more "split" APKs (with unique split
 * names). Any subset of those split APKs are a valid install, as long as the
 * following constraints are met:
 * <ul>
 * <li>All APKs must have the exact same package name, version code, and signing
 * certificates.
 * <li>All APKs must have unique split names.
 * <li>All installations must contain a single base APK.
 * </ul>
 *
 * @hide
 */
public class PackageParser {
	 public void setSeparateProcesses(String[] procs) {
        mSeparateProcesses = procs;
    }

    /**
     * Flag indicating this parser should only consider apps with
     * {@code coreApp} manifest attribute to be valid apps. This is useful when
     * creating a minimalist boot environment.
     */
    public void setOnlyCoreApps(boolean onlyCoreApps) {
        mOnlyCoreApps = onlyCoreApps;
    }

    public void setDisplayMetrics(DisplayMetrics metrics) {
        mMetrics = metrics;
    }

    /**
     * Sets the cache directory for this package parser.
     */
    public void setCacheDir(File cacheDir) {
        mCacheDir = cacheDir;
    }



     /**
     * Parse the package at the given location. Automatically detects if the
     * package is a monolithic style (single APK file) or cluster style
     * (directory of APKs).
     * <p>
     * This performs sanity checking on cluster style packages, such as
     * requiring identical package name and version codes, a single base APK,
     * and unique split names.
     * <p>
     * Note that this <em>does not</em> perform signature verification; that
     * must be done separately in {@link #collectCertificates(Package, int)}.
     *
     * If {@code useCaches} is true, the package parser might return a cached
     * result from a previous parse of the same {@code packageFile} with the same
     * {@code flags}. Note that this method does not check whether {@code packageFile}
     * has changed since the last parse, it's up to callers to do so.
     *
     * @see #parsePackageLite(File, int)
     */
    public Package parsePackage(File packageFile, int flags, boolean useCaches = true)throws PackageParserException {
        Package parsed = useCaches ? getCachedResult(packageFile, flags) : null; //由于系统默认 mCacheDir为null，所以getCachedResult 返回null
        if (parsed != null) {
            return parsed;
        }

        if (packageFile.isDirectory()) {
            parsed = parseClusterPackage(packageFile, flags);
        } else {
            parsed = parseMonolithicPackage(packageFile, flags);//执行这里
        }

        cacheResult(packageFile, flags, parsed);//同理由于系统默认 mCacheDir为null，所以cacheResult 什么也不做

        return parsed;
    }

     /**
     * Parse all APKs contained in the given directory, treating them as a
     * single package. This also performs sanity checking, such as requiring
     * identical package name and version codes, a single base APK, and unique
     * split names.
     * <p>
     * Note that this <em>does not</em> perform signature verification; that
     * must be done separately in {@link #collectCertificates(Package, int)}.
     */
    private Package parseClusterPackage(File packageDir, int flags) throws PackageParserException {
        final PackageLite lite = parseClusterPackageLite(packageDir, 0);
        if (mOnlyCoreApps && !lite.coreApp) {
            throw new PackageParserException(INSTALL_PARSE_FAILED_MANIFEST_MALFORMED,"Not a coreApp: " + packageDir);
        }

        // Build the split dependency tree.
        SparseArray<int[]> splitDependencies = null;
        final SplitAssetLoader assetLoader;
        if (lite.isolatedSplits && !ArrayUtils.isEmpty(lite.splitNames)) {
            try {
                splitDependencies = SplitAssetDependencyLoader.createDependenciesFromPackage(lite);
                assetLoader = new SplitAssetDependencyLoader(lite, splitDependencies, flags);
            } catch (SplitAssetDependencyLoader.IllegalDependencyException e) {
                throw new PackageParserException(INSTALL_PARSE_FAILED_BAD_MANIFEST, e.getMessage());
            }
        } else {
            assetLoader = new DefaultSplitAssetLoader(lite, flags);
        }

        try {
            final AssetManager assets = assetLoader.getBaseAssetManager();
            final File baseApk = new File(lite.baseCodePath);
            final Package pkg = parseBaseApk(baseApk, assets, flags);
            if (pkg == null) {
                throw new PackageParserException(INSTALL_PARSE_FAILED_NOT_APK,"Failed to parse base APK: " + baseApk);
            }

            if (!ArrayUtils.isEmpty(lite.splitNames)) {
                final int num = lite.splitNames.length;
                pkg.splitNames = lite.splitNames;
                pkg.splitCodePaths = lite.splitCodePaths;
                pkg.splitRevisionCodes = lite.splitRevisionCodes;
                pkg.splitFlags = new int[num];
                pkg.splitPrivateFlags = new int[num];
                pkg.applicationInfo.splitNames = pkg.splitNames;
                pkg.applicationInfo.splitDependencies = splitDependencies;

                for (int i = 0; i < num; i++) {
                    final AssetManager splitAssets = assetLoader.getSplitAssetManager(i);
                    parseSplitApk(pkg, i, splitAssets, flags);
                }
            }

            pkg.setCodePath(packageDir.getAbsolutePath());
            pkg.setUse32bitAbi(lite.use32bitAbi);
            return pkg;
        } finally {
            IoUtils.closeQuietly(assetLoader);
        }
    }


    private static AssetManager newConfiguredAssetManager() {
        AssetManager assetManager = new AssetManager();
        assetManager.setConfiguration(0, 0, null, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,Build.VERSION.RESOURCES_SDK_INT);
        return assetManager;
    }

    /**
     * Parse the given APK file, treating it as as a single monolithic package.
     * <p>
     * Note that this <em>does not</em> perform signature verification; that
     * must be done separately in {@link #collectCertificates(Package, int)}.
     *
     * @deprecated external callers should move to
     *             {@link #parsePackage(File, int)}. Eventually this method will
     *             be marked private.
     */
    @Deprecated
    public Package parseMonolithicPackage(File apkFile, int flags) throws PackageParserException {
        final AssetManager assets = newConfiguredAssetManager();//创建AssetManager并且初始化
        final PackageLite lite = parseMonolithicPackageLite(apkFile, flags);
        if (mOnlyCoreApps) {
            if (!lite.coreApp) {
                throw new PackageParserException(INSTALL_PARSE_FAILED_MANIFEST_MALFORMED,"Not a coreApp: " + apkFile);
            }
        }

        try {
            final Package pkg = parseBaseApk(apkFile, assets, flags);
            pkg.setCodePath(apkFile.getAbsolutePath());
            pkg.setUse32bitAbi(lite.use32bitAbi);
            return pkg;
        } finally {
            IoUtils.closeQuietly(assets);
        }
    }


    private Package parseBaseApk(File apkFile, AssetManager assets, int flags) throws PackageParserException {
        final String apkPath = apkFile.getAbsolutePath();

        String volumeUuid = null;
        if (apkPath.startsWith(MNT_EXPAND)) { //MNT_EXPAND = "/mnt/expand/";
            final int end = apkPath.indexOf('/', MNT_EXPAND.length());
            volumeUuid = apkPath.substring(MNT_EXPAND.length(), end);
        }

        mParseError = PackageManager.INSTALL_SUCCEEDED;
        mArchiveSourcePath = apkFile.getAbsolutePath();

        if (DEBUG_JAR) Slog.d(TAG, "Scanning base APK: " + apkPath);

        final int cookie = loadApkIntoAssetManager(assets, apkPath, flags);

        Resources res = null;
        XmlResourceParser parser = null;
        try {
            res = new Resources(assets, mMetrics, null);
            parser = assets.openXmlResourceParser(cookie, ANDROID_MANIFEST_FILENAME);  //ANDROID_MANIFEST_FILENAME = "AndroidManifest.xml"

            final String[] outError = new String[1];
            final Package pkg = parseBaseApk(apkPath, res, parser, flags, outError);
            if (pkg == null) {
                throw new PackageParserException(mParseError,apkPath + " (at " + parser.getPositionDescription() + "): " + outError[0]);
            }

            pkg.setVolumeUuid(volumeUuid);
            pkg.setApplicationVolumeUuid(volumeUuid);
            pkg.setBaseCodePath(apkPath);
            pkg.setSignatures(null);

            return pkg;

        } catch (PackageParserException e) {
            throw e;
        } catch (Exception e) {
            throw new PackageParserException(INSTALL_PARSE_FAILED_UNEXPECTED_EXCEPTION,"Failed to read manifest from " + apkPath, e);
        } finally {
            IoUtils.closeQuietly(parser);
        }
    }


    /**
     * Parse the manifest of a <em>base APK</em>. When adding new features you
     * need to consider whether they should be supported by split APKs and child
     * packages.
     *
     * @param apkPath The package apk file path
     * @param res The resources from which to resolve values
     * @param parser The manifest parser
     * @param flags Flags how to parse
     * @param outError Human readable error message
     * @return Parsed package or null on error.
     *
     * @throws XmlPullParserException
     * @throws IOException
     */
    private Package parseBaseApk(String apkPath, Resources res, XmlResourceParser parser, int flags, String[] outError) throws XmlPullParserException, IOException {
        final String splitName;
        final String pkgName;

        try {
            Pair<String, String> packageSplit = parsePackageSplitNames(parser, parser); //获取包名
            pkgName = packageSplit.first;
            splitName = packageSplit.second;//null

            if (!TextUtils.isEmpty(splitName)) {
                outError[0] = "Expected base APK, but found split " + splitName;
                mParseError = PackageManager.INSTALL_PARSE_FAILED_BAD_PACKAGE_NAME;
                return null;
            }
        } catch (PackageParserException e) {
            mParseError = PackageManager.INSTALL_PARSE_FAILED_BAD_PACKAGE_NAME;
            return null;
        }

        if (mCallback != null) {
            String[] overlayPaths = mCallback.getOverlayPaths(pkgName, apkPath);
            if (overlayPaths != null && overlayPaths.length > 0) {
                for (String overlayPath : overlayPaths) {
                    res.getAssets().addOverlayPath(overlayPath);
                }
            }
        }

        final Package pkg = new Package(pkgName);

        TypedArray sa = res.obtainAttributes(parser,com.android.internal.R.styleable.AndroidManifest);

        pkg.mVersionCode = pkg.applicationInfo.versionCode = sa.getInteger( com.android.internal.R.styleable.AndroidManifest_versionCode, 0);
        pkg.baseRevisionCode = sa.getInteger(com.android.internal.R.styleable.AndroidManifest_revisionCode, 0);
        pkg.mVersionName = sa.getNonConfigurationString(com.android.internal.R.styleable.AndroidManifest_versionName, 0);
        if (pkg.mVersionName != null) {
            pkg.mVersionName = pkg.mVersionName.intern();
        }

        pkg.coreApp = parser.getAttributeBooleanValue(null, "coreApp", false);

        sa.recycle();

        return parseBaseApkCommon(pkg, null, res, parser, flags, outError);
    }



     /**
     * This is the common parsing routing for handling parent and child
     * packages in a base APK. The difference between parent and child
     * parsing is that some tags are not supported by child packages as
     * well as some manifest attributes are ignored. The implementation
     * assumes the calling code has already handled the manifest tag if needed
     * (this applies to the parent only).
     *
     * @param pkg The package which to populate
     * @param acceptedTags Which tags to handle, null to handle all
     * @param res Resources against which to resolve values
     * @param parser Parser of the manifest
     * @param flags Flags about how to parse
     * @param outError Human readable error if parsing fails
     * @return The package if parsing succeeded or null.
     *
     * @throws XmlPullParserException
     * @throws IOException
     */
    private Package parseBaseApkCommon(Package pkg, Set<String> acceptedTags, Resources res,XmlResourceParser parser, int flags, String[] outError) throws XmlPullParserException,IOException {
        mParseInstrumentationArgs = null;
        mParseActivityArgs = null;
        mParseServiceArgs = null;
        mParseProviderArgs = null;

        int type;
        boolean foundApp = false;

        TypedArray sa = res.obtainAttributes(parser,com.android.internal.R.styleable.AndroidManifest);

        String str = sa.getNonConfigurationString(om.android.internal.R.styleable.AndroidManifest_sharedUserId, 0);
        if (str != null && str.length() > 0) {
            if ((flags & PARSE_IS_EPHEMERAL) != 0) {
                outError[0] = "sharedUserId not allowed in ephemeral application";
                mParseError = PackageManager.INSTALL_PARSE_FAILED_BAD_SHARED_USER_ID;
                return null;
            }
            String nameError = validateName(str, true, false);
            if (nameError != null && !"android".equals(pkg.packageName)) {
                outError[0] = "<manifest> specifies bad sharedUserId name \""  + str + "\": " + nameError;
                mParseError = PackageManager.INSTALL_PARSE_FAILED_BAD_SHARED_USER_ID;
                return null;
            }
            pkg.mSharedUserId = str.intern();
            pkg.mSharedUserLabel = sa.getResourceId( com.android.internal.R.styleable.AndroidManifest_sharedUserLabel, 0);
        }

        pkg.installLocation = sa.getInteger(com.android.internal.R.styleable.AndroidManifest_installLocation,PARSE_DEFAULT_INSTALL_LOCATION);
        pkg.applicationInfo.installLocation = pkg.installLocation;

        final int targetSandboxVersion = sa.getInteger(com.android.internal.R.styleable.AndroidManifest_targetSandboxVersion,PARSE_DEFAULT_TARGET_SANDBOX);
        pkg.applicationInfo.targetSandboxVersion = targetSandboxVersion;

        /* Set the global "forward lock" flag */
        if ((flags & PARSE_FORWARD_LOCK) != 0) {
            pkg.applicationInfo.privateFlags |= ApplicationInfo.PRIVATE_FLAG_FORWARD_LOCK;
        }

        /* Set the global "on SD card" flag */
        if ((flags & PARSE_EXTERNAL_STORAGE) != 0) {
            pkg.applicationInfo.flags |= ApplicationInfo.FLAG_EXTERNAL_STORAGE;
        }

        if (sa.getBoolean(com.android.internal.R.styleable.AndroidManifest_isolatedSplits, false)) {
            pkg.applicationInfo.privateFlags |= ApplicationInfo.PRIVATE_FLAG_ISOLATED_SPLIT_LOADING;
        }

        // Resource boolean are -1, so 1 means we don't know the value.
        int supportsSmallScreens = 1;
        int supportsNormalScreens = 1;
        int supportsLargeScreens = 1;
        int supportsXLargeScreens = 1;
        int resizeable = 1;
        int anyDensity = 1;

        int outerDepth = parser.getDepth();
        while ((type = parser.next()) != XmlPullParser.END_DOCUMENT && (type != XmlPullParser.END_TAG || parser.getDepth() > outerDepth)) {
            if (type == XmlPullParser.END_TAG || type == XmlPullParser.TEXT) {
                continue;
            }

            String tagName = parser.getName();

            if (acceptedTags != null && !acceptedTags.contains(tagName)) {
                Slog.w(TAG, "Skipping unsupported element under <manifest>: "
                        + tagName + " at " + mArchiveSourcePath + " "
                        + parser.getPositionDescription());
                XmlUtils.skipCurrentTag(parser);
                continue;
            }

            if (tagName.equals(TAG_APPLICATION)) {
                if (foundApp) {
                    if (RIGID_PARSER) {
                        outError[0] = "<manifest> has more than one <application>";
                        mParseError = PackageManager.INSTALL_PARSE_FAILED_MANIFEST_MALFORMED;
                        return null;
                    } else {
                        Slog.w(TAG, "<manifest> has more than one <application>");
                        XmlUtils.skipCurrentTag(parser);
                        continue;
                    }
                }

                foundApp = true;
                if (!parseBaseApplication(pkg, res, parser, flags, outError)) {
                    return null;
                }
            } else if (tagName.equals(TAG_OVERLAY)) {
                sa = res.obtainAttributes(parser,com.android.internal.R.styleable.AndroidManifestResourceOverlay);
                pkg.mOverlayTarget = sa.getString(com.android.internal.R.styleable.AndroidManifestResourceOverlay_targetPackage);
                pkg.mOverlayPriority = sa.getInt(com.android.internal.R.styleable.AndroidManifestResourceOverlay_priority,0);
                pkg.mIsStaticOverlay = sa.getBoolean(com.android.internal.R.styleable.AndroidManifestResourceOverlay_isStatic,false);
                final String propName = sa.getString(com.android.internal.R.styleable.AndroidManifestResourceOverlay_requiredSystemPropertyName);
                final String propValue = sa.getString(com.android.internal.R.styleable.AndroidManifestResourceOverlay_requiredSystemPropertyValue);
                sa.recycle();

                if (pkg.mOverlayTarget == null) {
                    outError[0] = "<overlay> does not specify a target package";
                    mParseError = PackageManager.INSTALL_PARSE_FAILED_MANIFEST_MALFORMED;
                    return null;
                }

                if (pkg.mOverlayPriority < 0 || pkg.mOverlayPriority > 9999) {
                    outError[0] = "<overlay> priority must be between 0 and 9999";
                    mParseError = PackageManager.INSTALL_PARSE_FAILED_MANIFEST_MALFORMED;
                    return null;
                }

                // check to see if overlay should be excluded based on system property condition
                if (!checkOverlayRequiredSystemProperty(propName, propValue)) {
                    Slog.i(TAG, "Skipping target and overlay pair " + pkg.mOverlayTarget + " and "
                        + pkg.baseCodePath+ ": overlay ignored due to required system property: "
                        + propName + " with value: " + propValue);
                    return null;
                }

                XmlUtils.skipCurrentTag(parser);

            } else if (tagName.equals(TAG_KEY_SETS)) {
                if (!parseKeySets(pkg, res, parser, outError)) {
                    return null;
                }
            } else if (tagName.equals(TAG_PERMISSION_GROUP)) {
                if (!parsePermissionGroup(pkg, flags, res, parser, outError)) {
                    return null;
                }
            } else if (tagName.equals(TAG_PERMISSION)) {
                if (!parsePermission(pkg, res, parser, outError)) {
                    return null;
                }
            } else if (tagName.equals(TAG_PERMISSION_TREE)) {
                if (!parsePermissionTree(pkg, res, parser, outError)) {
                    return null;
                }
            } else if (tagName.equals(TAG_USES_PERMISSION)) {
                if (!parseUsesPermission(pkg, res, parser)) {
                    return null;
                }
            } else if (tagName.equals(TAG_USES_PERMISSION_SDK_M) || tagName.equals(TAG_USES_PERMISSION_SDK_23)) {
                if (!parseUsesPermission(pkg, res, parser)) {
                    return null;
                }
            } else if (tagName.equals(TAG_USES_CONFIGURATION)) {
                ConfigurationInfo cPref = new ConfigurationInfo();
                sa = res.obtainAttributes(parser,
                        com.android.internal.R.styleable.AndroidManifestUsesConfiguration);
                cPref.reqTouchScreen = sa.getInt(
                        com.android.internal.R.styleable.AndroidManifestUsesConfiguration_reqTouchScreen,
                        Configuration.TOUCHSCREEN_UNDEFINED);
                cPref.reqKeyboardType = sa.getInt(
                        com.android.internal.R.styleable.AndroidManifestUsesConfiguration_reqKeyboardType,
                        Configuration.KEYBOARD_UNDEFINED);
                if (sa.getBoolean(
                        com.android.internal.R.styleable.AndroidManifestUsesConfiguration_reqHardKeyboard,
                        false)) {
                    cPref.reqInputFeatures |= ConfigurationInfo.INPUT_FEATURE_HARD_KEYBOARD;
                }
                cPref.reqNavigation = sa.getInt(
                        com.android.internal.R.styleable.AndroidManifestUsesConfiguration_reqNavigation,
                        Configuration.NAVIGATION_UNDEFINED);
                if (sa.getBoolean(
                        com.android.internal.R.styleable.AndroidManifestUsesConfiguration_reqFiveWayNav,
                        false)) {
                    cPref.reqInputFeatures |= ConfigurationInfo.INPUT_FEATURE_FIVE_WAY_NAV;
                }
                sa.recycle();
                pkg.configPreferences = ArrayUtils.add(pkg.configPreferences, cPref);

                XmlUtils.skipCurrentTag(parser);

            } else if (tagName.equals(TAG_USES_FEATURE)) {
                FeatureInfo fi = parseUsesFeature(res, parser);
                pkg.reqFeatures = ArrayUtils.add(pkg.reqFeatures, fi);

                if (fi.name == null) {
                    ConfigurationInfo cPref = new ConfigurationInfo();
                    cPref.reqGlEsVersion = fi.reqGlEsVersion;
                    pkg.configPreferences = ArrayUtils.add(pkg.configPreferences, cPref);
                }

                XmlUtils.skipCurrentTag(parser);

            } else if (tagName.equals(TAG_FEATURE_GROUP)) {
                FeatureGroupInfo group = new FeatureGroupInfo();
                ArrayList<FeatureInfo> features = null;
                final int innerDepth = parser.getDepth();
                while ((type = parser.next()) != XmlPullParser.END_DOCUMENT
                        && (type != XmlPullParser.END_TAG || parser.getDepth() > innerDepth)) {
                    if (type == XmlPullParser.END_TAG || type == XmlPullParser.TEXT) {
                        continue;
                    }

                    final String innerTagName = parser.getName();
                    if (innerTagName.equals("uses-feature")) {
                        FeatureInfo featureInfo = parseUsesFeature(res, parser);
                        // FeatureGroups are stricter and mandate that
                        // any <uses-feature> declared are mandatory.
                        featureInfo.flags |= FeatureInfo.FLAG_REQUIRED;
                        features = ArrayUtils.add(features, featureInfo);
                    } else {
                        Slog.w(TAG, "Unknown element under <feature-group>: " + innerTagName +
                                " at " + mArchiveSourcePath + " " +
                                parser.getPositionDescription());
                    }
                    XmlUtils.skipCurrentTag(parser);
                }

                if (features != null) {
                    group.features = new FeatureInfo[features.size()];
                    group.features = features.toArray(group.features);
                }
                pkg.featureGroups = ArrayUtils.add(pkg.featureGroups, group);

            } else if (tagName.equals(TAG_USES_SDK)) {
                if (SDK_VERSION > 0) {
                    sa = res.obtainAttributes(parser,
                            com.android.internal.R.styleable.AndroidManifestUsesSdk);

                    int minVers = 1;
                    String minCode = null;
                    int targetVers = 0;
                    String targetCode = null;

                    TypedValue val = sa.peekValue(
                            com.android.internal.R.styleable.AndroidManifestUsesSdk_minSdkVersion);
                    if (val != null) {
                        if (val.type == TypedValue.TYPE_STRING && val.string != null) {
                            targetCode = minCode = val.string.toString();
                        } else {
                            // If it's not a string, it's an integer.
                            targetVers = minVers = val.data;
                        }
                    }

                    val = sa.peekValue(
                            com.android.internal.R.styleable.AndroidManifestUsesSdk_targetSdkVersion);
                    if (val != null) {
                        if (val.type == TypedValue.TYPE_STRING && val.string != null) {
                            targetCode = val.string.toString();
                            if (minCode == null) {
                                minCode = targetCode;
                            }
                        } else {
                            // If it's not a string, it's an integer.
                            targetVers = val.data;
                        }
                    }

                    sa.recycle();

                    final int minSdkVersion = PackageParser.computeMinSdkVersion(minVers, minCode,
                            SDK_VERSION, SDK_CODENAMES, outError);
                    if (minSdkVersion < 0) {
                        mParseError = PackageManager.INSTALL_FAILED_OLDER_SDK;
                        return null;
                    }

                    final int targetSdkVersion = PackageParser.computeTargetSdkVersion(targetVers,
                            targetCode, SDK_VERSION, SDK_CODENAMES, outError);
                    if (targetSdkVersion < 0) {
                        mParseError = PackageManager.INSTALL_FAILED_OLDER_SDK;
                        return null;
                    }

                    pkg.applicationInfo.minSdkVersion = minSdkVersion;
                    pkg.applicationInfo.targetSdkVersion = targetSdkVersion;
                }

                XmlUtils.skipCurrentTag(parser);

            } else if (tagName.equals(TAG_SUPPORT_SCREENS)) {
                sa = res.obtainAttributes(parser,
                        com.android.internal.R.styleable.AndroidManifestSupportsScreens);

                pkg.applicationInfo.requiresSmallestWidthDp = sa.getInteger(
                        com.android.internal.R.styleable.AndroidManifestSupportsScreens_requiresSmallestWidthDp,
                        0);
                pkg.applicationInfo.compatibleWidthLimitDp = sa.getInteger(
                        com.android.internal.R.styleable.AndroidManifestSupportsScreens_compatibleWidthLimitDp,
                        0);
                pkg.applicationInfo.largestWidthLimitDp = sa.getInteger(
                        com.android.internal.R.styleable.AndroidManifestSupportsScreens_largestWidthLimitDp,
                        0);

                // This is a trick to get a boolean and still able to detect
                // if a value was actually set.
                supportsSmallScreens = sa.getInteger(
                        com.android.internal.R.styleable.AndroidManifestSupportsScreens_smallScreens,
                        supportsSmallScreens);
                supportsNormalScreens = sa.getInteger(
                        com.android.internal.R.styleable.AndroidManifestSupportsScreens_normalScreens,
                        supportsNormalScreens);
                supportsLargeScreens = sa.getInteger(
                        com.android.internal.R.styleable.AndroidManifestSupportsScreens_largeScreens,
                        supportsLargeScreens);
                supportsXLargeScreens = sa.getInteger(
                        com.android.internal.R.styleable.AndroidManifestSupportsScreens_xlargeScreens,
                        supportsXLargeScreens);
                resizeable = sa.getInteger(
                        com.android.internal.R.styleable.AndroidManifestSupportsScreens_resizeable,
                        resizeable);
                anyDensity = sa.getInteger(
                        com.android.internal.R.styleable.AndroidManifestSupportsScreens_anyDensity,
                        anyDensity);

                sa.recycle();

                XmlUtils.skipCurrentTag(parser);

            } else if (tagName.equals(TAG_PROTECTED_BROADCAST)) {
                sa = res.obtainAttributes(parser,
                        com.android.internal.R.styleable.AndroidManifestProtectedBroadcast);

                // Note: don't allow this value to be a reference to a resource
                // that may change.
                String name = sa.getNonResourceString(
                        com.android.internal.R.styleable.AndroidManifestProtectedBroadcast_name);

                sa.recycle();

                if (name != null && (flags&PARSE_IS_SYSTEM) != 0) {
                    if (pkg.protectedBroadcasts == null) {
                        pkg.protectedBroadcasts = new ArrayList<String>();
                    }
                    if (!pkg.protectedBroadcasts.contains(name)) {
                        pkg.protectedBroadcasts.add(name.intern());
                    }
                }

                XmlUtils.skipCurrentTag(parser);

            } else if (tagName.equals(TAG_INSTRUMENTATION)) {
                if (parseInstrumentation(pkg, res, parser, outError) == null) {
                    return null;
                }
            } else if (tagName.equals(TAG_ORIGINAL_PACKAGE)) {
                sa = res.obtainAttributes(parser,
                        com.android.internal.R.styleable.AndroidManifestOriginalPackage);

                String orig =sa.getNonConfigurationString(
                        com.android.internal.R.styleable.AndroidManifestOriginalPackage_name, 0);
                if (!pkg.packageName.equals(orig)) {
                    if (pkg.mOriginalPackages == null) {
                        pkg.mOriginalPackages = new ArrayList<String>();
                        pkg.mRealPackage = pkg.packageName;
                    }
                    pkg.mOriginalPackages.add(orig);
                }

                sa.recycle();

                XmlUtils.skipCurrentTag(parser);

            } else if (tagName.equals(TAG_ADOPT_PERMISSIONS)) {
                sa = res.obtainAttributes(parser,
                        com.android.internal.R.styleable.AndroidManifestOriginalPackage);

                String name = sa.getNonConfigurationString(
                        com.android.internal.R.styleable.AndroidManifestOriginalPackage_name, 0);

                sa.recycle();

                if (name != null) {
                    if (pkg.mAdoptPermissions == null) {
                        pkg.mAdoptPermissions = new ArrayList<String>();
                    }
                    pkg.mAdoptPermissions.add(name);
                }

                XmlUtils.skipCurrentTag(parser);

            } else if (tagName.equals(TAG_USES_GL_TEXTURE)) {
                // Just skip this tag
                XmlUtils.skipCurrentTag(parser);
                continue;

            } else if (tagName.equals(TAG_COMPATIBLE_SCREENS)) {
                // Just skip this tag
                XmlUtils.skipCurrentTag(parser);
                continue;
            } else if (tagName.equals(TAG_SUPPORTS_INPUT)) {//
                XmlUtils.skipCurrentTag(parser);
                continue;

            } else if (tagName.equals(TAG_EAT_COMMENT)) {
                // Just skip this tag
                XmlUtils.skipCurrentTag(parser);
                continue;

            } else if (tagName.equals(TAG_PACKAGE)) {
                if (!MULTI_PACKAGE_APK_ENABLED) {
                    XmlUtils.skipCurrentTag(parser);
                    continue;
                }
                if (!parseBaseApkChild(pkg, res, parser, flags, outError)) {
                    // If parsing a child failed the error is already set
                    return null;
                }

            } else if (tagName.equals(TAG_RESTRICT_UPDATE)) {
                if ((flags & PARSE_IS_SYSTEM_DIR) != 0) {
                    sa = res.obtainAttributes(parser,
                            com.android.internal.R.styleable.AndroidManifestRestrictUpdate);
                    final String hash = sa.getNonConfigurationString(
                            com.android.internal.R.styleable.AndroidManifestRestrictUpdate_hash, 0);
                    sa.recycle();

                    pkg.restrictUpdateHash = null;
                    if (hash != null) {
                        final int hashLength = hash.length();
                        final byte[] hashBytes = new byte[hashLength / 2];
                        for (int i = 0; i < hashLength; i += 2){
                            hashBytes[i/2] = (byte) ((Character.digit(hash.charAt(i), 16) << 4)
                                    + Character.digit(hash.charAt(i + 1), 16));
                        }
                        pkg.restrictUpdateHash = hashBytes;
                    }
                }

                XmlUtils.skipCurrentTag(parser);

            } else if (RIGID_PARSER) {
                outError[0] = "Bad element under <manifest>: "
                    + parser.getName();
                mParseError = PackageManager.INSTALL_PARSE_FAILED_MANIFEST_MALFORMED;
                return null;

            } else {
                Slog.w(TAG, "Unknown element under <manifest>: " + parser.getName()
                        + " at " + mArchiveSourcePath + " "
                        + parser.getPositionDescription());
                XmlUtils.skipCurrentTag(parser);
                continue;
            }
        }

        if (!foundApp && pkg.instrumentation.size() == 0) {
            outError[0] = "<manifest> does not contain an <application> or <instrumentation>";
            mParseError = PackageManager.INSTALL_PARSE_FAILED_MANIFEST_EMPTY;
        }

        final int NP = PackageParser.NEW_PERMISSIONS.length;
        StringBuilder implicitPerms = null;
        for (int ip=0; ip<NP; ip++) {
            final PackageParser.NewPermissionInfo npi
                    = PackageParser.NEW_PERMISSIONS[ip];
            if (pkg.applicationInfo.targetSdkVersion >= npi.sdkVersion) {
                break;
            }
            if (!pkg.requestedPermissions.contains(npi.name)) {
                if (implicitPerms == null) {
                    implicitPerms = new StringBuilder(128);
                    implicitPerms.append(pkg.packageName);
                    implicitPerms.append(": compat added ");
                } else {
                    implicitPerms.append(' ');
                }
                implicitPerms.append(npi.name);
                pkg.requestedPermissions.add(npi.name);
            }
        }
        if (implicitPerms != null) {
            Slog.i(TAG, implicitPerms.toString());
        }

        final int NS = PackageParser.SPLIT_PERMISSIONS.length;
        for (int is=0; is<NS; is++) {
            final PackageParser.SplitPermissionInfo spi
                    = PackageParser.SPLIT_PERMISSIONS[is];
            if (pkg.applicationInfo.targetSdkVersion >= spi.targetSdk
                    || !pkg.requestedPermissions.contains(spi.rootPerm)) {
                continue;
            }
            for (int in=0; in<spi.newPerms.length; in++) {
                final String perm = spi.newPerms[in];
                if (!pkg.requestedPermissions.contains(perm)) {
                    pkg.requestedPermissions.add(perm);
                }
            }
        }

        if (supportsSmallScreens < 0 || (supportsSmallScreens > 0 && pkg.applicationInfo.targetSdkVersion >= android.os.Build.VERSION_CODES.DONUT)) {
            pkg.applicationInfo.flags |= ApplicationInfo.FLAG_SUPPORTS_SMALL_SCREENS;
        }
        if (supportsNormalScreens != 0) {
            pkg.applicationInfo.flags |= ApplicationInfo.FLAG_SUPPORTS_NORMAL_SCREENS;
        }
        if (supportsLargeScreens < 0 || (supportsLargeScreens > 0
                && pkg.applicationInfo.targetSdkVersion >= android.os.Build.VERSION_CODES.DONUT)) {
            pkg.applicationInfo.flags |= ApplicationInfo.FLAG_SUPPORTS_LARGE_SCREENS;
        }
        if (supportsXLargeScreens < 0 || (supportsXLargeScreens > 0
                && pkg.applicationInfo.targetSdkVersion
                        >= android.os.Build.VERSION_CODES.GINGERBREAD)) {
            pkg.applicationInfo.flags |= ApplicationInfo.FLAG_SUPPORTS_XLARGE_SCREENS;
        }
        if (resizeable < 0 || (resizeable > 0
                && pkg.applicationInfo.targetSdkVersion
                        >= android.os.Build.VERSION_CODES.DONUT)) {
            pkg.applicationInfo.flags |= ApplicationInfo.FLAG_RESIZEABLE_FOR_SCREENS;
        }
        if (anyDensity < 0 || (anyDensity > 0
                && pkg.applicationInfo.targetSdkVersion
                        >= android.os.Build.VERSION_CODES.DONUT)) {
            pkg.applicationInfo.flags |= ApplicationInfo.FLAG_SUPPORTS_SCREEN_DENSITIES;
        }

        // At this point we can check if an application is not supporting densities and hence
        // cannot be windowed / resized. Note that an SDK version of 0 is common for
        // pre-Doughnut applications.
        if (pkg.applicationInfo.usesCompatibilityMode()) {
            adjustPackageToBeUnresizeableAndUnpipable(pkg);
        }
        return pkg;
    }




    private static int loadApkIntoAssetManager(AssetManager assets, String apkPath, int flags) throws PackageParserException {
        if ((flags & PARSE_MUST_BE_APK) != 0 && !isApkPath(apkPath)) {
            throw new PackageParserException(INSTALL_PARSE_FAILED_NOT_APK, "Invalid package file: " + apkPath);
        }

        // The AssetManager guarantees uniqueness for asset paths, so if this asset path
        // already exists in the AssetManager, addAssetPath will only return the cookie
        // assigned to it.
        int cookie = assets.addAssetPath(apkPath);
        if (cookie == 0) {
            throw new PackageParserException(INSTALL_PARSE_FAILED_BAD_MANIFEST,  "Failed adding asset path: " + apkPath);
        }
        return cookie;
    }




    private static PackageLite parseMonolithicPackageLite(File packageFile, int flags) throws PackageParserException {
        Trace.traceBegin(TRACE_TAG_PACKAGE_MANAGER, "parseApkLite");
        final ApkLite baseApk = parseApkLite(packageFile, flags);
        final String packagePath = packageFile.getAbsolutePath();
        Trace.traceEnd(TRACE_TAG_PACKAGE_MANAGER);
        return new PackageLite(packagePath, baseApk, null, null, null, null, null, null);
    }

    /**
     * Utility method that retrieves lightweight details about a single APK
     * file, including package name, split name, and install location.
     *
     * @param apkFile path to a single APK
     * @param flags optional parse flags, such as
     *            {@link #PARSE_COLLECT_CERTIFICATES}
     */
    public static ApkLite parseApkLite(File apkFile, int flags) throws PackageParserException {
        final String apkPath = apkFile.getAbsolutePath();

        AssetManager assets = null;
        XmlResourceParser parser = null;
        try {
            assets = newConfiguredAssetManager();//创建AssetManager.并且初始化
            int cookie = assets.addAssetPath(apkPath);
            if (cookie == 0) {
                throw new PackageParserException(INSTALL_PARSE_FAILED_NOT_APK, "Failed to parse " + apkPath);
            }

            final DisplayMetrics metrics = new DisplayMetrics();
            metrics.setToDefaults();

            parser = assets.openXmlResourceParser(cookie, ANDROID_MANIFEST_FILENAME); //ANDROID_MANIFEST_FILENAME = "AndroidManifest.xml"

            final Signature[] signatures;
            final Certificate[][] certificates;
            if ((flags & PARSE_COLLECT_CERTIFICATES) != 0) {
                // TODO: factor signature related items out of Package object
                final Package tempPkg = new Package((String) null);
                Trace.traceBegin(TRACE_TAG_PACKAGE_MANAGER, "collectCertificates");
                try {
                    collectCertificates(tempPkg, apkFile, flags);
                } finally {
                    Trace.traceEnd(TRACE_TAG_PACKAGE_MANAGER);
                }
                signatures = tempPkg.mSignatures;
                certificates = tempPkg.mCertificates;
            } else {
                signatures = null;
                certificates = null;
            }

            final AttributeSet attrs = parser;
            return parseApkLite(apkPath, parser, attrs, flags, signatures, certificates);

        } catch (XmlPullParserException | IOException | RuntimeException e) {
            Slog.w(TAG, "Failed to parse " + apkPath, e);
            throw new PackageParserException(INSTALL_PARSE_FAILED_UNEXPECTED_EXCEPTION, "Failed to parse " + apkPath, e);
        } finally {
            IoUtils.closeQuietly(parser);
            IoUtils.closeQuietly(assets);
        }
    }

    private static ApkLite parseApkLite(String codePath, XmlPullParser parser, AttributeSet attrs,int flags, Signature[] signatures, Certificate[][] certificates)throws IOException, XmlPullParserException, PackageParserException {
        final Pair<String, String> packageSplit = parsePackageSplitNames(parser, attrs);

        int installLocation = PARSE_DEFAULT_INSTALL_LOCATION;
        int versionCode = 0;
        int revisionCode = 0;
        boolean coreApp = false;
        boolean debuggable = false;
        boolean multiArch = false;
        boolean use32bitAbi = false;
        boolean extractNativeLibs = true;
        boolean isolatedSplits = false;
        boolean isFeatureSplit = false;
        String configForSplit = null;
        String usesSplitName = null;

        for (int i = 0; i < attrs.getAttributeCount(); i++) {
            final String attr = attrs.getAttributeName(i);
            if (attr.equals("installLocation")) {
                installLocation = attrs.getAttributeIntValue(i,PARSE_DEFAULT_INSTALL_LOCATION);
            } else if (attr.equals("versionCode")) {
                versionCode = attrs.getAttributeIntValue(i, 0);
            } else if (attr.equals("revisionCode")) {
                revisionCode = attrs.getAttributeIntValue(i, 0);
            } else if (attr.equals("coreApp")) {
                coreApp = attrs.getAttributeBooleanValue(i, false);
            } else if (attr.equals("isolatedSplits")) {
                isolatedSplits = attrs.getAttributeBooleanValue(i, false);
            } else if (attr.equals("configForSplit")) {
                configForSplit = attrs.getAttributeValue(i);
            } else if (attr.equals("isFeatureSplit")) {
                isFeatureSplit = attrs.getAttributeBooleanValue(i, false);
            }
        }

        // Only search the tree when the tag is directly below <manifest>
        int type;
        final int searchDepth = parser.getDepth() + 1;

        final List<VerifierInfo> verifiers = new ArrayList<VerifierInfo>();
        while ((type = parser.next()) != XmlPullParser.END_DOCUMENT && (type != XmlPullParser.END_TAG || parser.getDepth() >= searchDepth)) {
            if (type == XmlPullParser.END_TAG || type == XmlPullParser.TEXT) {
                continue;
            }

            if (parser.getDepth() != searchDepth) {
                continue;
            }

            if (TAG_PACKAGE_VERIFIER.equals(parser.getName())) {
                final VerifierInfo verifier = parseVerifier(attrs);
                if (verifier != null) {
                    verifiers.add(verifier);
                }
            } else if (TAG_APPLICATION.equals(parser.getName())) {
                for (int i = 0; i < attrs.getAttributeCount(); ++i) {
                    final String attr = attrs.getAttributeName(i);
                    if ("debuggable".equals(attr)) {
                        debuggable = attrs.getAttributeBooleanValue(i, false);
                    }
                    if ("multiArch".equals(attr)) {
                        multiArch = attrs.getAttributeBooleanValue(i, false);
                    }
                    if ("use32bitAbi".equals(attr)) {
                        use32bitAbi = attrs.getAttributeBooleanValue(i, false);
                    }
                    if ("extractNativeLibs".equals(attr)) {
                        extractNativeLibs = attrs.getAttributeBooleanValue(i, true);
                    }
                }
            } else if (TAG_USES_SPLIT.equals(parser.getName())) {
                if (usesSplitName != null) {
                    Slog.w(TAG, "Only one <uses-split> permitted. Ignoring others.");
                    continue;
                }

                usesSplitName = attrs.getAttributeValue(ANDROID_RESOURCES, "name");
                if (usesSplitName == null) {
                    throw new PackageParserException(PackageManager.INSTALL_PARSE_FAILED_MANIFEST_MALFORMED,"<uses-split> tag requires 'android:name' attribute");
                }
            }
        }

        return new ApkLite(codePath, packageSplit.first, packageSplit.second, isFeatureSplit,configForSplit, usesSplitName, versionCode, revisionCode, installLocation,
                verifiers, signatures, certificates, coreApp, debuggable, multiArch, use32bitAbi,extractNativeLibs, isolatedSplits);
    }

    private static Pair<String, String> parsePackageSplitNames(XmlPullParser parser,AttributeSet attrs) throws IOException, XmlPullParserException,PackageParserException {

        int type;
        while ((type = parser.next()) != XmlPullParser.START_TAG&& type != XmlPullParser.END_DOCUMENT) {
        }

        if (type != XmlPullParser.START_TAG) {
            throw new PackageParserException(INSTALL_PARSE_FAILED_MANIFEST_MALFORMED,"No start tag found");
        }
        if (!parser.getName().equals(TAG_MANIFEST)) {																											//	TAG_MANIFEST = "manifest"
            throw new PackageParserException(INSTALL_PARSE_FAILED_MANIFEST_MALFORMED,"No <manifest> tag");
        }

        final String packageName = attrs.getAttributeValue(null, "package");
        if (!"android".equals(packageName)) {
            final String error = validateName(packageName, true, true);
            if (error != null) {
                throw new PackageParserException(INSTALL_PARSE_FAILED_BAD_PACKAGE_NAME,"Invalid manifest package: " + error);
            }
        }

        String splitName = attrs.getAttributeValue(null, "split");
        if (splitName != null) {
            if (splitName.length() == 0) {
                splitName = null;
            } else {
                final String error = validateName(splitName, false, false);
                if (error != null) {
                    throw new PackageParserException(INSTALL_PARSE_FAILED_BAD_PACKAGE_NAME, "Invalid manifest split: " + error);
                }
            }
        }

        return Pair.create(packageName.intern(), (splitName != null) ? splitName.intern() : splitName);
    }

    

}