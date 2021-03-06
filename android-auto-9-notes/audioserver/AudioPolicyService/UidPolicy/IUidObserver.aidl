
//  frameworks/base/core/java/android/app/IUidObserver.aidl

oneway interface IUidObserver {
    // WARNING: when these transactions are updated, check if they are any callers on the native
    // side. If so, make sure they are using the correct transaction ids and arguments.
    // If a transaction which will also be used on the native side is being inserted, add it to
    // below block of transactions.

    // Since these transactions are also called from native code, these must be kept in sync with
    // the ones in frameworks/native/include/binder/IActivityManager.h
    // =============== Beginning of transactions used on native side as well ======================

    /**
     * Report that there are no longer any processes running for a uid.
     */
    void onUidGone(int uid, boolean disabled);

    /**
     * Report that a uid is now active (no longer idle).
     */
    void onUidActive(int uid);

    /**
     * Report that a uid is idle -- it has either been running in the background for
     * a sufficient period of time, or all of its processes have gone away.
     */
    void onUidIdle(int uid, boolean disabled);

    // =============== End of transactions used on native side as well ============================

    /**
     * General report of a state change of an uid.
     *
     * @param uid The uid for which the state change is being reported.
     * @param procState The updated process state for the uid.
     * @param procStateSeq The sequence no. associated with process state change of the uid,
     *                     see UidRecord.procStateSeq for details.
     */
    void onUidStateChanged(int uid, int procState, long procStateSeq);

    /**
     * Report when the cached state of a uid has changed.
     * If true, a uid has become cached -- that is, it has some active processes that are
     * all in the cached state.  It should be doing as little as possible at this point.
     * If false, that a uid is no longer cached.  This will only be called after
     * onUidCached() has been reported true.  It will happen when either one of its actively
     * running processes is no longer cached, or it no longer has any actively running processes.
     */
    void onUidCachedChanged(int uid, boolean cached);
}