public class ConnectivityManager {

    /**
     * Request a network to satisfy a set of {@link android.net.NetworkCapabilities}, limited
     * by a timeout.
     *
     * This function behaves identically to the non-timed-out version
     * {@link #requestNetwork(NetworkRequest, NetworkCallback)}, but if a suitable network
     * is not found within the given time (in milliseconds) the
     * {@link NetworkCallback#onUnavailable()} callback is called. The request can still be
     * released normally by calling {@link #unregisterNetworkCallback(NetworkCallback)} but does
     * not have to be released if timed-out (it is automatically released). Unregistering a
     * request that timed out is not an error.
     *
     * <p>Do not use this method to poll for the existence of specific networks (e.g. with a small
     * timeout) - {@link #registerNetworkCallback(NetworkRequest, NetworkCallback)} is provided
     * for that purpose. Calling this method will attempt to bring up the requested network.
     *
     * <p>This method requires the caller to hold either the
     * {@link android.Manifest.permission#CHANGE_NETWORK_STATE} permission
     * or the ability to modify system settings as determined by
     * {@link android.provider.Settings.System#canWrite}.</p>
     *
     * @param request {@link NetworkRequest} describing this request.
     * @param networkCallback The {@link NetworkCallback} to be utilized for this request. Note
     *                        the callback must not be shared - it uniquely specifies this request.
     * @param timeoutMs The time in milliseconds to attempt looking for a suitable network
     *                  before {@link NetworkCallback#onUnavailable()} is called. The timeout must
     *                  be a positive value (i.e. >0).
     */
    public void requestNetwork(NetworkRequest request, NetworkCallback networkCallback,int timeoutMs) {
        checkTimeout(timeoutMs); //检测timeoutMs是否大于等于0
        int legacyType = inferLegacyTypeForNetworkCapabilities(request.networkCapabilities); // legacyType == TYPE_NONE
        requestNetwork(request, networkCallback, timeoutMs, legacyType, getDefaultHandler());
    }


    /**
     * Guess what the network request was trying to say so that the resulting
     * network is accessible via the legacy (deprecated) API such as
     * requestRouteToHost.
     *
     * This means we should try to be fairly precise about transport and
     * capability but ignore things such as networkSpecifier.
     * If the request has more than one transport or capability it doesn't
     * match the old legacy requests (they selected only single transport/capability)
     * so this function cannot map the request to a single legacy type and
     * the resulting network will not be available to the legacy APIs.
     *
     * This code is only called from the requestNetwork API (L and above).
     *
     * Setting a legacy type causes CONNECTIVITY_ACTION broadcasts, which are expensive
     * because they wake up lots of apps - see http://b/23350688 . So we currently only
     * do this for SUPL requests, which are the only ones that we know need it. If
     * omitting these broadcasts causes unacceptable app breakage, then for backwards
     * compatibility we can send them:
     *
     * if (targetSdkVersion < Build.VERSION_CODES.M) &&        // legacy API unsupported >= M
     *     targetSdkVersion >= Build.VERSION_CODES.LOLLIPOP))  // requestNetwork not present < L
     *
     * TODO - This should be removed when the legacy APIs are removed.
     */
    private int inferLegacyTypeForNetworkCapabilities(NetworkCapabilities netCap) {
        if (netCap == null) {
            return TYPE_NONE;
        }

	// 判断 netCap 中 对应 NetworkCapabilities.TRANSPORT_CELLULAR 的位 为1 返回true，否则 为false 
        if (!netCap.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)) {
            return TYPE_NONE;
        }

        // Do this only for SUPL, until GnssLocationProvider is fixed. http://b/25876485 .
        if (!netCap.hasCapability(NetworkCapabilities.NET_CAPABILITY_SUPL)) {
            // NOTE: if this causes app breakage, we should not just comment out this early return;
            // instead, we should make this early return conditional on the requesting app's target
            // SDK version, as described in the comment above.
            return TYPE_NONE;
        }

        String type = null;
        int result = TYPE_NONE;

        if (netCap.hasCapability(NetworkCapabilities.NET_CAPABILITY_CBS)) {
            type = "enableCBS";
            result = TYPE_MOBILE_CBS;
        } else if (netCap.hasCapability(NetworkCapabilities.NET_CAPABILITY_IMS)) {
            type = "enableIMS";
            result = TYPE_MOBILE_IMS;
        } else if (netCap.hasCapability(NetworkCapabilities.NET_CAPABILITY_FOTA)) {
            type = "enableFOTA";
            result = TYPE_MOBILE_FOTA;
        } else if (netCap.hasCapability(NetworkCapabilities.NET_CAPABILITY_DUN)) {
            type = "enableDUN";
            result = TYPE_MOBILE_DUN;
        } else if (netCap.hasCapability(NetworkCapabilities.NET_CAPABILITY_SUPL)) {
            type = "enableSUPL";
            result = TYPE_MOBILE_SUPL;
        // back out this hack for mms as they no longer need this and it's causing
        // device slowdowns - b/23350688 (note, supl still needs this)
        //} else if (netCap.hasCapability(NetworkCapabilities.NET_CAPABILITY_MMS)) {
        //    type = "enableMMS";
        //    result = TYPE_MOBILE_MMS;
        } else if (netCap.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)) {
            type = "enableHIPRI";
            result = TYPE_MOBILE_HIPRI;
        }
        if (type != null) {
            NetworkCapabilities testCap = networkCapabilitiesForFeature(TYPE_MOBILE, type);
            if (testCap.equalsNetCapabilities(netCap) && testCap.equalsTransportTypes(netCap)) {
                return result;
            }
        }
        return TYPE_NONE;
    }
   


}






public class ConnectivityService extends IConnectivityManager.Stub {
    
    private synchronized int nextNetworkRequestId() {
        return mNextNetworkRequestId++;
    }

    @Override
    public NetworkRequest requestNetwork(NetworkCapabilities networkCapabilities,Messenger messenger, int timeoutMs, IBinder binder, int legacyType) {
        final NetworkRequest.Type type = (networkCapabilities == null) ? NetworkRequest.Type.TRACK_DEFAULT : NetworkRequest.Type.REQUEST;
        // If the requested networkCapabilities is null, take them instead from
        // the default network request. This allows callers to keep track of
        // the system default network.
        if (type == NetworkRequest.Type.TRACK_DEFAULT) {
            networkCapabilities = new NetworkCapabilities(mDefaultRequest.networkCapabilities);
            enforceAccessPermission();
        } else {
            networkCapabilities = new NetworkCapabilities(networkCapabilities);
            enforceNetworkRequestPermissions(networkCapabilities);
            // TODO: this is incorrect. We mark the request as metered or not depending on the state
            // of the app when the request is filed, but we never change the request if the app
            // changes network state. http://b/29964605
            enforceMeteredApnPolicy(networkCapabilities);
        }
        ensureRequestableCapabilities(networkCapabilities);

        if (timeoutMs < 0) {
            throw new IllegalArgumentException("Bad timeout specified");
        }
        ensureValidNetworkSpecifier(networkCapabilities);

        NetworkRequest networkRequest = new NetworkRequest(networkCapabilities, legacyType,nextNetworkRequestId(), type);
        NetworkRequestInfo nri = new NetworkRequestInfo(messenger, networkRequest, binder);
        if (DBG) log("requestNetwork for " + nri);

        mHandler.sendMessage(mHandler.obtainMessage(EVENT_REGISTER_NETWORK_REQUEST, nri));
        if (timeoutMs > 0) {
            mHandler.sendMessageDelayed(mHandler.obtainMessage(EVENT_TIMEOUT_NETWORK_REQUEST,nri), timeoutMs);
        }
        return networkRequest;
    }


    // EVENT_REGISTER_NETWORK_REQUEST 处理
    private void handleRegisterNetworkRequest(NetworkRequestInfo nri) {
	//   private final HashMap<NetworkRequest, NetworkRequestInfo> mNetworkRequests = new HashMap<NetworkRequest, NetworkRequestInfo>();
        mNetworkRequests.put(nri.request, nri);

	// private final LocalLog mNetworkRequestInfoLogs = new LocalLog(MAX_NETWORK_REQUEST_LOGS);
        mNetworkRequestInfoLogs.log("REGISTER " + nri);
        if (nri.request.isListen()) { //  判断请求是否为 LISTEN
            for (NetworkAgentInfo network : mNetworkAgentInfos.values()) {
                if (nri.request.networkCapabilities.hasSignalStrength() && network.satisfiesImmutableCapabilitiesOf(nri.request)) {
                    updateSignalStrengthThresholds(network, "REGISTER", nri.request);
                }
            }
        }
        rematchAllNetworksAndRequests(null, 0);
        if (nri.request.isRequest() && mNetworkForRequestId.get(nri.request.requestId) == null) {
            sendUpdatedScoreToFactories(nri.request, 0);
        }
    }


    /**
     * Attempt to rematch all Networks with NetworkRequests.  This may result in Networks
     * being disconnected.
     * @param changed If only one Network's score or capabilities have been modified since the last
     *         time this function was called, pass this Network in this argument, otherwise pass
     *         null.
     * @param oldScore If only one Network has been changed but its NetworkCapabilities have not
     *         changed, pass in the Network's score (from getCurrentScore()) prior to the change via
     *         this argument, otherwise pass {@code changed.getCurrentScore()} or 0 if
     *         {@code changed} is {@code null}. This is because NetworkCapabilities influence a
     *         network's score.
     */
    private void rematchAllNetworksAndRequests(NetworkAgentInfo changed, int oldScore) {
        // TODO: This may get slow.  The "changed" parameter is provided for future optimization
        // to avoid the slowness.  It is not simply enough to process just "changed", for
        // example in the case where "changed"'s score decreases and another network should begin
        // satifying a NetworkRequest that "changed" currently satisfies.

        // Optimization: Only reprocess "changed" if its score improved.  This is safe because it
        // can only add more NetworkRequests satisfied by "changed", and this is exactly what
        // rematchNetworkAndRequests() handles.
        final long now = SystemClock.elapsedRealtime();
        if (changed != null && oldScore < changed.getCurrentScore()) {
            rematchNetworkAndRequests(changed, ReapUnvalidatedNetworks.REAP, now);
        } else {
            final NetworkAgentInfo[] nais = mNetworkAgentInfos.values().toArray(new NetworkAgentInfo[mNetworkAgentInfos.size()]);
            // Rematch higher scoring networks first to prevent requests first matching a lower
            // scoring network and then a higher scoring network, which could produce multiple
            // callbacks and inadvertently unlinger networks.
            Arrays.sort(nais);
            for (NetworkAgentInfo nai : nais) {
 		// Only reap the last time through the loop.  Reaping before all rematching
                // is complete could incorrectly teardown a network that hasn't yet been
                // rematched.
                rematchNetworkAndRequests(nai,(nai != nais[nais.length-1]) ? ReapUnvalidatedNetworks.DONT_REAP : ReapUnvalidatedNetworks.REAP, now);
            }
        }
    }


    private void sendUpdatedScoreToFactories(NetworkRequest networkRequest, int score) {
        if (VDBG) log("sending new Min Network Score(" + score + "): " + networkRequest.toString());
        for (NetworkFactoryInfo nfi : mNetworkFactoryInfos.values()) {
            nfi.asyncChannel.sendMessage(android.net.NetworkFactory.CMD_REQUEST_NETWORK, score, 0,networkRequest);
        }
    }

   
   // mNetworkFactoryInfos 得来
    public void registerNetworkFactory(Messenger messenger, String name) {
        enforceConnectivityInternalPermission();
        NetworkFactoryInfo nfi = new NetworkFactoryInfo(name, messenger, new AsyncChannel());
        mHandler.sendMessage(mHandler.obtainMessage(EVENT_REGISTER_NETWORK_FACTORY, nfi));
    }

    private void handleRegisterNetworkFactory(NetworkFactoryInfo nfi) {
        if (DBG) log("Got NetworkFactory Messenger for " + nfi.name);
        mNetworkFactoryInfos.put(nfi.messenger, nfi);
        nfi.asyncChannel.connect(mContext, mTrackerHandler, nfi.messenger);
    }



    // EVENT_TIMEOUT_NETWORK_REQUEST 处理
    private void handleTimedOutNetworkRequest(final NetworkRequestInfo nri) {
        if (mNetworkRequests.get(nri.request) == null) {
            return;
        }
        if (mNetworkForRequestId.get(nri.request.requestId) != null) {
            return;
        }
        if (VDBG || (DBG && nri.request.isRequest())) {
            log("releasing " + nri.request + " (timeout)");
        }
        handleRemoveNetworkRequest(nri);
        callCallbackForRequest(nri, null, ConnectivityManager.CALLBACK_UNAVAIL, 0);
    }




}
