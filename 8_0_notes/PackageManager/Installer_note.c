public class Installer extends SystemService {

    /**
     * @param isolated indicates if this object should <em>not</em> connect to
     *            the real {@code installd}. All remote calls will be ignored
     *            unless you extend this class and intercept them.
     */
    public Installer(Context context, boolean isolated) {  //初始化第一步创建
        super(context);
        mIsolated = isolated;
    }


    public void onStart() {//初始化第二步调用
        if (mIsolated) {
            mInstalld = null;
        } else {
            connect();
        }
    }

    private void connect() {
        IBinder binder = ServiceManager.getService("installd");
        if (binder != null) {
            try {
                binder.linkToDeath(new DeathRecipient() {
                    @Override
                    public void binderDied() {
                        Slog.w(TAG, "installd died; reconnecting");
                        connect();
                    }
                }, 0);
            } catch (RemoteException e) {
                binder = null;
            }
        }

        if (binder != null) {
            mInstalld = IInstalld.Stub.asInterface(binder);
            try {
                invalidateMounts();
            } catch (InstallerException ignored) {
            }
        } else {
            Slog.w(TAG, "installd not found; trying again");
            BackgroundThread.getHandler().postDelayed(() -> {
                connect();
            }, DateUtils.SECOND_IN_MILLIS);
        }
    }
   
    public void invalidateMounts() throws InstallerException {
        if (!checkBeforeRemote()) return;
        try {
            mInstalld.invalidateMounts();
        } catch (Exception e) {
            throw InstallerException.from(e);
        }
    }


}
