/**
 * Local state maintained about a currently loaded .apk.
 * @hide
 */
public final class LoadedApk {

	 /**
     * Create information about a new .apk
     *
     * NOTE: This constructor is called with ActivityThread's lock held,
     * so MUST NOT call back out to the activity manager.
     */
    public LoadedApk(ActivityThread activityThread, ApplicationInfo aInfo,CompatibilityInfo compatInfo, ClassLoader baseLoader,boolean securityViolation, boolean includeCode, boolean registerPackage) {
        mActivityThread = activityThread;
        setApplicationInfo(aInfo);
        mPackageName = aInfo.packageName;
        mBaseClassLoader = baseLoader;
        mSecurityViolation = securityViolation;
        mIncludeCode = includeCode;
        mRegisterPackage = registerPackage;
        mDisplayAdjustments.setCompatibilityInfo(compatInfo);
    }

    
}