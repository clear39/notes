

public static final class InputMethodManagerService.Lifecycle extends SystemService {
    private InputMethodManagerService mService;

    public Lifecycle(Context context) {
        super(context);
        mService = new InputMethodManagerService(context);
    }

    @Override
    public void onStart() {
        LocalServices.addService(InputMethodManagerInternal.class,new LocalServiceImpl(mService.mHandler));
        publishBinderService(Context.INPUT_METHOD_SERVICE, mService);
    }

    @Override
    public void onSwitchUser(@UserIdInt int userHandle) {
        // Called on ActivityManager thread.
        // TODO: Dispatch this to a worker thread as needed.
        mService.onSwitchUser(userHandle);
    }

    @Override
    public void onBootPhase(int phase) {
        // Called on ActivityManager thread.
        // TODO: Dispatch this to a worker thread as needed.
        if (phase == SystemService.PHASE_ACTIVITY_MANAGER_READY) {
            StatusBarManagerService statusBarService = (StatusBarManagerService) ServiceManager.getService(Context.STATUS_BAR_SERVICE);
            mService.systemRunning(statusBarService);
        }
    }

    @Override
    public void onUnlockUser(final @UserIdInt int userHandle) {
        // Called on ActivityManager thread.
        mService.mHandler.sendMessage(mService.mHandler.obtainMessage(MSG_SYSTEM_UNLOCK_USER,userHandle /* arg1 */, 0 /* arg2 */));
    }
}











//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/services/core/java/com/android/server/InputMethodManagerService.java
public class InputMethodManagerService extends IInputMethodManager.Stub
        implements ServiceConnection, Handler.Callback {

    public InputMethodManagerService(Context context) {
        mIPackageManager = AppGlobals.getPackageManager();
        mContext = context;
        mRes = context.getResources();
        mHandler = new Handler(this);
        // Note: SettingsObserver doesn't register observers in its constructor.
        mSettingsObserver = new SettingsObserver(mHandler);
        mIWindowManager = IWindowManager.Stub.asInterface(ServiceManager.getService(Context.WINDOW_SERVICE));
        mWindowManagerInternal = LocalServices.getService(WindowManagerInternal.class);
        mCaller = new HandlerCaller(context, null, new HandlerCaller.Callback() {
            @Override
            public void executeMessage(Message msg) {
                handleMessage(msg);
            }
        }, true /*asyncHandler*/);
        mAppOpsManager = mContext.getSystemService(AppOpsManager.class);
        mUserManager = mContext.getSystemService(UserManager.class);
        mHardKeyboardListener = new HardKeyboardListener();
        /***
         * 
         */
        mHasFeature = context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_INPUT_METHODS);
        /***
         * frameworks/base/core/res/res/values/config.xml:65:    <string translatable="false" name="status_bar_ime">ime</string>
         */
        mSlotIme = mContext.getString(com.android.internal.R.string.status_bar_ime);
        /***
         * frameworks/base/core/res/res/values/config.xml:3174:    <integer name="config_externalHardKeyboardBehavior">0</integer>
         */
        mHardKeyboardBehavior = mContext.getResources().getInteger(com.android.internal.R.integer.config_externalHardKeyboardBehavior);

        Bundle extras = new Bundle();
        extras.putBoolean(Notification.EXTRA_ALLOW_DURING_SETUP, true);
        /**
         * frameworks/base/core/res/res/values/colors.xml:162:    <color name="system_notification_accent_color">#ff607D8B</color>
         */
        @ColorInt final int accentColor = mContext.getColor(com.android.internal.R.color.system_notification_accent_color);
        mImeSwitcherNotification =new Notification.Builder(mContext, SystemNotificationChannels.VIRTUAL_KEYBOARD)
                                        .setSmallIcon(com.android.internal.R.drawable.ic_notification_ime_default)
                                        .setWhen(0)
                                        .setOngoing(true)
                                        .addExtras(extras)
                                        .setCategory(Notification.CATEGORY_SYSTEM)
                                        .setColor(accentColor);
        /*
        // 
        //  A protected broadcast intent action for internal use for {@link PendingIntent} in
        //  the notification.
        // 
        //private static final String ACTION_SHOW_INPUT_METHOD_PICKER = "com.android.server.InputMethodManagerService.SHOW_INPUT_METHOD_PICKER";
        */
        Intent intent = new Intent(ACTION_SHOW_INPUT_METHOD_PICKER).setPackage(mContext.getPackageName());
        mImeSwitchPendingIntent = PendingIntent.getBroadcast(mContext, 0, intent, 0);

        mShowOngoingImeSwitcherForPhones = false;

        mNotificationShown = false;
        int userId = 0;
        try {
            userId = ActivityManager.getService().getCurrentUser().id;
        } catch (RemoteException e) {
            Slog.w(TAG, "Couldn't get current user ID; guessing it's 0", e);
        }

        // mSettings should be created before buildInputMethodListLocked
        mSettings = new InputMethodSettings(mRes, context.getContentResolver(), mMethodMap, mMethodList, userId, !mSystemReady);

        updateCurrentProfileIds();
        mFileManager = new InputMethodFileManager(mMethodMap, userId);
        mSwitchingController = InputMethodSubtypeSwitchingController.createInstanceLocked(mSettings, context);
        // Register VR-state listener.
        IVrManager vrManager = (IVrManager) ServiceManager.getService(Context.VR_SERVICE);
        if (vrManager != null) {
            try {
                vrManager.registerListener(mVrStateCallbacks);
            } catch (RemoteException e) {
                Slog.e(TAG, "Failed to register VR mode state listener.");
            }
        }
    }


    public void systemRunning(StatusBarManagerService statusBar) {
        synchronized (mMethodMap) {
            if (DEBUG) {
                Slog.d(TAG, "--- systemReady");
            }
            if (!mSystemReady) {
                mSystemReady = true;
                mLastSystemLocales = mRes.getConfiguration().getLocales();
                final int currentUserId = mSettings.getCurrentUserId();
                mSettings.switchCurrentUser(currentUserId,!mUserManager.isUserUnlockingOrUnlocked(currentUserId));
                mKeyguardManager = mContext.getSystemService(KeyguardManager.class);
                mNotificationManager = mContext.getSystemService(NotificationManager.class);
                mStatusBar = statusBar;
                if (mStatusBar != null) {
                    mStatusBar.setIconVisibility(mSlotIme, false);
                }
                updateSystemUiLocked(mCurToken, mImeWindowVis, mBackDisposition);
                mShowOngoingImeSwitcherForPhones = mRes.getBoolean(com.android.internal.R.bool.show_ongoing_ime_switcher);
                if (mShowOngoingImeSwitcherForPhones) {
                    mWindowManagerInternal.setOnHardKeyboardStatusChangeListener( mHardKeyboardListener);
                }

                mMyPackageMonitor.register(mContext, null, UserHandle.ALL, true);
                mSettingsObserver.registerContentObserverLocked(currentUserId);

                final IntentFilter broadcastFilter = new IntentFilter();
                broadcastFilter.addAction(Intent.ACTION_CLOSE_SYSTEM_DIALOGS);
                broadcastFilter.addAction(Intent.ACTION_USER_ADDED);
                broadcastFilter.addAction(Intent.ACTION_USER_REMOVED);
                broadcastFilter.addAction(Intent.ACTION_LOCALE_CHANGED);
                broadcastFilter.addAction(ACTION_SHOW_INPUT_METHOD_PICKER);
                mContext.registerReceiver(new ImmsBroadcastReceiver(), broadcastFilter);

                final String defaultImiId = mSettings.getSelectedInputMethod();
                final boolean imeSelectedOnBoot = !TextUtils.isEmpty(defaultImiId);
                buildInputMethodListLocked(!imeSelectedOnBoot /* resetDefaultEnabledIme */);
                resetDefaultImeLocked(mContext);
                updateFromSettingsLocked(true);
                InputMethodUtils.setNonSelectedSystemImesDisabledUntilUsed(mIPackageManager,
                        mSettings.getEnabledInputMethodListLocked(), currentUserId,
                        mContext.getBasePackageName());

                try {
                    /***
                     * 启动默认的输入法Service
                     */
                    startInputInnerLocked();
                } catch (RuntimeException e) {
                    Slog.w(TAG, "Unexpected exception", e);
                }
            }
        }
    }


    InputBindResult startInputInnerLocked() {
        if (mCurMethodId == null) {
            return InputBindResult.NO_IME;
        }

        if (!mSystemReady) {
            // If the system is not yet ready, we shouldn't be running third
            // party code.
            return new InputBindResult(
                    InputBindResult.ResultCode.ERROR_SYSTEM_NOT_READY,
                    null, null, mCurMethodId, mCurSeq,
                    mCurUserActionNotificationSequenceNumber);
        }

        InputMethodInfo info = mMethodMap.get(mCurMethodId);
        if (info == null) {
            throw new IllegalArgumentException("Unknown id: " + mCurMethodId);
        }

        unbindCurrentMethodLocked(true);

        mCurIntent = new Intent(InputMethod.SERVICE_INTERFACE);
        mCurIntent.setComponent(info.getComponent());
        mCurIntent.putExtra(Intent.EXTRA_CLIENT_LABEL,com.android.internal.R.string.input_method_binding_label);
        mCurIntent.putExtra(Intent.EXTRA_CLIENT_INTENT, PendingIntent.getActivity(mContext, 0, new Intent(Settings.ACTION_INPUT_METHOD_SETTINGS), 0));
        if (bindCurrentInputMethodServiceLocked(mCurIntent, this, IME_CONNECTION_BIND_FLAGS)) {
            mLastBindTime = SystemClock.uptimeMillis();
            mHaveConnection = true;
            mCurId = info.getId();
            mCurToken = new Binder();
            try {
                if (DEBUG) Slog.v(TAG, "Adding window token: " + mCurToken);
                mIWindowManager.addWindowToken(mCurToken, TYPE_INPUT_METHOD, DEFAULT_DISPLAY);
            } catch (RemoteException e) {
            }
            return new InputBindResult(
                    InputBindResult.ResultCode.SUCCESS_WAITING_IME_BINDING,
                    null, null, mCurId, mCurSeq,
                    mCurUserActionNotificationSequenceNumber);
        }
        mCurIntent = null;
        Slog.w(TAG, "Failure connecting to input method service: " + mCurIntent);
        return InputBindResult.IME_NOT_CONNECTED;
    }















    /***
     * WindowManagerGlobal.getWindowSession-->WindowManagerService.openSession--> new Session
     * --> InputMethodManagerService.addClient
     */
    @Override
    public void addClient(IInputMethodClient client,
            IInputContext inputContext, int uid, int pid) {
        if (!calledFromValidUser()) {
            return;
        }
        synchronized (mMethodMap) {
            mClients.put(client.asBinder(), new ClientState(client,inputContext, uid, pid));
        }
    }
}