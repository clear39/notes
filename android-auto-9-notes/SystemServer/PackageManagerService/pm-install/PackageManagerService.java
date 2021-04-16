

//	@	frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java





 @Override
    public IPackageInstaller getPackageInstaller() {
        if (getInstantAppPackageName(Binder.getCallingUid()) != null) {
            return null;
        }
        return mInstallerService;
    }