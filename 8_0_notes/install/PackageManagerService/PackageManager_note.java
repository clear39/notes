//PackageManager为抽象类,真正实现类ApplicationPackageManager  @rameworks/base/core/java/android/app/ApplicationPackageManager.java





//	@frameworks/base/core/java/android/content/pm/PackageManager.java
    /**
     * Retrieve overall information about an application package defined in a
     * package archive file
     *
     * @param archiveFilePath The path to the archive file
     * @param flags Additional option flags to modify the data returned.
     * @return A PackageInfo object containing information about the package
     *         archive. If the package could not be parsed, returns null.
     */
    public PackageInfo getPackageArchiveInfo(String archiveFilePath, @PackageInfoFlags int flags) {
        final PackageParser parser = new PackageParser();
        parser.setCallback(new PackageParser.CallbackImpl(this));
        final File apkFile = new File(archiveFilePath);
        try {
            if ((flags & (MATCH_DIRECT_BOOT_UNAWARE | MATCH_DIRECT_BOOT_AWARE)) != 0) {
                // Caller expressed an explicit opinion about what encryption
                // aware/unaware components they want to see, so fall through and
                // give them what they want
            } else {
                // Caller expressed no opinion, so match everything
                flags |= MATCH_DIRECT_BOOT_AWARE | MATCH_DIRECT_BOOT_UNAWARE;
            }

            PackageParser.Package pkg = parser.parseMonolithicPackage(apkFile, 0);
            if ((flags & GET_SIGNATURES) != 0) {
                PackageParser.collectCertificates(pkg, 0);
            }
            PackageUserState state = new PackageUserState();
            return PackageParser.generatePackageInfo(pkg, null, flags, 0, 0, null, state);
        } catch (PackageParserException e) {
            return null;
        }
    }
