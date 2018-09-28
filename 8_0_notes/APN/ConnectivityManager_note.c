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
        requestNetwork(request, networkCallback, timeoutMs, legacyType == TYPE_NONE, getDefaultHandler());
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

	    // 判断 netCap 中 mTransportTypes 对应 NetworkCapabilities.TRANSPORT_CELLULAR 的位 为1 返回true，否则 为false 
        if (!netCap.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)) { // 所有这里整体为false，继续往下执行
            return TYPE_NONE;
        }

        // Do this only for SUPL, until GnssLocationProvider is fixed. http://b/25876485 .
	    // 判断 netCap 中 mNetworkCapabilities 对应 NetworkCapabilities.NET_CAPABILITY_SUPL 的位 为1 返回true，否则 为false 
        if (!netCap.hasCapability(NetworkCapabilities.NET_CAPABILITY_SUPL)) {   // 所以这里整体为true，返回 TYPE_NONE
            // NOTE: if this causes app breakage, we should not just comment out this early return;
            // instead, we should make this early return conditional on the requesting app's target 
            // SDK version, as described in the comment above.
            return TYPE_NONE;
        }

        。。。。。。
    }



        /**
     * Helper function to request a network with a particular legacy type.
     *
     * This is temporarily public @hide so it can be called by system code that uses the
     * NetworkRequest API to request networks but relies on CONNECTIVITY_ACTION broadcasts for
     * instead network notifications.
     *
     * TODO: update said system code to rely on NetworkCallbacks and make this method private.
     *
     * @hide
     */
    public void requestNetwork(NetworkRequest request, NetworkCallback networkCallback,int timeoutMs, int legacyType, Handler handler) {
        CallbackHandler cbHandler = new CallbackHandler(handler); //创建一个新的Handler，用于处理ConnectivityService发送过来的消息
        NetworkCapabilities nc = request.networkCapabilities; //这里注意 NetworkRequest request 没有传送到 ConnectivityService 
        sendRequestForNetwork(nc, networkCallback, timeoutMs, REQUEST, legacyType == TYPE_NONE, cbHandler);
    }


    private NetworkRequest sendRequestForNetwork(NetworkCapabilities need, NetworkCallback callback,int timeoutMs, int action == REQUEST, int legacyType == TYPE_NONE, CallbackHandler handler) {
        checkCallbackNotNull(callback);
        Preconditions.checkArgument(action == REQUEST || need != null, "null NetworkCapabilities");
        final NetworkRequest request;
        try {
            synchronized(sCallbacks) {
		// callback.networkRequest 为 null 
                if (callback.networkRequest != null && callback.networkRequest != ALREADY_UNREGISTERED) {
                    // TODO: throw exception instead and enforce 1:1 mapping of callbacks
                    // and requests (http://b/20701525).
                    Log.e(TAG, "NetworkCallback was already registered");
                }



                Messenger messenger = new Messenger(handler);
                Binder binder = new Binder();
                if (action == LISTEN) {
                    。。。。。。
                } else {
                    //执行这里，进入到ConnectivityService中
                    request = mService.requestNetwork(need, messenger, timeoutMs, binder, legacyType);
                }
                if (request != null) {
                    sCallbacks中.put(request, callback);  //注意 这里加入到 sCallbacks 中
                }
                callback.networkRequest = request;  //注意这里对callback.networkRequest赋值
            }
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        } catch (ServiceSpecificException e) {
            throw convertServiceException(e);
        }
        return request;
    }
   


}
