public class MmsNetworkManager {


     public MmsNetworkManager(){
	......
     	mNetworkRequest = new NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR)
                .addCapability(NetworkCapabilities.NET_CAPABILITY_MMS)
                .setNetworkSpecifier(Integer.toString(mSubId))
                .build();
	......
     }


    /**
     * Acquire the MMS network
     *
     * @param requestId request ID for logging   (requestId用于日志打印,为调用该函数的类) packages/services/Mms/src/com/android/mms/service/MmsRequest.java:153 
     * @throws com.android.mms.service.exception.MmsNetworkException if we fail to acquire it
     * 
     */
    public void acquireNetwork(final String requestId) throws MmsNetworkException {
        synchronized (this) {
            // Since we are acquiring the network, remove the network release task if exists.
            mReleaseHandler.removeCallbacks(mNetworkReleaseTask);
            mMmsRequestCount += 1;
            if (mNetwork != null) {
                // Already available
                LogUtil.d(requestId, "MmsNetworkManager: already available");
                return;
            }
            // Not available, so start a new request if not done yet
            if (mNetworkCallback == null) { // mNetworkCallback 
                LogUtil.d(requestId, "MmsNetworkManager: start new network request");
                startNewNetworkRequestLocked();
            }
            final long shouldEnd = SystemClock.elapsedRealtime() + NETWORK_ACQUIRE_TIMEOUT_MILLIS;
            long waitTime = NETWORK_ACQUIRE_TIMEOUT_MILLIS;
            while (waitTime > 0) {
                try {
                    this.wait(waitTime);
                } catch (InterruptedException e) {
                    LogUtil.w(requestId, "MmsNetworkManager: acquire network wait interrupted");
                }
                if (mNetwork != null) {
                    // Success
                    return;
                }
                // Calculate remaining waiting time to make sure we wait the full timeout period  
                waitTime = shouldEnd - SystemClock.elapsedRealtime();
            }
            // Timed out, so release the request and fail
            LogUtil.e(requestId, "MmsNetworkManager: timed out");
            releaseRequestLocked(mNetworkCallback);
            throw new MmsNetworkException("Acquiring network timed out");
        }
    }
	

    // Timeout used to call ConnectivityManager.requestNetwork
    // Given that the telephony layer will retry on failures, this timeout should be high enough.
    private static final int NETWORK_REQUEST_TIMEOUT_MILLIS = 30 * 60 * 1000;


    /**
     * Start a new {@link android.net.NetworkRequest} for MMS
     */
    private void startNewNetworkRequestLocked() {
        final ConnectivityManager connectivityManager = getConnectivityManager();
        mNetworkCallback = new NetworkRequestCallback();
        connectivityManager.requestNetwork(mNetworkRequest, mNetworkCallback, NETWORK_REQUEST_TIMEOUT_MILLIS);
    }



    /**
     * Network callback for our network request
     */
    private class NetworkRequestCallback extends ConnectivityManager.NetworkCallback { //ConnectivityManager.NetworkCallback中都是空实现
        @Override
        public void onAvailable(Network network) {
            super.onAvailable(network);  // 空实现，没有实际作用
            LogUtil.i("NetworkCallbackListener.onAvailable: network=" + network);
            synchronized (MmsNetworkManager.this) {
                mNetwork = network;
                MmsNetworkManager.this.notifyAll();
            }
        }

        @Override
        public void onLost(Network network) {
            super.onLost(network);// 空实现，没有实际作用
            LogUtil.w("NetworkCallbackListener.onLost: network=" + network);
            synchronized (MmsNetworkManager.this) {
                releaseRequestLocked(this);
                MmsNetworkManager.this.notifyAll();
            }
        }

        @Override
        public void onUnavailable() {
            super.onUnavailable();// 空实现，没有实际作用
            LogUtil.w("NetworkCallbackListener.onUnavailable");
            synchronized (MmsNetworkManager.this) {
                releaseRequestLocked(this);
                MmsNetworkManager.this.notifyAll();
            }
        }
    }
}





/**
* Base class for {@code NetworkRequest} callbacks. Used for notifications about network
* changes. Should be extended by applications wanting notifications.
*
* A {@code NetworkCallback} is registered by calling
* {@link #requestNetwork(NetworkRequest, NetworkCallback)},
* {@link #registerNetworkCallback(NetworkRequest, NetworkCallback)},
* or {@link #registerDefaultNetworkCallback(NetworkCallback). A {@code NetworkCallback} is
* unregistered by calling {@link #unregisterNetworkCallback(NetworkCallback)}.
* A {@code NetworkCallback} should be registered at most once at any time.
* A {@code NetworkCallback} that has been unregistered can be registered again.
*/
public static class ConnectivityManager.NetworkCallback {
        /**
         * Called when the framework connects to a new network to evaluate whether it satisfies this
         * request. If evaluation succeeds, this callback may be followed by an {@link #onAvailable}
         * callback. There is no guarantee that this new network will satisfy any requests, or that
         * the network will stay connected for longer than the time necessary to evaluate it.
         * <p>
         * Most applications <b>should not</b> act on this callback, and should instead use
         * {@link #onAvailable}. This callback is intended for use by applications that can assist
         * the framework in properly evaluating the network &mdash; for example, an application that
         * can automatically log in to a captive portal without user intervention.
         *
         * @param network The {@link Network} of the network that is being evaluated.
         *
         * @hide
         */
        public void onPreCheck(Network network) {}

        /**
         * Called when the framework connects and has declared a new network ready for use.
         * This callback may be called more than once if the {@link Network} that is
         * satisfying the request changes.
         *
         * @param network The {@link Network} of the satisfying network.
         */
        public void onAvailable(Network network) {}

        /**
         * Called when the network is about to be disconnected.  Often paired with an
         * {@link NetworkCallback#onAvailable} call with the new replacement network
         * for graceful handover.  This may not be called if we have a hard loss
         * (loss without warning).  This may be followed by either a
         * {@link NetworkCallback#onLost} call or a
         * {@link NetworkCallback#onAvailable} call for this network depending
         * on whether we lose or regain it.
         *
         * @param network The {@link Network} that is about to be disconnected.
         * @param maxMsToLive The time in ms the framework will attempt to keep the
         *                     network connected.  Note that the network may suffer a
         *                     hard loss at any time.
         */
        public void onLosing(Network network, int maxMsToLive) {}

        /**
         * Called when the framework has a hard loss of the network or when the
         * graceful failure ends.
         *
         * @param network The {@link Network} lost.
         */
        public void onLost(Network network) {}

        /**
         * Called if no network is found in the timeout time specified in
         * {@link #requestNetwork(NetworkRequest, NetworkCallback, int)} call. This callback is not
         * called for the version of {@link #requestNetwork(NetworkRequest, NetworkCallback)}
         * without timeout. When this callback is invoked the associated
         * {@link NetworkRequest} will have already been removed and released, as if
         * {@link #unregisterNetworkCallback(NetworkCallback)} had been called.
         */
        public void onUnavailable() {}

        /**
         * Called when the network the framework connected to for this request
         * changes capabilities but still satisfies the stated need.
         *
         * @param network The {@link Network} whose capabilities have changed.
         * @param networkCapabilities The new {@link android.net.NetworkCapabilities} for this network.
         */
        public void onCapabilitiesChanged(Network network,NetworkCapabilities networkCapabilities) {}

        /**
         * Called when the network the framework connected to for this request
         * changes {@link LinkProperties}.
         *
         * @param network The {@link Network} whose link properties have changed.
         * @param linkProperties The new {@link LinkProperties} for this network.
         */
        public void onLinkPropertiesChanged(Network network, LinkProperties linkProperties) {}

        /**
         * Called when the network the framework connected to for this request
         * goes into {@link NetworkInfo.DetailedState.SUSPENDED}.
         * This generally means that while the TCP connections are still live,
         * temporarily network data fails to transfer.  Specifically this is used
         * on cellular networks to mask temporary outages when driving through
         * a tunnel, etc.
         * @hide
         */
        public void onNetworkSuspended(Network network) {}

        /**
         * Called when the network the framework connected to for this request
         * returns from a {@link NetworkInfo.DetailedState.SUSPENDED} state.
         * This should always be preceeded by a matching {@code onNetworkSuspended}
         * call.
         * @hide
         */
        public void onNetworkResumed(Network network) {}

        private NetworkRequest networkRequest;
}








