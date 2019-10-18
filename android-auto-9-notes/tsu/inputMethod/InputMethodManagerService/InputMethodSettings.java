

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/core/java/com/android/internal/inputmethod/InputMethodUtils.java

public static class InputMethodSettings {
    /***
     * 
     * 
     * copyOnWrite 为 true
     */
    public InputMethodSettings(
        Resources res, ContentResolver resolver,
        HashMap<String, InputMethodInfo> methodMap, ArrayList<InputMethodInfo> methodList,
        @UserIdInt int userId, boolean copyOnWrite) {
        mRes = res;
        mResolver = resolver;
        mMethodMap = methodMap;
        switchCurrentUser(userId, copyOnWrite);
    }


     /**
     * Must be called when the current user is changed.
     *
     * @param userId The user ID.
     * @param copyOnWrite If {@code true}, for each settings key
     * (e.g. {@link Settings.Secure#ACTION_INPUT_METHOD_SUBTYPE_SETTINGS}) we use the actual
     * settings on the {@link Settings.Secure} until we do the first write operation.
     */
    public void switchCurrentUser(@UserIdInt int userId, boolean copyOnWrite) {
        if (DEBUG) {
            Slog.d(TAG, "--- Switch the current user from " + mCurrentUserId + " to " + userId);
        }
        if (mCurrentUserId != userId || mCopyOnWrite != copyOnWrite) {
            mCopyOnWriteDataStore.clear();
            mEnabledInputMethodsStrCache = "";
            // TODO: mCurrentProfileIds should be cleared here.
        }
        mCurrentUserId = userId;
        mCopyOnWrite = copyOnWrite;
        // TODO: mCurrentProfileIds should be updated here.
    }

}
