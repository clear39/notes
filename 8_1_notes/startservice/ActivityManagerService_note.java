public class ActivityManagerService extends IActivityManager.Stub
        implements Watchdog.Monitor, BatteryStatsImpl.BatteryCallback {



 	@Override
    public ComponentName startService(IApplicationThread caller, Intent service,
            String resolvedType, boolean requireForeground, String callingPackage, int userId)
            throws TransactionTooLargeException {
        enforceNotIsolatedCaller("startService");
        // Refuse possible leaked file descriptors
        if (service != null && service.hasFileDescriptors() == true) {
            throw new IllegalArgumentException("File descriptors passed in Intent");
        }

        if (callingPackage == null) {
            throw new IllegalArgumentException("callingPackage cannot be null");
        }

        if (DEBUG_SERVICE) Slog.v(TAG_SERVICE,
                "*** startService: " + service + " type=" + resolvedType + " fg=" + requireForeground);
        synchronized(this) {
            final int callingPid = Binder.getCallingPid();
            final int callingUid = Binder.getCallingUid();
            final long origId = Binder.clearCallingIdentity();
            ComponentName res;
            try {
                res = mServices.startServiceLocked(caller, service,
                        resolvedType, callingPid, callingUid,
                        requireForeground, callingPackage, userId);
            } finally {
                Binder.restoreCallingIdentity(origId);
            }
            return res;
        }
    }


    int getAppStartModeLocked(int uid, String packageName, int packageTargetSdk,int callingPid, boolean alwaysRestrict, boolean disabledOnly) {
        UidRecord uidRec = mActiveUids.get(uid);
        if (DEBUG_BACKGROUND_CHECK) Slog.d(TAG, "checkAllowBackground: uid=" + uid + " pkg="
                + packageName + " rec=" + uidRec + " always=" + alwaysRestrict + " idle=" + (uidRec != null ? uidRec.idle : false));
        if (uidRec == null || alwaysRestrict || uidRec.idle) {
            boolean ephemeral;
            if (uidRec == null) {
                ephemeral = getPackageManagerInternalLocked().isPackageEphemeral(UserHandle.getUserId(uid), packageName);
            } else {
                ephemeral = uidRec.ephemeral;
            }

            if (ephemeral) {
                // We are hard-core about ephemeral apps not running in the background.
                return ActivityManager.APP_START_MODE_DISABLED;
            } else {
                if (disabledOnly) {
                    // The caller is only interested in whether app starts are completely
                    // disabled for the given package (that is, it is an instant app).  So
                    // we don't need to go further, which is all just seeing if we should
                    // apply a "delayed" mode for a regular app.
                    return ActivityManager.APP_START_MODE_NORMAL;
                }
                final int startMode = (alwaysRestrict)
                        ? appRestrictedInBackgroundLocked(uid, packageName, packageTargetSdk)
                        : appServicesRestrictedInBackgroundLocked(uid, packageName,
                                packageTargetSdk);
                if (DEBUG_BACKGROUND_CHECK) Slog.d(TAG, "checkAllowBackground: uid=" + uid
                        + " pkg=" + packageName + " startMode=" + startMode
                        + " onwhitelist=" + isOnDeviceIdleWhitelistLocked(uid));
                if (startMode == ActivityManager.APP_START_MODE_DELAYED) {
                    // This is an old app that has been forced into a "compatible as possible"
                    // mode of background check.  To increase compatibility, we will allow other
                    // foreground apps to cause its services to start.
                    if (callingPid >= 0) {
                        ProcessRecord proc;
                        synchronized (mPidsSelfLocked) {
                            proc = mPidsSelfLocked.get(callingPid);
                        }
                        if (proc != null &&
                                !ActivityManager.isProcStateBackground(proc.curProcState)) {
                            // Whoever is instigating this is in the foreground, so we will allow it
                            // to go through.
                            return ActivityManager.APP_START_MODE_NORMAL;
                        }
                    }
                }
                return startMode;
            }
        }
        return ActivityManager.APP_START_MODE_NORMAL;
    }


    // Service launch is available to apps with run-in-background exemptions but
    // some other background operations are not.  If we're doing a check
    // of service-launch policy, allow those callers to proceed unrestricted.
    int appServicesRestrictedInBackgroundLocked(int uid, String packageName, int packageTargetSdk) {
        // Persistent app?
        if (mPackageManagerInt.isPackagePersistent(packageName)) {
            if (DEBUG_BACKGROUND_CHECK) {
                Slog.i(TAG, "App " + uid + "/" + packageName
                        + " is persistent; not restricted in background");
            }
            return ActivityManager.APP_START_MODE_NORMAL;
        }

        // Non-persistent but background whitelisted?
        if (uidOnBackgroundWhitelist(uid)) {
            if (DEBUG_BACKGROUND_CHECK) {
                Slog.i(TAG, "App " + uid + "/" + packageName
                        + " on background whitelist; not restricted in background");
            }
            return ActivityManager.APP_START_MODE_NORMAL;
        }

        // Is this app on the battery whitelist?
        if (isOnDeviceIdleWhitelistLocked(uid)) {
            if (DEBUG_BACKGROUND_CHECK) {
                Slog.i(TAG, "App " + uid + "/" + packageName
                        + " on idle whitelist; not restricted in background");
            }
            return ActivityManager.APP_START_MODE_NORMAL;
        }

        // None of the service-policy criteria apply, so we apply the common criteria
        return appRestrictedInBackgroundLocked(uid, packageName, packageTargetSdk);
    }

        // Unified app-op and target sdk check
    int appRestrictedInBackgroundLocked(int uid, String packageName, int packageTargetSdk) {
        // Apps that target O+ are always subject to background check
        if (packageTargetSdk >= Build.VERSION_CODES.O) {
            if (DEBUG_BACKGROUND_CHECK) {
                Slog.i(TAG, "App " + uid + "/" + packageName + " targets O+, restricted");
            }
            return ActivityManager.APP_START_MODE_DELAYED_RIGID;
        }
        // ...and legacy apps get an AppOp check
        int appop = mAppOpsService.noteOperation(AppOpsManager.OP_RUN_IN_BACKGROUND,uid, packageName);
        if (DEBUG_BACKGROUND_CHECK) {
            Slog.i(TAG, "Legacy app " + uid + "/" + packageName + " bg appop " + appop);
        }
        switch (appop) {
            case AppOpsManager.MODE_ALLOWED:
                return ActivityManager.APP_START_MODE_NORMAL;
            case AppOpsManager.MODE_IGNORED:
                return ActivityManager.APP_START_MODE_DELAYED;
            default:
                return ActivityManager.APP_START_MODE_DELAYED_RIGID;
        }
    }

}

public final class ActiveServices {

	ComponentName startServiceLocked(IApplicationThread caller, Intent service, String resolvedType,
            int callingPid, int callingUid, boolean fgRequired, String callingPackage, final int userId)
            throws TransactionTooLargeException {
        if (DEBUG_DELAYED_STARTS) Slog.v(TAG_SERVICE, "startService: " + service  + " type=" + resolvedType + " args=" + service.getExtras());

        final boolean callerFg;
        if (caller != null) {
            final ProcessRecord callerApp = mAm.getRecordForAppLocked(caller);
            if (callerApp == null) {
                throw new SecurityException("Unable to find app for caller " + caller  + " (pid=" + callingPid + ") when starting service " + service);
            }
            callerFg = callerApp.setSchedGroup != ProcessList.SCHED_GROUP_BACKGROUND;
        } else {
            callerFg = true;
        }

        ServiceLookupResult res = retrieveServiceLocked(service, resolvedType, callingPackage,callingPid, callingUid, userId, true, callerFg, false);
        if (res == null) {
            return null;
        }
        if (res.record == null) {
            return new ComponentName("!", res.permission != null ? res.permission : "private to package");
        }

        ServiceRecord r = res.record;

        if (!mAm.mUserController.exists(r.userId)) {
            Slog.w(TAG, "Trying to start service with non-existent user! " + r.userId);
            return null;
        }

        // If this isn't a direct-to-foreground start, check our ability to kick off an
        // arbitrary service
        if (!r.startRequested && !fgRequired) {
            // Before going further -- if this app is not allowed to start services in the
            // background, then at this point we aren't going to let it period.
            final int allowed = mAm.getAppStartModeLocked(r.appInfo.uid, r.packageName,r.appInfo.targetSdkVersion, callingPid, false, false);
            if (allowed != ActivityManager.APP_START_MODE_NORMAL) {
                Slog.w(TAG, "Background start not allowed: service " + service + " to " + r.name.flattenToShortString()
                        + " from pid=" + callingPid + " uid=" + callingUid + " pkg=" + callingPackage);
                if (allowed == ActivityManager.APP_START_MODE_DELAYED) {
                    // In this case we are silently disabling the app, to disrupt as
                    // little as possible existing apps.
                    return null;
                }
                // This app knows it is in the new model where this operation is not
                // allowed, so tell it what has happened.
                UidRecord uidRec = mAm.mActiveUids.get(r.appInfo.uid);
                return new ComponentName("?", "app is in background uid " + uidRec);
            }
        }

        ......

        ComponentName cmp = startServiceInnerLocked(smap, service, r, callerFg, addToStarting);
        return cmp;
    }

}



