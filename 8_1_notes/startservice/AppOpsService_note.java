public class AppOpsService extends IAppOpsService.Stub {
	 @Override
    public int noteOperation(int code, int uid, String packageName) {
        verifyIncomingUid(uid);
        verifyIncomingOp(code);
        String resolvedPackageName = resolvePackageName(uid, packageName);
        if (resolvedPackageName == null) {
            return AppOpsManager.MODE_IGNORED;
        }
        return noteOperationUnchecked(code, uid, resolvedPackageName, 0, null);
    }

    private int noteOperationUnchecked(int code, int uid, String packageName,
            int proxyUid, String proxyPackageName) {
        synchronized (this) {
            Ops ops = getOpsRawLocked(uid, packageName, true);
            if (ops == null) {
                if (DEBUG) Log.d(TAG, "noteOperation: no op for code " + code + " uid " + uid
                        + " package " + packageName);
                return AppOpsManager.MODE_ERRORED;
            }
            Op op = getOpLocked(ops, code, true);
            if (isOpRestrictedLocked(uid, code, packageName)) {
                return AppOpsManager.MODE_IGNORED;
            }
            if (op.duration == -1) {
                Slog.w(TAG, "Noting op not finished: uid " + uid + " pkg " + packageName
                        + " code " + code + " time=" + op.time + " duration=" + op.duration);
            }
            op.duration = 0;
            final int switchCode = AppOpsManager.opToSwitch(code);
            UidState uidState = ops.uidState;
            // If there is a non-default per UID policy (we set UID op mode only if
            // non-default) it takes over, otherwise use the per package policy.
            if (uidState.opModes != null && uidState.opModes.indexOfKey(switchCode) >= 0) {
                final int uidMode = uidState.opModes.get(switchCode);
                if (uidMode != AppOpsManager.MODE_ALLOWED) {
                    if (DEBUG) Log.d(TAG, "noteOperation: reject #" + op.mode + " for code "
                            + switchCode + " (" + code + ") uid " + uid + " package "
                            + packageName);
                    op.rejectTime = System.currentTimeMillis();
                    return uidMode;
                }
            } else {
                final Op switchOp = switchCode != code ? getOpLocked(ops, switchCode, true) : op;
                if (switchOp.mode != AppOpsManager.MODE_ALLOWED) {
                    if (DEBUG) Log.d(TAG, "noteOperation: reject #" + op.mode + " for code "
                            + switchCode + " (" + code + ") uid " + uid + " package "
                            + packageName);
                    op.rejectTime = System.currentTimeMillis();
                    return switchOp.mode;
                }
            }
            if (DEBUG) Log.d(TAG, "noteOperation: allowing code " + code + " uid " + uid
                    + " package " + packageName);
            op.time = System.currentTimeMillis();
            op.rejectTime = 0;
            op.proxyUid = proxyUid;
            op.proxyPackageName = proxyPackageName;
            return AppOpsManager.MODE_ALLOWED;
        }
    }
}