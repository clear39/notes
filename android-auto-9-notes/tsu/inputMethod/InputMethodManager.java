
//  @   /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/base/core/java/android/view/inputmethod/InputMethodManager.java

public final class InputMethodManager {

    /***
     * 在 WindowManagerGlobal.getWindowSession() 调用
     */
    public static InputMethodManager getInstance() {
        synchronized (InputMethodManager.class) {
            if (sInstance == null) {
                try {
                    sInstance = new InputMethodManager(Looper.getMainLooper());
                } catch (ServiceNotFoundException e) {
                    throw new IllegalStateException(e);
                }
            }
            return sInstance;
        }
    }

    InputMethodManager(Looper looper) throws ServiceNotFoundException {
        /***
         * public static final String INPUT_METHOD_SERVICE = "input_method";
         */
        this(IInputMethodManager.Stub.asInterface(ServiceManager.getServiceOrThrow(Context.INPUT_METHOD_SERVICE)), looper);
    }

    InputMethodManager(IInputMethodManager service, Looper looper) {
        mService = service;
        mMainLooper = looper;
        mH = new H(looper);
        /***
         * 
         */
        mIInputContext = new ControlledInputConnectionWrapper(looper,mDummyInputConnection, this);
    }

    /***
     * 在 WindowManagerGlobal.getWindowSession() 调用，
     * 并且 传递给 WindowManagerService.openSession
     */
    public IInputMethodClient getClient() {
        /***
         * final IInputMethodClient.Stub mClient = new IInputMethodClient.Stub() {
         * 
         */
        return mClient;
    }

    /***
     * 在 WindowManagerGlobal.getWindowSession() 调用，
     * 并且 传递给 WindowManagerService.openSession
     */
    public IInputContext getInputContext() {
        /***
         * 在构造函数中创建
         */
        return mIInputContext;
    }















    boolean startInputInner(@InputMethodClient.StartInputReason final int startInputReason,
        IBinder windowGainingFocus, int controlFlags, int softInputMode,
        int windowFlags) {
        final View view;
        synchronized (mH) {
            view = mServedView;

            // Make sure we have a window token for the served view.
            /***
             * 
             */
            if (DEBUG) {
                Log.v(TAG, "Starting input: view=" + dumpViewInfo(view) +
                        " reason=" + InputMethodClient.getStartInputReason(startInputReason));
            }
            if (view == null) {
                if (DEBUG) Log.v(TAG, "ABORT input: no served view!");
                return false;
            }
        }

        // Now we need to get an input connection from the served view.
        // This is complicated in a couple ways: we can't be holding our lock
        // when calling out to the view, and we need to make sure we call into
        // the view on the same thread that is driving its view hierarchy.
        Handler vh = view.getHandler();
        if (vh == null) {
            // If the view doesn't have a handler, something has changed out
            // from under us, so just close the current input.
            // If we don't close the current input, the current input method can remain on the
            // screen without a connection.
            if (DEBUG) Log.v(TAG, "ABORT input: no handler for view! Close current input.");
            closeCurrentInput();
            return false;
        }
        if (vh.getLooper() != Looper.myLooper()) {
            // The view is running on a different thread than our own, so
            // we need to reschedule our work for over there.
            if (DEBUG) Log.v(TAG, "Starting input: reschedule to view thread");
            vh.post(() -> startInputInner(startInputReason, null, 0, 0, 0));
            return false;
        }

        // Okay we are now ready to call into the served view and have it
        // do its stuff.
        // Life is good: let's hook everything up!
        EditorInfo tba = new EditorInfo();
        // Note: Use Context#getOpPackageName() rather than Context#getPackageName() so that the
        // system can verify the consistency between the uid of this process and package name passed
        // from here. See comment of Context#getOpPackageName() for details.
        tba.packageName = view.getContext().getOpPackageName();
        tba.fieldId = view.getId();
        InputConnection ic = view.onCreateInputConnection(tba);
        if (DEBUG) Log.v(TAG, "Starting input: tba=" + tba + " ic=" + ic);

        synchronized (mH) {
            // Now that we are locked again, validate that our state hasn't
            // changed.
            if (mServedView != view || !mServedConnecting) {
                // Something else happened, so abort.
                if (DEBUG) Log.v(TAG,
                        "Starting input: finished by someone else. view=" + dumpViewInfo(view)
                        + " mServedView=" + dumpViewInfo(mServedView)
                        + " mServedConnecting=" + mServedConnecting);
                return false;
            }

            // If we already have a text box, then this view is already
            // connected so we want to restart it.
            if (mCurrentTextBoxAttribute == null) {
                controlFlags |= CONTROL_START_INITIAL;
            }

            // Hook 'em up and let 'er rip.
            mCurrentTextBoxAttribute = tba;
            mServedConnecting = false;
            if (mServedInputConnectionWrapper != null) {
                mServedInputConnectionWrapper.deactivate();
                mServedInputConnectionWrapper = null;
            }
            ControlledInputConnectionWrapper servedContext;
            final int missingMethodFlags;
            if (ic != null) {
                mCursorSelStart = tba.initialSelStart;
                mCursorSelEnd = tba.initialSelEnd;
                mCursorCandStart = -1;
                mCursorCandEnd = -1;
                mCursorRect.setEmpty();
                mCursorAnchorInfo = null;
                final Handler icHandler;
                missingMethodFlags = InputConnectionInspector.getMissingMethodFlags(ic);
                if ((missingMethodFlags & InputConnectionInspector.MissingMethodFlags.GET_HANDLER)
                        != 0) {
                    // InputConnection#getHandler() is not implemented.
                    icHandler = null;
                } else {
                    icHandler = ic.getHandler();
                }
                servedContext = new ControlledInputConnectionWrapper(
                        icHandler != null ? icHandler.getLooper() : vh.getLooper(), ic, this);
            } else {
                servedContext = null;
                missingMethodFlags = 0;
            }
            mServedInputConnectionWrapper = servedContext;

            try {
                if (DEBUG) Log.v(TAG, "START INPUT: view=" + dumpViewInfo(view) + " ic="
                        + ic + " tba=" + tba + " controlFlags=#"
                        + Integer.toHexString(controlFlags));
                final InputBindResult res = mService.startInputOrWindowGainedFocus(
                        startInputReason, mClient, windowGainingFocus, controlFlags, softInputMode,
                        windowFlags, tba, servedContext, missingMethodFlags,
                        view.getContext().getApplicationInfo().targetSdkVersion);
                if (DEBUG) Log.v(TAG, "Starting input: Bind result=" + res);
                if (res == null) {
                    Log.wtf(TAG, "startInputOrWindowGainedFocus must not return"
                            + " null. startInputReason="
                            + InputMethodClient.getStartInputReason(startInputReason)
                            + " editorInfo=" + tba
                            + " controlFlags=#" + Integer.toHexString(controlFlags));
                    return false;
                }
                if (res.id != null) {
                    setInputChannelLocked(res.channel);
                    mBindSequence = res.sequence;
                    mCurMethod = res.method;
                    mCurId = res.id;
                    mNextUserActionNotificationSequenceNumber =
                            res.userActionNotificationSequenceNumber;
                } else if (res.channel != null && res.channel != mCurChannel) {
                    res.channel.dispose();
                }
                switch (res.result) {
                    case InputBindResult.ResultCode.ERROR_NOT_IME_TARGET_WINDOW:
                        mRestartOnNextWindowFocus = true;
                        break;
                }
                if (mCurMethod != null && mCompletions != null) {
                    try {
                        mCurMethod.displayCompletions(mCompletions);
                    } catch (RemoteException e) {
                    }
                }
            } catch (RemoteException e) {
                Log.w(TAG, "IME died: " + mCurId, e);
            }
        }

        return true;
    }    






}