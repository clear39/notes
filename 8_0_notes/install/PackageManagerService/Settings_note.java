
//	@frameworks/base/services/core/java/com/android/server/pm/Settings.java
final class Settings {

	//  lock = 	mPackages			//	final ArrayMap<String, PackageParser.Package> mPackages = new ArrayMap<String, PackageParser.Package>();
	Settings(Object lock) {
        this(Environment.getDataDirectory(), lock);
    }


    Settings(File dataDir, Object lock) {	// dataDir = "/data"
        mLock = lock;

        mRuntimePermissionsPersistence = new RuntimePermissionPersistence(mLock);

        mSystemDir = new File(dataDir, "system"); //	mSystemDir = "/data/system"
        mSystemDir.mkdirs();
        FileUtils.setPermissions(mSystemDir.toString(),FileUtils.S_IRWXU|FileUtils.S_IRWXG|FileUtils.S_IROTH|FileUtils.S_IXOTH, -1, -1);
        mSettingsFilename = new File(mSystemDir, "packages.xml");	//	mSettingsFilename = "/data/system/packages.xml"
        mBackupSettingsFilename = new File(mSystemDir, "packages-backup.xml");//	mBackupSettingsFilename = "/data/system/packages-backup..xml"
        mPackageListFilename = new File(mSystemDir, "packages.list");//	mPackageListFilename = "/data/system/packages.list"
        FileUtils.setPermissions(mPackageListFilename, 0640, SYSTEM_UID, PACKAGE_INFO_GID);

        final File kernelDir = new File("/config/sdcardfs");
        mKernelMappingFilename = kernelDir.exists() ? kernelDir : null;

        // Deprecated: Needed for migration
        mStoppedPackagesFilename = new File(mSystemDir, "packages-stopped.xml");//	mStoppedPackagesFilename = "/data/system/packages-stopped.xml
        mBackupStoppedPackagesFilename = new File(mSystemDir, "packages-stopped-backup.xml");//	mStoppedPackagesFilename = "/data/system/packages-stopped-backup.xml
    }


    SharedUserSetting addSharedUserLPw(String name, int uid, int pkgFlags, int pkgPrivateFlags) {
        SharedUserSetting s = mSharedUsers.get(name);
        if (s != null) {
            if (s.userId == uid) {
                return s;
            }
            PackageManagerService.reportSettingsProblem(Log.ERROR,"Adding duplicate shared user, keeping first: " + name);
            return null;
        }
        s = new SharedUserSetting(name, pkgFlags, pkgPrivateFlags);
        s.userId = uid;
        if (addUserIdLPw(uid, s, name)) {
            mSharedUsers.put(name, s);
            return s;
        }
        return null;
    }

    private boolean addUserIdLPw(int uid, Object obj, Object name) {
        if (uid > Process.LAST_APPLICATION_UID) {
            return false;
        }

        if (uid >= Process.FIRST_APPLICATION_UID) {
            int N = mUserIds.size();
            final int index = uid - Process.FIRST_APPLICATION_UID;
            while (index >= N) {
                mUserIds.add(null);
                N++;
            }
            if (mUserIds.get(index) != null) {
                PackageManagerService.reportSettingsProblem(Log.ERROR, "Adding duplicate user id: " + uid + " name=" + name);
                return false;
            }
            mUserIds.set(index, obj);
        } else {
            if (mOtherUserIds.get(uid) != null) {
                PackageManagerService.reportSettingsProblem(Log.ERROR, "Adding duplicate shared id: " + uid + " name=" + name);
                return false;
            }
            mOtherUserIds.put(uid, obj);
        }
        return true;
    }
}