public class ConnectivityManager {

    /**
     * Tells the underlying(潜在的) networking system that the caller wants to
     * begin using the named feature. The interpretation of {@code feature}
     * is completely up to each networking implementation.
     * <p>This method requires the caller to hold the permission
     * {@link android.Manifest.permission#CHANGE_NETWORK_STATE}.
     * @param networkType specifies which network the request pertains to
     * @param feature the name of the feature to be used
     * @return an integer value representing the outcome of the request.
     * The interpretation of this value is specific to each networking
     * implementation+feature combination, except that the value {@code -1}
     * always indicates failure.
     */
    public int startUsingNetworkFeature(int networkType, String feature) {
        try {
            return mService.startUsingNetworkFeature(networkType, feature,new Binder());
        } catch (RemoteException e) {
            return -1;
        }
    }



    /**
     * Ensure that a network route exists to deliver traffic to the specified
     * host via the specified network interface. An attempt to add a route that
     * already exists is ignored, but treated as successful.
     * @param networkType the type of the network over which traffic to the specified
     * host is to be routed
     * @param hostAddress the IP address of the host to which the route is desired
     * @return {@code true} on success, {@code false} on failure
     * @hide
     */
    public boolean requestRouteToHostAddress(int networkType, InetAddress hostAddress) {
        byte[] address = hostAddress.getAddress();
        try {
            return mService.requestRouteToHostAddress(networkType, address, mPackageName);
        } catch (RemoteException e) {
            return false;
        }
    }


}






public class ConnectivityService extends IConnectivityManager.Stub {
    

    // javadoc from interface
    public int startUsingNetworkFeature(int networkType, String feature,IBinder binder) {
        long startTime = 0;
        if (DBG) {
            startTime = SystemClock.elapsedRealtime();
        }
        if (VDBG) {
            log("startUsingNetworkFeature for net " + networkType + ": " + feature + ", uid=" + Binder.getCallingUid());
        }
        enforceChangePermission();
        try {
            if (!ConnectivityManager.isNetworkTypeValid(networkType) || mNetConfigs[networkType] == null) {
                return PhoneConstants.APN_REQUEST_FAILED;
            }

            FeatureUser f = new FeatureUser(networkType, feature, binder);

            // TODO - move this into individual networktrackers
            int usedNetworkType = convertFeatureToNetworkType(networkType, feature);

            if (mLockdownEnabled) {
                // Since carrier APNs usually aren't available from VPN
                // endpoint, mark them as unavailable.
                return PhoneConstants.APN_TYPE_NOT_AVAILABLE;
            }

            if (mProtectedNetworks.contains(usedNetworkType)) {
                enforceConnectivityInternalPermission();
            }

            // if UID is restricted, don't allow them to bring up metered APNs
            final boolean networkMetered = isNetworkMeteredUnchecked(usedNetworkType);
            final int uidRules;
            synchronized (mRulesLock) {
                uidRules = mUidRules.get(Binder.getCallingUid(), RULE_ALLOW_ALL);
            }
            if (networkMetered && (uidRules & RULE_REJECT_METERED) != 0) {
                return PhoneConstants.APN_REQUEST_FAILED;
            }

            NetworkStateTracker network = mNetTrackers[usedNetworkType];
            if (network != null) {
                Integer currentPid = new Integer(getCallingPid());
                if (usedNetworkType != networkType) {
                    NetworkInfo ni = network.getNetworkInfo();

                    if (ni.isAvailable() == false) {
                        if (!TextUtils.equals(feature,Phone.FEATURE_ENABLE_DUN_ALWAYS)) {
                            if (DBG) log("special network not available ni=" + ni.getTypeName());
                            return PhoneConstants.APN_TYPE_NOT_AVAILABLE;
                        } else {
                            // else make the attempt anyway - probably giving REQUEST_STARTED below
                            if (DBG) {
                                log("special network not available, but try anyway ni=" +
                                        ni.getTypeName());
                            }
                        }
                    }

                    int restoreTimer = getRestoreDefaultNetworkDelay(usedNetworkType);

                    synchronized(this) {
                        boolean addToList = true;
                        if (restoreTimer < 0) {
                            // In case there is no timer is specified for the feature,
                            // make sure we don't add duplicate entry with the same request.
                            for (FeatureUser u : mFeatureUsers) {
                                if (u.isSameUser(f)) {
                                    // Duplicate user is found. Do not add.
                                    addToList = false;
                                    break;
                                }
                            }
                        }

                        if (addToList) mFeatureUsers.add(f);
                        if (!mNetRequestersPids[usedNetworkType].contains(currentPid)) {
                            // this gets used for per-pid dns when connected
                            mNetRequestersPids[usedNetworkType].add(currentPid);
                        }
                    }

                    if (restoreTimer >= 0) {
                        mHandler.sendMessageDelayed(mHandler.obtainMessage(EVENT_RESTORE_DEFAULT_NETWORK, f), restoreTimer);
                    }

                    if ((ni.isConnectedOrConnecting() == true) && !network.isTeardownRequested()) {
                        if (ni.isConnected() == true) {
                            final long token = Binder.clearCallingIdentity();
                            try {
                                // add the pid-specific dns
                                handleDnsConfigurationChange(usedNetworkType);
                                if (VDBG) log("special network already active");
                            } finally {
                                Binder.restoreCallingIdentity(token);
                            }
                            return PhoneConstants.APN_ALREADY_ACTIVE;
                        }
                        if (VDBG) log("special network already connecting");
                        return PhoneConstants.APN_REQUEST_STARTED;
                    }

                    // check if the radio in play can make another contact
                    // assume if cannot for now

                    if (DBG) {
                        log("startUsingNetworkFeature reconnecting to " + networkType + ": " + feature);
                    }
                    if (network.reconnect()) {
                        if (DBG) log("startUsingNetworkFeature X: return APN_REQUEST_STARTED");
                        return PhoneConstants.APN_REQUEST_STARTED;
                    } else {
                        if (DBG) log("startUsingNetworkFeature X: return APN_REQUEST_FAILED");
                        return PhoneConstants.APN_REQUEST_FAILED;
                    }
                } else {
                    // need to remember this unsupported request so we respond appropriately on stop
                    synchronized(this) {
                        mFeatureUsers.add(f);
                        if (!mNetRequestersPids[usedNetworkType].contains(currentPid)) {
                            // this gets used for per-pid dns when connected
                            mNetRequestersPids[usedNetworkType].add(currentPid);
                        }
                    }
                    if (DBG) log("startUsingNetworkFeature X: return -1 unsupported feature.");
                    return -1;
                }
            }
            if (DBG) log("startUsingNetworkFeature X: return APN_TYPE_NOT_AVAILABLE");
            return PhoneConstants.APN_TYPE_NOT_AVAILABLE;
         } finally {
            if (DBG) {
                final long execTime = SystemClock.elapsedRealtime() - startTime;
                if (execTime > 250) {
                    loge("startUsingNetworkFeature took too long: " + execTime + "ms");
                } else {
                    if (VDBG) log("startUsingNetworkFeature took " + execTime + "ms");
                }
            }
         }
    }


   /**
     * Ensure that a network route exists to deliver traffic to the specified
     * host via the specified network interface.
     * @param networkType the type of the network over which traffic to the
     * specified host is to be routed
     * @param hostAddress the IP address of the host to which the route is
     * desired
     * @return {@code true} on success, {@code false} on failure
     */
    public boolean requestRouteToHostAddress(int networkType, byte[] hostAddress,String packageName) {
        enforceChangePermission();
        if (mProtectedNetworks.contains(networkType)) {
            enforceConnectivityInternalPermission();
        }
        boolean exempt;
        InetAddress addr;
        try {
            addr = InetAddress.getByAddress(hostAddress);
        } catch (UnknownHostException e) {
            if (DBG) log("requestRouteToHostAddress got " + e.toString());
            return false;
        }
        // System apps may request routes bypassing the VPN to keep other networks working.
        if (Binder.getCallingUid() == Process.SYSTEM_UID) {
            exempt = true;
        } else {
            mAppOpsManager.checkPackage(Binder.getCallingUid(), packageName);
            try {
                ApplicationInfo info = mContext.getPackageManager().getApplicationInfo(packageName,
                        0);
                exempt = (info.flags & ApplicationInfo.FLAG_SYSTEM) != 0;
            } catch (NameNotFoundException e) {
                throw new IllegalArgumentException("Failed to find calling package details", e);
            }
        }

        // Non-exempt routeToHost's can only be added if the host is not covered by the VPN.
        // This can be either because the VPN's routes do not cover the destination or a
        // system application added an exemption that covers this destination.
        if (!exempt && isAddressUnderVpn(addr)) {
            return false;
        }

        if (!ConnectivityManager.isNetworkTypeValid(networkType)) {
            if (DBG) log("requestRouteToHostAddress on invalid network: " + networkType);
            return false;
        }
        NetworkStateTracker tracker = mNetTrackers[networkType];
        DetailedState netState = tracker.getNetworkInfo().getDetailedState();

        if (tracker == null || (netState != DetailedState.CONNECTED &&
                netState != DetailedState.CAPTIVE_PORTAL_CHECK) ||
                tracker.isTeardownRequested()) {
            if (VDBG) {
                log("requestRouteToHostAddress on down network "
                        + "(" + networkType + ") - dropped"
                        + " tracker=" + tracker
                        + " netState=" + netState
                        + " isTeardownRequested="
                            + ((tracker != null) ? tracker.isTeardownRequested() : "tracker:null"));
            }
            return false;
        }
        final long token = Binder.clearCallingIdentity();
        try {
            LinkProperties lp = tracker.getLinkProperties();
            boolean ok = addRouteToAddress(lp, addr, exempt);
            if (DBG) log("requestRouteToHostAddress ok=" + ok);
            return ok;
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }


}
