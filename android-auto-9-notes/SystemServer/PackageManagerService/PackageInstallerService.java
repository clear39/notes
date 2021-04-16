
//	@	frameworks/base/services/core/java/com/android/server/pm/PackageInstallerService.java
public class PackageInstallerService extends IPackageInstaller.Stub {

	public PackageInstallerService(Context context, PackageManagerService pm) {
        mContext = context;
        mPm = pm;
        mPermissionManager = LocalServices.getService(PermissionManagerInternal.class);

        mInstallThread = new HandlerThread(TAG);
        mInstallThread.start();

        mInstallHandler = new Handler(mInstallThread.getLooper());

        mCallbacks = new Callbacks(mInstallThread.getLooper());

        mSessionsFile = new AtomicFile(new File(Environment.getDataSystemDirectory(), "install_sessions.xml"),"package-session");
        mSessionsDir = new File(Environment.getDataSystemDirectory(), "install_sessions");
        mSessionsDir.mkdirs();
    }



	@Override
    public int createSession(SessionParams params, String installerPackageName, int userId) {
        try {
            return createSessionInternal(params, installerPackageName, userId);
        } catch (IOException e) {
            throw ExceptionUtils.wrap(e);
        }
    }

    private int createSessionInternal(SessionParams params, String installerPackageName, int userId) throws IOException {
        final int callingUid = Binder.getCallingUid();
        mPermissionManager.enforceCrossUserPermission(callingUid, userId, true, true, "createSession");

        if (mPm.isUserRestricted(userId, UserManager.DISALLOW_INSTALL_APPS)) {
            throw new SecurityException("User restriction prevents installing");
        }

        // INSTALL_INTERNAL = 0x00000010;
        // INSTALL_FROM_ADB = 0x00000020;
        // INSTALL_ALL_USERS = 0x00000040;
        if ((callingUid == Process.SHELL_UID) || (callingUid == Process.ROOT_UID)) {
            params.installFlags |= PackageManager.INSTALL_FROM_ADB;
        } else {
            // Only apps with INSTALL_PACKAGES are allowed to set an installer that is not the
            // caller.
            if (mContext.checkCallingOrSelfPermission(Manifest.permission.INSTALL_PACKAGES) != PackageManager.PERMISSION_GRANTED) {
                mAppOps.checkPackage(callingUid, installerPackageName);
            }

            params.installFlags &= ~PackageManager.INSTALL_FROM_ADB;
            params.installFlags &= ~PackageManager.INSTALL_ALL_USERS;
            params.installFlags |= PackageManager.INSTALL_REPLACE_EXISTING;
            if ((params.installFlags & PackageManager.INSTALL_VIRTUAL_PRELOAD) != 0
                    && !mPm.isCallerVerifier(callingUid)) {
                params.installFlags &= ~PackageManager.INSTALL_VIRTUAL_PRELOAD;
            }
        }

        // Only system components can circumvent runtime permissions when installing.
        if ((params.installFlags & PackageManager.INSTALL_GRANT_RUNTIME_PERMISSIONS) != 0
                && mContext.checkCallingOrSelfPermission(Manifest.permission.INSTALL_GRANT_RUNTIME_PERMISSIONS) == PackageManager.PERMISSION_DENIED) {
            throw new SecurityException("You need the "
                    + "android.permission.INSTALL_GRANT_RUNTIME_PERMISSIONS permission "
                    + "to use the PackageManager.INSTALL_GRANT_RUNTIME_PERMISSIONS flag");
        }

        if ((params.installFlags & PackageManager.INSTALL_FORWARD_LOCK) != 0
                || (params.installFlags & PackageManager.INSTALL_EXTERNAL) != 0) {
            throw new IllegalArgumentException(
                    "New installs into ASEC containers no longer supported");
        }

        // Defensively resize giant app icons
        if (params.appIcon != null) {
            final ActivityManager am = (ActivityManager) mContext.getSystemService(
                    Context.ACTIVITY_SERVICE);
            final int iconSize = am.getLauncherLargeIconSize();
            if ((params.appIcon.getWidth() > iconSize * 2)
                    || (params.appIcon.getHeight() > iconSize * 2)) {
                params.appIcon = Bitmap.createScaledBitmap(params.appIcon, iconSize, iconSize,
                        true);
            }
        }

        switch (params.mode) {
            case SessionParams.MODE_FULL_INSTALL:
            case SessionParams.MODE_INHERIT_EXISTING:
                break;
            default:
                throw new IllegalArgumentException("Invalid install mode: " + params.mode);
        }

        // If caller requested explicit location, sanity check it, otherwise
        // resolve the best internal or adopted location.
        if ((params.installFlags & PackageManager.INSTALL_INTERNAL) != 0) {
            if (!PackageHelper.fitsOnInternal(mContext, params)) {
                throw new IOException("No suitable internal storage available");
            }

        } else if ((params.installFlags & PackageManager.INSTALL_EXTERNAL) != 0) {
            if (!PackageHelper.fitsOnExternal(mContext, params)) {
                throw new IOException("No suitable external storage available");
            }

        } else if ((params.installFlags & PackageManager.INSTALL_FORCE_VOLUME_UUID) != 0) {
            // For now, installs to adopted media are treated as internal from
            // an install flag point-of-view.
            params.setInstallFlagsInternal();

        } else {
            // For now, installs to adopted media are treated as internal from
            // an install flag point-of-view.
            params.setInstallFlagsInternal();

            // Resolve best location for install, based on combination of
            // requested install flags, delta size, and manifest settings.
            final long ident = Binder.clearCallingIdentity();
            try {
                params.volumeUuid = PackageHelper.resolveInstallVolume(mContext, params);
            } finally {
                Binder.restoreCallingIdentity(ident);
            }
        }

        final int sessionId;
        final PackageInstallerSession session;
        synchronized (mSessions) {
            // Sanity check that installer isn't going crazy
            final int activeCount = getSessionCount(mSessions, callingUid);
            if (activeCount >= MAX_ACTIVE_SESSIONS) {
                throw new IllegalStateException("Too many active sessions for UID " + callingUid);
            }
            final int historicalCount = mHistoricalSessionsByInstaller.get(callingUid);
            if (historicalCount >= MAX_HISTORICAL_SESSIONS) {
                throw new IllegalStateException("Too many historical sessions for UID " + callingUid);
            }

            sessionId = allocateSessionIdLocked();
        }

        final long createdMillis = System.currentTimeMillis();
        // We're staging to exactly one location
        File stageDir = null;
        String stageCid = null;
        // public static final int INSTALL_INTERNAL = 0x00000010;
        if ((params.installFlags & PackageManager.INSTALL_INTERNAL) != 0) {
            final boolean isInstant = (params.installFlags & PackageManager.INSTALL_INSTANT_APP) != 0;
            stageDir = buildStageDir(params.volumeUuid, sessionId, isInstant);
        } else {
            stageCid = buildExternalStageCid(sessionId);
        }

        session = new PackageInstallerSession(mInternalCallback, mContext, mPm,
                mInstallThread.getLooper(), sessionId, userId, installerPackageName, callingUid,
                params, createdMillis, stageDir, stageCid, false, false);

        synchronized (mSessions) {
            mSessions.put(sessionId, session);
        }

        mCallbacks.notifySessionCreated(session.sessionId, session.userId);
        writeSessionsAsync();
        return sessionId;
    }


     @Override
    public IPackageInstallerSession openSession(int sessionId) {
        try {
            return openSessionInternal(sessionId);
        } catch (IOException e) {
            throw ExceptionUtils.wrap(e);
        }
    }

    private IPackageInstallerSession openSessionInternal(int sessionId) throws IOException {
        synchronized (mSessions) {
            final PackageInstallerSession session = mSessions.get(sessionId);
            if (session == null || !isCallingUidOwner(session)) {
                throw new SecurityException("Caller has no access to session " + sessionId);
            }
            session.open();
            return session;
        }
    }

}