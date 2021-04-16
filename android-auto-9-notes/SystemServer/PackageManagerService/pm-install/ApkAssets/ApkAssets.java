


public final class ApkAssets {

    public static @NonNull ApkAssets loadFromFd(@NonNull FileDescriptor fd,
        @NonNull String friendlyName, boolean system, boolean forceSharedLibrary)
        throws IOException {
        // fd 打开apk
        // friendlyName 为apk路径
        // system和forceSharedLibrary 均为false
        return new ApkAssets(fd, friendlyName, system, forceSharedLibrary);
    }

    private ApkAssets(@NonNull FileDescriptor fd, @NonNull String friendlyName, boolean system,
            boolean forceSharedLib) throws IOException {
        Preconditions.checkNotNull(fd, "fd");
        Preconditions.checkNotNull(friendlyName, "friendlyName");
        // nativeLoadFromFd @ frameworks/base/core/jni/android_content_res_ApkAssets.cpp
        mNativePtr = nativeLoadFromFd(fd, friendlyName, system, forceSharedLib);
        //
        mStringBlock = new StringBlock(nativeGetStringBlock(mNativePtr), true /*useSparse*/);
    }

    /**
     * Retrieve a parser for a compiled XML file. This is associated with a single APK and
     * <em>NOT</em> a full AssetManager. This means that shared-library references will not be
     * dynamically assigned runtime package IDs.
     *
     * @param fileName The path to the file within the APK.
     * @return An XmlResourceParser.
     * @throws IOException if the file was not found or an error occurred retrieving it.
     */
    public @NonNull XmlResourceParser openXml(@NonNull String fileName) throws IOException {
        Preconditions.checkNotNull(fileName, "fileName");
        synchronized (this) {
        	//
            long nativeXmlPtr = nativeOpenXml(mNativePtr, fileName);
            try (XmlBlock block = new XmlBlock(null, nativeXmlPtr)) {
                XmlResourceParser parser = block.newParser();
                // If nativeOpenXml doesn't throw, it will always return a valid native pointer,
                // which makes newParser always return non-null. But let's be paranoid.
                if (parser == null) {
                    throw new AssertionError("block.newParser() returned a null parser");
                }
                return parser;
            }
        }
    }

}