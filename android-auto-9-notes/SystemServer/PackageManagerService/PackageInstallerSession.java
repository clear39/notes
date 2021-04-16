
// @ frameworks/base/services/core/java/com/android/server/pm/PackageInstallerSession.java
public class PackageInstallerSession extends IPackageInstallerSession.Stub {

	public PackageInstallerSession(PackageInstallerService.InternalCallback callback,
            Context context, PackageManagerService pm, Looper looper, int sessionId, int userId,
            String installerPackageName, int installerUid, SessionParams params, long createdMillis,
            File stageDir, String stageCid, boolean prepared, boolean sealed) {

        mCallback = callback;
        mContext = context;
        mPm = pm;
        mHandler = new Handler(looper, mHandlerCallback);

        this.sessionId = sessionId;
        this.userId = userId;
        mOriginalInstallerUid = installerUid;
        mInstallerPackageName = installerPackageName;
        mInstallerUid = installerUid;
        this.params = params;
        this.createdMillis = createdMillis;
        this.stageDir = stageDir;
        this.stageCid = stageCid;

        if ((stageDir == null) == (stageCid == null)) {
            throw new IllegalArgumentException("Exactly one of stageDir or stageCid stage must be set");
        }

        mPrepared = prepared; // falses

        if (sealed) { // false
            synchronized (mLock) {
                try {
                    sealAndValidateLocked();
                } catch (PackageManagerException | IOException e) {
                    destroyInternal();
                    throw new IllegalArgumentException(e);
                }
            }
        }

        final long identity = Binder.clearCallingIdentity();
        try {
        	// public static final String DEFAULT_CONTAINER_PACKAGE = "com.android.defcontainer";
        	// 	
            final int uid = mPm.getPackageUid(PackageManagerService.DEFAULT_CONTAINER_PACKAGE,PackageManager.MATCH_SYSTEM_ONLY, UserHandle.USER_SYSTEM);
            defaultContainerGid = UserHandle.getSharedAppGid(uid);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        // attempt to bind to the DefContainer as early as possible
        if ((params.installFlags & PackageManager.INSTALL_INSTANT_APP) != 0) {
            mHandler.sendMessage(mHandler.obtainMessage(MSG_EARLY_BIND));
        }
    }

    public void open() throws IOException {
        if (mActiveCount.getAndIncrement() == 0) {
            mCallback.onSessionActiveChanged(this, true);
        }

        boolean wasPrepared;
        synchronized (mLock) {
            wasPrepared = mPrepared;
            if (!mPrepared) {
                if (stageDir != null) {
                    prepareStageDir(stageDir);
                } else {
                    throw new IllegalArgumentException("stageDir must be set");
                }

                mPrepared = true;
            }
        }

        if (!wasPrepared) {
            mCallback.onSessionPrepared(this);
        }
    }


}