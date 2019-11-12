 // @frameworks/base/core/java/android/content/pm/PackageParser.java


 /**
 * Representation of a full package parsed from APK files on disk. A package
 * consists of a single base APK, and zero or more split APKs.
 */
public final static class Package implements Parcelable {

    public String packageName;

    // The package name declared in the manifest as the package can be
    // renamed, for example static shared libs use synthetic package names.
    public String manifestPackageName;

    /** Names of any split APKs, ordered by parsed splitName */
    public String[] splitNames;

    // TODO: work towards making these paths invariant

    public String volumeUuid;

    /**
     * Path where this package was found on disk. For monolithic packages
     * this is path to single base APK file; for cluster packages this is
     * path to the cluster directory.
     */
    public String codePath;

    /** Path of base APK */
    public String baseCodePath;
    /** Paths of any split APKs, ordered by parsed splitName */
    public String[] splitCodePaths;

    /** Revision code of base APK */
    public int baseRevisionCode;
    /** Revision codes of any split APKs, ordered by parsed splitName */
    public int[] splitRevisionCodes;

    /** Flags of any split APKs; ordered by parsed splitName */
    public int[] splitFlags;

    /**
     * Private flags of any split APKs; ordered by parsed splitName.
     *
     * {@hide}
     */
    public int[] splitPrivateFlags;

    public boolean baseHardwareAccelerated;

    // For now we only support one application per package.
    public ApplicationInfo applicationInfo = new ApplicationInfo();

    public final ArrayList<Permission> permissions = new ArrayList<Permission>(0);
    public final ArrayList<PermissionGroup> permissionGroups = new ArrayList<PermissionGroup>(0);
    public final ArrayList<Activity> activities = new ArrayList<Activity>(0);
    public final ArrayList<Activity> receivers = new ArrayList<Activity>(0);
    public final ArrayList<Provider> providers = new ArrayList<Provider>(0);
    public final ArrayList<Service> services = new ArrayList<Service>(0);
    public final ArrayList<Instrumentation> instrumentation = new ArrayList<Instrumentation>(0);

    public final ArrayList<String> requestedPermissions = new ArrayList<String>();

    public ArrayList<String> protectedBroadcasts;

    public Package parentPackage;
    public ArrayList<Package> childPackages;

    public String staticSharedLibName = null;
    public int staticSharedLibVersion = 0;
    public ArrayList<String> libraryNames = null;
    public ArrayList<String> usesLibraries = null;
    public ArrayList<String> usesStaticLibraries = null;
    public int[] usesStaticLibrariesVersions = null;
    public String[] usesStaticLibrariesCertDigests = null;
    public ArrayList<String> usesOptionalLibraries = null;
    public String[] usesLibraryFiles = null;

    public ArrayList<ActivityIntentInfo> preferredActivityFilters = null;

    public ArrayList<String> mOriginalPackages = null;
    public String mRealPackage = null;
    public ArrayList<String> mAdoptPermissions = null;

    // We store the application meta-data independently to avoid multiple unwanted references
    public Bundle mAppMetaData = null;

    // The version code declared for this package.
    public int mVersionCode;

    // The version name declared for this package.
    public String mVersionName;

    // The shared user id that this package wants to use.
    public String mSharedUserId;

    // The shared user label that this package wants to use.
    public int mSharedUserLabel;

    // Signatures that were read from the package.
    public Signature[] mSignatures;
    public Certificate[][] mCertificates;

    // For use by package manager service for quick lookup of
    // preferred up order.
    public int mPreferredOrder = 0;

    // For use by package manager to keep track of when a package was last used.
    public long[] mLastPackageUsageTimeInMills =
            new long[PackageManager.NOTIFY_PACKAGE_USE_REASONS_COUNT];

    // // User set enabled state.
    // public int mSetEnabled = PackageManager.COMPONENT_ENABLED_STATE_DEFAULT;
    //
    // // Whether the package has been stopped.
    // public boolean mSetStopped = false;

    // Additional data supplied by callers.
    public Object mExtras;

    // Applications hardware preferences
    public ArrayList<ConfigurationInfo> configPreferences = null;

    // Applications requested features
    public ArrayList<FeatureInfo> reqFeatures = null;

    // Applications requested feature groups
    public ArrayList<FeatureGroupInfo> featureGroups = null;

    public int installLocation;

    public boolean coreApp;

    /* An app that's required for all users and cannot be uninstalled for a user */
    public boolean mRequiredForAllUsers;

    /* The restricted account authenticator type that is used by this application */
    public String mRestrictedAccountType;

    /* The required account type without which this application will not function */
    public String mRequiredAccountType;

    public String mOverlayTarget;
    public int mOverlayPriority;
    public boolean mIsStaticOverlay;
    public boolean mTrustedOverlay;

    /**
     * Data used to feed the KeySetManagerService
     */
    public ArraySet<PublicKey> mSigningKeys;
    public ArraySet<String> mUpgradeKeySets;
    public ArrayMap<String, ArraySet<PublicKey>> mKeySetMapping;

    /**
     * The install time abi override for this package, if any.
     *
     * TODO: This seems like a horrible place to put the abiOverride because
     * this isn't something the packageParser parsers. However, this fits in with
     * the rest of the PackageManager where package scanning randomly pushes
     * and prods fields out of {@code this.applicationInfo}.
     */
    public String cpuAbiOverride;
    /**
     * The install time abi override to choose 32bit abi's when multiple abi's
     * are present. This is only meaningfull for multiarch applications.
     * The use32bitAbi attribute is ignored if cpuAbiOverride is also set.
     */
    public boolean use32bitAbi;

    public byte[] restrictUpdateHash;

    /**
     * Set if the app or any of its components are visible to Instant Apps.
     */
    public boolean visibleToInstantApps;

    public Package(String packageName) {
        this.packageName = packageName;
        this.manifestPackageName = packageName;
        applicationInfo.packageName = packageName;
        applicationInfo.uid = -1;
    }

    public void setApplicationVolumeUuid(String volumeUuid) {
        final UUID storageUuid = StorageManager.convert(volumeUuid);
        this.applicationInfo.volumeUuid = volumeUuid;
        this.applicationInfo.storageUuid = storageUuid;
        if (childPackages != null) {
            final int packageCount = childPackages.size();
            for (int i = 0; i < packageCount; i++) {
                childPackages.get(i).applicationInfo.volumeUuid = volumeUuid;
                childPackages.get(i).applicationInfo.storageUuid = storageUuid;
            }
        }
    }

    public void setApplicationInfoCodePath(String codePath) {
        this.applicationInfo.setCodePath(codePath);
        if (childPackages != null) {
            final int packageCount = childPackages.size();
            for (int i = 0; i < packageCount; i++) {
                childPackages.get(i).applicationInfo.setCodePath(codePath);
            }
        }
    }

    public void setApplicationInfoResourcePath(String resourcePath) {
        this.applicationInfo.setResourcePath(resourcePath);
        if (childPackages != null) {
            final int packageCount = childPackages.size();
            for (int i = 0; i < packageCount; i++) {
                childPackages.get(i).applicationInfo.setResourcePath(resourcePath);
            }
        }
    }

    public void setApplicationInfoBaseResourcePath(String resourcePath) {
        this.applicationInfo.setBaseResourcePath(resourcePath);
        if (childPackages != null) {
            final int packageCount = childPackages.size();
            for (int i = 0; i < packageCount; i++) {
                childPackages.get(i).applicationInfo.setBaseResourcePath(resourcePath);
            }
        }
    }

    public void setApplicationInfoBaseCodePath(String baseCodePath) {
        this.applicationInfo.setBaseCodePath(baseCodePath);
        if (childPackages != null) {
            final int packageCount = childPackages.size();
            for (int i = 0; i < packageCount; i++) {
                childPackages.get(i).applicationInfo.setBaseCodePath(baseCodePath);
            }
        }
    }

    public List<String> getChildPackageNames() {
        if (childPackages == null) {
            return null;
        }
        final int childCount = childPackages.size();
        final List<String> childPackageNames = new ArrayList<>(childCount);
        for (int i = 0; i < childCount; i++) {
            String childPackageName = childPackages.get(i).packageName;
            childPackageNames.add(childPackageName);
        }
        return childPackageNames;
    }

    public boolean hasChildPackage(String packageName) {
        final int childCount = (childPackages != null) ? childPackages.size() : 0;
        for (int i = 0; i < childCount; i++) {
            if (childPackages.get(i).packageName.equals(packageName)) {
                return true;
            }
        }
        return false;
    }

    public void setApplicationInfoSplitCodePaths(String[] splitCodePaths) {
        this.applicationInfo.setSplitCodePaths(splitCodePaths);
        // Children have no splits
    }

    public void setApplicationInfoSplitResourcePaths(String[] resroucePaths) {
        this.applicationInfo.setSplitResourcePaths(resroucePaths);
        // Children have no splits
    }

    public void setSplitCodePaths(String[] codePaths) {
        this.splitCodePaths = codePaths;
    }

    public void setCodePath(String codePath) {
        this.codePath = codePath;
        if (childPackages != null) {
            final int packageCount = childPackages.size();
            for (int i = 0; i < packageCount; i++) {
                childPackages.get(i).codePath = codePath;
            }
        }
    }

    public void setBaseCodePath(String baseCodePath) {
        this.baseCodePath = baseCodePath;
        if (childPackages != null) {
            final int packageCount = childPackages.size();
            for (int i = 0; i < packageCount; i++) {
                childPackages.get(i).baseCodePath = baseCodePath;
            }
        }
    }

    public void setSignatures(Signature[] signatures) {
        this.mSignatures = signatures;
        if (childPackages != null) {
            final int packageCount = childPackages.size();
            for (int i = 0; i < packageCount; i++) {
                childPackages.get(i).mSignatures = signatures;
            }
        }
    }

    public void setVolumeUuid(String volumeUuid) {
        this.volumeUuid = volumeUuid;
        if (childPackages != null) {
            final int packageCount = childPackages.size();
            for (int i = 0; i < packageCount; i++) {
                childPackages.get(i).volumeUuid = volumeUuid;
            }
        }
    }

    public void setApplicationInfoFlags(int mask, int flags) {
        applicationInfo.flags = (applicationInfo.flags & ~mask) | (mask & flags);
        if (childPackages != null) {
            final int packageCount = childPackages.size();
            for (int i = 0; i < packageCount; i++) {
                childPackages.get(i).applicationInfo.flags =
                        (applicationInfo.flags & ~mask) | (mask & flags);
            }
        }
    }

    public void setUse32bitAbi(boolean use32bitAbi) {
        this.use32bitAbi = use32bitAbi;
        if (childPackages != null) {
            final int packageCount = childPackages.size();
            for (int i = 0; i < packageCount; i++) {
                childPackages.get(i).use32bitAbi = use32bitAbi;
            }
        }
    }

    public boolean isLibrary() {
        return staticSharedLibName != null || !ArrayUtils.isEmpty(libraryNames);
    }

    public List<String> getAllCodePaths() {
        ArrayList<String> paths = new ArrayList<>();
        paths.add(baseCodePath);
        if (!ArrayUtils.isEmpty(splitCodePaths)) {
            Collections.addAll(paths, splitCodePaths);
        }
        return paths;
    }

    /**
     * Filtered set of {@link #getAllCodePaths()} that excludes
     * resource-only APKs.
     */
    public List<String> getAllCodePathsExcludingResourceOnly() {
        ArrayList<String> paths = new ArrayList<>();
        if ((applicationInfo.flags & ApplicationInfo.FLAG_HAS_CODE) != 0) {
            paths.add(baseCodePath);
        }
        if (!ArrayUtils.isEmpty(splitCodePaths)) {
            for (int i = 0; i < splitCodePaths.length; i++) {
                if ((splitFlags[i] & ApplicationInfo.FLAG_HAS_CODE) != 0) {
                    paths.add(splitCodePaths[i]);
                }
            }
        }
        return paths;
    }

    public void setPackageName(String newName) {
        packageName = newName;
        applicationInfo.packageName = newName;
        for (int i=permissions.size()-1; i>=0; i--) {
            permissions.get(i).setPackageName(newName);
        }
        for (int i=permissionGroups.size()-1; i>=0; i--) {
            permissionGroups.get(i).setPackageName(newName);
        }
        for (int i=activities.size()-1; i>=0; i--) {
            activities.get(i).setPackageName(newName);
        }
        for (int i=receivers.size()-1; i>=0; i--) {
            receivers.get(i).setPackageName(newName);
        }
        for (int i=providers.size()-1; i>=0; i--) {
            providers.get(i).setPackageName(newName);
        }
        for (int i=services.size()-1; i>=0; i--) {
            services.get(i).setPackageName(newName);
        }
        for (int i=instrumentation.size()-1; i>=0; i--) {
            instrumentation.get(i).setPackageName(newName);
        }
    }

    public boolean hasComponentClassName(String name) {
        for (int i=activities.size()-1; i>=0; i--) {
            if (name.equals(activities.get(i).className)) {
                return true;
            }
        }
        for (int i=receivers.size()-1; i>=0; i--) {
            if (name.equals(receivers.get(i).className)) {
                return true;
            }
        }
        for (int i=providers.size()-1; i>=0; i--) {
            if (name.equals(providers.get(i).className)) {
                return true;
            }
        }
        for (int i=services.size()-1; i>=0; i--) {
            if (name.equals(services.get(i).className)) {
                return true;
            }
        }
        for (int i=instrumentation.size()-1; i>=0; i--) {
            if (name.equals(instrumentation.get(i).className)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @hide
     */
    public boolean isForwardLocked() {
        return applicationInfo.isForwardLocked();
    }

    /**
     * @hide
     */
    public boolean isSystemApp() {
        return applicationInfo.isSystemApp();
    }

    /**
     * @hide
     */
    public boolean isPrivilegedApp() {
        return applicationInfo.isPrivilegedApp();
    }

    /**
     * @hide
     */
    public boolean isUpdatedSystemApp() {
        return applicationInfo.isUpdatedSystemApp();
    }

    /**
     * @hide
     */
    public boolean canHaveOatDir() {
        // The following app types CANNOT have oat directory
        // - non-updated system apps
        // - forward-locked apps or apps installed in ASEC containers
        return (!isSystemApp() || isUpdatedSystemApp())
                && !isForwardLocked() && !applicationInfo.isExternalAsec();
    }

    public boolean isMatch(int flags) {
        if ((flags & PackageManager.MATCH_SYSTEM_ONLY) != 0) {
            return isSystemApp();
        }
        return true;
    }

    public long getLatestPackageUseTimeInMills() {
        long latestUse = 0L;
        for (long use : mLastPackageUsageTimeInMills) {
            latestUse = Math.max(latestUse, use);
        }
        return latestUse;
    }

    public long getLatestForegroundPackageUseTimeInMills() {
        int[] foregroundReasons = {
            PackageManager.NOTIFY_PACKAGE_USE_ACTIVITY,
            PackageManager.NOTIFY_PACKAGE_USE_FOREGROUND_SERVICE
        };

        long latestUse = 0L;
        for (int reason : foregroundReasons) {
            latestUse = Math.max(latestUse, mLastPackageUsageTimeInMills[reason]);
        }
        return latestUse;
    }

    public String toString() {
        return "Package{"
            + Integer.toHexString(System.identityHashCode(this))
            + " " + packageName + "}";
    }

    @Override
    public int describeContents() {
        return 0;
    }

    public Package(Parcel dest) {
        // We use the boot classloader for all classes that we load.
        final ClassLoader boot = Object.class.getClassLoader();

        packageName = dest.readString().intern();
        manifestPackageName = dest.readString();
        splitNames = dest.readStringArray();
        volumeUuid = dest.readString();
        codePath = dest.readString();
        baseCodePath = dest.readString();
        splitCodePaths = dest.readStringArray();
        baseRevisionCode = dest.readInt();
        splitRevisionCodes = dest.createIntArray();
        splitFlags = dest.createIntArray();
        splitPrivateFlags = dest.createIntArray();
        baseHardwareAccelerated = (dest.readInt() == 1);
        applicationInfo = dest.readParcelable(boot);
        if (applicationInfo.permission != null) {
            applicationInfo.permission = applicationInfo.permission.intern();
        }

        // We don't serialize the "owner" package and the application info object for each of
        // these components, in order to save space and to avoid circular dependencies while
        // serialization. We need to fix them all up here.
        dest.readParcelableList(permissions, boot);
        fixupOwner(permissions);
        dest.readParcelableList(permissionGroups, boot);
        fixupOwner(permissionGroups);
        dest.readParcelableList(activities, boot);
        fixupOwner(activities);
        dest.readParcelableList(receivers, boot);
        fixupOwner(receivers);
        dest.readParcelableList(providers, boot);
        fixupOwner(providers);
        dest.readParcelableList(services, boot);
        fixupOwner(services);
        dest.readParcelableList(instrumentation, boot);
        fixupOwner(instrumentation);

        dest.readStringList(requestedPermissions);
        internStringArrayList(requestedPermissions);
        protectedBroadcasts = dest.createStringArrayList();
        internStringArrayList(protectedBroadcasts);

        parentPackage = dest.readParcelable(boot);

        childPackages = new ArrayList<>();
        dest.readParcelableList(childPackages, boot);
        if (childPackages.size() == 0) {
            childPackages = null;
        }

        staticSharedLibName = dest.readString();
        if (staticSharedLibName != null) {
            staticSharedLibName = staticSharedLibName.intern();
        }
        staticSharedLibVersion = dest.readInt();
        libraryNames = dest.createStringArrayList();
        internStringArrayList(libraryNames);
        usesLibraries = dest.createStringArrayList();
        internStringArrayList(usesLibraries);
        usesOptionalLibraries = dest.createStringArrayList();
        internStringArrayList(usesOptionalLibraries);
        usesLibraryFiles = dest.readStringArray();

        final int libCount = dest.readInt();
        if (libCount > 0) {
            usesStaticLibraries = new ArrayList<>(libCount);
            dest.readStringList(usesStaticLibraries);
            internStringArrayList(usesStaticLibraries);
            usesStaticLibrariesVersions = new int[libCount];
            dest.readIntArray(usesStaticLibrariesVersions);
            usesStaticLibrariesCertDigests = new String[libCount];
            dest.readStringArray(usesStaticLibrariesCertDigests);
        }

        preferredActivityFilters = new ArrayList<>();
        dest.readParcelableList(preferredActivityFilters, boot);
        if (preferredActivityFilters.size() == 0) {
            preferredActivityFilters = null;
        }

        mOriginalPackages = dest.createStringArrayList();
        mRealPackage = dest.readString();
        mAdoptPermissions = dest.createStringArrayList();
        mAppMetaData = dest.readBundle();
        mVersionCode = dest.readInt();
        mVersionName = dest.readString();
        if (mVersionName != null) {
            mVersionName = mVersionName.intern();
        }
        mSharedUserId = dest.readString();
        if (mSharedUserId != null) {
            mSharedUserId = mSharedUserId.intern();
        }
        mSharedUserLabel = dest.readInt();

        mSignatures = (Signature[]) dest.readParcelableArray(boot, Signature.class);
        mCertificates = (Certificate[][]) dest.readSerializable();

        mPreferredOrder = dest.readInt();

        // long[] packageUsageTimeMillis is not persisted because it isn't information that
        // is parsed from the APK.

        // Object mExtras is not persisted because it is not information that is read from
        // the APK, rather, it is supplied by callers.


        configPreferences = new ArrayList<>();
        dest.readParcelableList(configPreferences, boot);
        if (configPreferences.size() == 0) {
            configPreferences = null;
        }

        reqFeatures = new ArrayList<>();
        dest.readParcelableList(reqFeatures, boot);
        if (reqFeatures.size() == 0) {
            reqFeatures = null;
        }

        featureGroups = new ArrayList<>();
        dest.readParcelableList(featureGroups, boot);
        if (featureGroups.size() == 0) {
            featureGroups = null;
        }

        installLocation = dest.readInt();
        coreApp = (dest.readInt() == 1);
        mRequiredForAllUsers = (dest.readInt() == 1);
        mRestrictedAccountType = dest.readString();
        mRequiredAccountType = dest.readString();
        mOverlayTarget = dest.readString();
        mOverlayPriority = dest.readInt();
        mIsStaticOverlay = (dest.readInt() == 1);
        mTrustedOverlay = (dest.readInt() == 1);
        mSigningKeys = (ArraySet<PublicKey>) dest.readArraySet(boot);
        mUpgradeKeySets = (ArraySet<String>) dest.readArraySet(boot);

        mKeySetMapping = readKeySetMapping(dest);

        cpuAbiOverride = dest.readString();
        use32bitAbi = (dest.readInt() == 1);
        restrictUpdateHash = dest.createByteArray();
        visibleToInstantApps = dest.readInt() == 1;
    }

    private static void internStringArrayList(List<String> list) {
        if (list != null) {
            final int N = list.size();
            for (int i = 0; i < N; ++i) {
                list.set(i, list.get(i).intern());
            }
        }
    }

    /**
     * Sets the package owner and the the {@code applicationInfo} for every component
     * owner by this package.
     */
    private void fixupOwner(List<? extends Component<?>> list) {
        if (list != null) {
            for (Component<?> c : list) {
                c.owner = this;
                if (c instanceof Activity) {
                    ((Activity) c).info.applicationInfo = this.applicationInfo;
                } else if (c instanceof Service) {
                    ((Service) c).info.applicationInfo = this.applicationInfo;
                } else if (c instanceof Provider) {
                    ((Provider) c).info.applicationInfo = this.applicationInfo;
                }
            }
        }
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(packageName);
        dest.writeString(manifestPackageName);
        dest.writeStringArray(splitNames);
        dest.writeString(volumeUuid);
        dest.writeString(codePath);
        dest.writeString(baseCodePath);
        dest.writeStringArray(splitCodePaths);
        dest.writeInt(baseRevisionCode);
        dest.writeIntArray(splitRevisionCodes);
        dest.writeIntArray(splitFlags);
        dest.writeIntArray(splitPrivateFlags);
        dest.writeInt(baseHardwareAccelerated ? 1 : 0);
        dest.writeParcelable(applicationInfo, flags);

        dest.writeParcelableList(permissions, flags);
        dest.writeParcelableList(permissionGroups, flags);
        dest.writeParcelableList(activities, flags);
        dest.writeParcelableList(receivers, flags);
        dest.writeParcelableList(providers, flags);
        dest.writeParcelableList(services, flags);
        dest.writeParcelableList(instrumentation, flags);

        dest.writeStringList(requestedPermissions);
        dest.writeStringList(protectedBroadcasts);
        dest.writeParcelable(parentPackage, flags);
        dest.writeParcelableList(childPackages, flags);
        dest.writeString(staticSharedLibName);
        dest.writeInt(staticSharedLibVersion);
        dest.writeStringList(libraryNames);
        dest.writeStringList(usesLibraries);
        dest.writeStringList(usesOptionalLibraries);
        dest.writeStringArray(usesLibraryFiles);

        if (ArrayUtils.isEmpty(usesStaticLibraries)) {
            dest.writeInt(-1);
        } else {
            dest.writeInt(usesStaticLibraries.size());
            dest.writeStringList(usesStaticLibraries);
            dest.writeIntArray(usesStaticLibrariesVersions);
            dest.writeStringArray(usesStaticLibrariesCertDigests);
        }

        dest.writeParcelableList(preferredActivityFilters, flags);

        dest.writeStringList(mOriginalPackages);
        dest.writeString(mRealPackage);
        dest.writeStringList(mAdoptPermissions);
        dest.writeBundle(mAppMetaData);
        dest.writeInt(mVersionCode);
        dest.writeString(mVersionName);
        dest.writeString(mSharedUserId);
        dest.writeInt(mSharedUserLabel);

        dest.writeParcelableArray(mSignatures, flags);
        dest.writeSerializable(mCertificates);

        dest.writeInt(mPreferredOrder);

        // long[] packageUsageTimeMillis is not persisted because it isn't information that
        // is parsed from the APK.

        // Object mExtras is not persisted because it is not information that is read from
        // the APK, rather, it is supplied by callers.

        dest.writeParcelableList(configPreferences, flags);
        dest.writeParcelableList(reqFeatures, flags);
        dest.writeParcelableList(featureGroups, flags);

        dest.writeInt(installLocation);
        dest.writeInt(coreApp ? 1 : 0);
        dest.writeInt(mRequiredForAllUsers ? 1 : 0);
        dest.writeString(mRestrictedAccountType);
        dest.writeString(mRequiredAccountType);
        dest.writeString(mOverlayTarget);
        dest.writeInt(mOverlayPriority);
        dest.writeInt(mIsStaticOverlay ? 1 : 0);
        dest.writeInt(mTrustedOverlay ? 1 : 0);
        dest.writeArraySet(mSigningKeys);
        dest.writeArraySet(mUpgradeKeySets);
        writeKeySetMapping(dest, mKeySetMapping);
        dest.writeString(cpuAbiOverride);
        dest.writeInt(use32bitAbi ? 1 : 0);
        dest.writeByteArray(restrictUpdateHash);
        dest.writeInt(visibleToInstantApps ? 1 : 0);
    }


    /**
     * Writes the keyset mapping to the provided package. {@code null} mappings are permitted.
     */
    private static void writeKeySetMapping(
            Parcel dest, ArrayMap<String, ArraySet<PublicKey>> keySetMapping) {
        if (keySetMapping == null) {
            dest.writeInt(-1);
            return;
        }

        final int N = keySetMapping.size();
        dest.writeInt(N);

        for (int i = 0; i < N; i++) {
            dest.writeString(keySetMapping.keyAt(i));
            ArraySet<PublicKey> keys = keySetMapping.valueAt(i);
            if (keys == null) {
                dest.writeInt(-1);
                continue;
            }

            final int M = keys.size();
            dest.writeInt(M);
            for (int j = 0; j < M; j++) {
                dest.writeSerializable(keys.valueAt(j));
            }
        }
    }

    /**
     * Reads a keyset mapping from the given parcel at the given data position. May return
     * {@code null} if the serialized mapping was {@code null}.
     */
    private static ArrayMap<String, ArraySet<PublicKey>> readKeySetMapping(Parcel in) {
        final int N = in.readInt();
        if (N == -1) {
            return null;
        }

        ArrayMap<String, ArraySet<PublicKey>> keySetMapping = new ArrayMap<>();
        for (int i = 0; i < N; ++i) {
            String key = in.readString();
            final int M = in.readInt();
            if (M == -1) {
                keySetMapping.put(key, null);
                continue;
            }

            ArraySet<PublicKey> keys = new ArraySet<>(M);
            for (int j = 0; j < M; ++j) {
                PublicKey pk = (PublicKey) in.readSerializable();
                keys.add(pk);
            }

            keySetMapping.put(key, keys);
        }

        return keySetMapping;
    }

    public static final Parcelable.Creator CREATOR = new Parcelable.Creator<Package>() {
        public Package createFromParcel(Parcel in) {
            return new Package(in);
        }

        public Package[] newArray(int size) {
            return new Package[size];
        }
    };
}