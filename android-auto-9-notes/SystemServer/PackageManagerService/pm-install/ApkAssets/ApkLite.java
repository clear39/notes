//	@	frameworks/base/core/java/android/content/pm/PackageParser.java
public static class ApkLite {
    public final String codePath;
    public final String packageName;
    public final String splitName;
    public boolean isFeatureSplit;
    public final String configForSplit;
    public final String usesSplitName;
    public final int versionCode;
    public final int versionCodeMajor;
    public final int revisionCode;
    public final int installLocation;
    public final VerifierInfo[] verifiers;
    public final SigningDetails signingDetails;
    public final boolean coreApp;
    public final boolean debuggable;
    public final boolean multiArch;
    public final boolean use32bitAbi;
    public final boolean extractNativeLibs;
    public final boolean isolatedSplits;

    public ApkLite(String codePath, String packageName, String splitName,
            boolean isFeatureSplit,
            String configForSplit, String usesSplitName, int versionCode, int versionCodeMajor,
            int revisionCode, int installLocation, List<VerifierInfo> verifiers,
            SigningDetails signingDetails, boolean coreApp,
            boolean debuggable, boolean multiArch, boolean use32bitAbi,
            boolean extractNativeLibs, boolean isolatedSplits) {
        this.codePath = codePath;
        this.packageName = packageName;
        this.splitName = splitName;
        this.isFeatureSplit = isFeatureSplit;
        this.configForSplit = configForSplit;
        this.usesSplitName = usesSplitName;
        this.versionCode = versionCode;
        this.versionCodeMajor = versionCodeMajor;
        this.revisionCode = revisionCode;
        this.installLocation = installLocation;
        this.signingDetails = signingDetails;
        this.verifiers = verifiers.toArray(new VerifierInfo[verifiers.size()]);
        this.coreApp = coreApp;
        this.debuggable = debuggable;
        this.multiArch = multiArch;
        this.use32bitAbi = use32bitAbi;
        this.extractNativeLibs = extractNativeLibs;
        this.isolatedSplits = isolatedSplits;
    }

    public long getLongVersionCode() {
        return PackageInfo.composeLongVersionCode(versionCodeMajor, versionCode);
    }
}