//  @   frameworks/base/services/core/java/com/android/server/trust/TrustManagerService.java
private final IBinder mService = new ITrustManager.Stub() {
    @Override
    public boolean isDeviceLocked(int userId) throws RemoteException {
        userId = ActivityManager.handleIncomingUser(getCallingPid(), getCallingUid(), userId,
                false /* allowAll */, true /* requireFull */, "isDeviceLocked", null);

        long token = Binder.clearCallingIdentity();
        try {
            if (!mLockPatternUtils.isSeparateProfileChallengeEnabled(userId)) {
                userId = resolveProfileParent(userId);
            }
            return isDeviceLockedInner(userId);
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }
}


public class TrustManagerService extends SystemService {

    public TrustManagerService(Context context) {
        super(context);
        mContext = context;
        mUserManager = (UserManager) mContext.getSystemService(Context.USER_SERVICE);
        mActivityManager = (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);
        mLockPatternUtils = new LockPatternUtils(context);
        mStrongAuthTracker = new StrongAuthTracker(context);
    }

    boolean isDeviceLockedInner(int userId) {
        synchronized (mDeviceLockedForUser) {
            return mDeviceLockedForUser.get(userId, true);
        }
    }



}

