public class ConnectivityService extends IConnectivityManager.Stub {
    
    private synchronized int nextNetworkRequestId() {
        return mNextNetworkRequestId++;
    }

    @Override
    public NetworkRequest requestNetwork(NetworkCapabilities networkCapabilities,Messenger messenger, int timeoutMs, IBinder binder, int legacyType) {

        // 这里 networkCapabilities 不为 null ，type ==  NetworkRequest.Type.REQUEST
        final NetworkRequest.Type type = (networkCapabilities == null) ? NetworkRequest.Type.TRACK_DEFAULT : NetworkRequest.Type.REQUEST; 
        // If the requested networkCapabilities is null, take them instead from
        // the default network request. This allows callers to keep track of
        // the system default network.
        if (type == NetworkRequest.Type.TRACK_DEFAULT) {
           ......
        } else { // 执行这里
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

      
        //nextNetworkRequestId 全局静态整型自增 1 并且赋值给成员变量 requestId
	    //重新构建 NetworkRequest
        NetworkRequest networkRequest = new NetworkRequest(networkCapabilities, legacyType,nextNetworkRequestId(), type);
        NetworkRequestInfo nri = new NetworkRequestInfo(messenger, networkRequest, binder); //构建一个 NetworkRequestInfo 

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
        mNetworkRequestInfoLogs.log("REGISTER " + nri);  //日志保存，可以忽略
        if (nri.request.isListen()) { //  判断请求是否为 LISTEN ，这里值为NetworkRequest.Type.REQUEST，所以不执行
            。。。。。。
        }

        rematchAllNetworksAndRequests(null, 0);
        if (nri.request.isRequest() && mNetworkForRequestId.get(nri.request.requestId) == null) { //nri.request.requestId == nextNetworkRequestId()
            sendUpdatedScoreToFactories(nri.request, 0);//这里执行
        }
    }


    /**
     * Attempt to rematch all Networks with NetworkRequests.  This may result in Networks being disconnected.
     * @param changed If only one Network's score or capabilities have been modified since the last
     *         time this function was called, pass this Network in this argument, otherwise pass
     *         null.
     * @param oldScore If only one Network has been changed but its NetworkCapabilities have not
     *         changed, pass in the Network's score (from getCurrentScore()) prior to the change via
     *         this argument, otherwise pass {@code changed.getCurrentScore()} or 0 if
     *         {@code changed} is {@code null}. This is because NetworkCapabilities influence a
     *         network's score.
     */
    private void rematchAllNetworksAndRequests(NetworkAgentInfo changed == null , int oldScore == 0) {
        // TODO: This may get slow.  The "changed" parameter is provided for future optimization
        // to avoid the slowness.  It is not simply enough to process just "changed", for
        // example in the case where "changed"'s score decreases and another network should begin
        // satifying a NetworkRequest that "changed" currently satisfies.

        // Optimization: Only reprocess "changed" if its score improved.  This is safe because it
        // can only add more NetworkRequests satisfied by "changed", and this is exactly what
        // rematchNetworkAndRequests() handles.
        final long now = SystemClock.elapsedRealtime();
        if (changed != null && oldScore < changed.getCurrentScore()) { //这里 changed 为null，所以不执行
            ......
        } else {
            final NetworkAgentInfo[] nais = mNetworkAgentInfos.values().toArray(new NetworkAgentInfo[mNetworkAgentInfos.size()]);
            // Rematch higher scoring networks first to prevent requests first matching a lower
            // scoring network and then a higher scoring network, which could produce multiple
            // callbacks and inadvertently unlinger networks.
            Arrays.sort(nais);
            for (NetworkAgentInfo nai : nais) {
 		         // Only reap（收获） the last time through the loop.  Reaping before all rematching
                // is complete could incorrectly teardown a network that hasn't yet been
                // rematched.
                rematchNetworkAndRequests(nai,(nai != nais[nais.length-1]) ? ReapUnvalidatedNetworks.DONT_REAP : ReapUnvalidatedNetworks.REAP, now);
            }
        }
    }


   


    // Handles a network appearing or improving its score.
    //
    // - Evaluates all current NetworkRequests that can be
    //   satisfied by newNetwork, and reassigns to newNetwork
    //   any such requests for which newNetwork is the best.
    //
    // - Lingers any validated Networks that as a result are no longer
    //   needed. A network is needed if it is the best network for
    //   one or more NetworkRequests, or if it is a VPN.
    //
    // - Tears down newNetwork if it just became validated
    //   but turns out to be unneeded.
    //
    // - If reapUnvalidatedNetworks==REAP, tears down unvalidated
    //   networks that have no chance (i.e. even if validated)
    //   of becoming the highest scoring network.
    //
    // NOTE: This function only adds NetworkRequests that "newNetwork" could satisfy,
    // it does not remove NetworkRequests that other Networks could better satisfy.
    // If you need to handle decreases in score, use {@link rematchAllNetworksAndRequests}.
    // This function should be used when possible instead of {@code rematchAllNetworksAndRequests}
    // as it performs better by a factor of the number of Networks.
    //
    // @param newNetwork is the network to be matched against NetworkRequests.
    // @param reapUnvalidatedNetworks indicates if an additional pass over all networks should be
    //               performed to tear down unvalidated networks that have no chance (i.e. even if
    //               validated) of becoming the highest scoring network.
    private void rematchNetworkAndRequests(NetworkAgentInfo newNetwork,ReapUnvalidatedNetworks reapUnvalidatedNetworks, long now) {
        if (!newNetwork.everConnected) return;
        boolean keep = newNetwork.isVPN();
        boolean isNewDefault = false;
        NetworkAgentInfo oldDefaultNetwork = null;

        final boolean wasBackgroundNetwork = newNetwork.isBackgroundNetwork();
        final int score = newNetwork.getCurrentScore();

        if (VDBG) log("rematching " + newNetwork.name());

        // Find and migrate to this Network any NetworkRequests for
        // which this network is now the best.
        ArrayList<NetworkAgentInfo> affectedNetworks = new ArrayList<NetworkAgentInfo>();
        ArrayList<NetworkRequestInfo> addedRequests = new ArrayList<NetworkRequestInfo>();
        NetworkCapabilities nc = newNetwork.networkCapabilities;
        if (VDBG) log(" network has: " + nc);
        for (NetworkRequestInfo nri : mNetworkRequests.values()) {
            // Process requests in the first pass and listens in the second pass. This allows us to
            // change a network's capabilities depending on which requests it has. This is only
            // correct if the change in capabilities doesn't affect whether the network satisfies
            // requests or not, and doesn't affect the network's score.
            if (nri.request.isListen()) continue;

            final NetworkAgentInfo currentNetwork = mNetworkForRequestId.get(nri.request.requestId);
            final boolean satisfies = newNetwork.satisfies(nri.request);
            if (newNetwork == currentNetwork && satisfies) {
                if (VDBG) {
                    log("Network " + newNetwork.name() + " was already satisfying" + " request " + nri.request.requestId + ". No change.");
                }
                keep = true;
                continue;
            }

            // check if it satisfies the NetworkCapabilities
            if (VDBG) log("  checking if request is satisfied: " + nri.request);
            if (satisfies) {
                // next check if it's better than any current network we're using for
                // this request
                if (VDBG) {
                    log("currentScore = " + (currentNetwork != null ? currentNetwork.getCurrentScore() : 0) + ", newScore = " + score);
                }
                if (currentNetwork == null || currentNetwork.getCurrentScore() < score) {
                    if (VDBG) log("rematch for " + newNetwork.name());
                    if (currentNetwork != null) {
                        if (VDBG) log("   accepting network in place of " + currentNetwork.name());
                        currentNetwork.removeRequest(nri.request.requestId);
                        currentNetwork.lingerRequest(nri.request, now, mLingerDelayMs);
                        affectedNetworks.add(currentNetwork);
                    } else {
                        if (VDBG) log("   accepting network in place of null");
                    }
                    newNetwork.unlingerRequest(nri.request);
                    mNetworkForRequestId.put(nri.request.requestId, newNetwork);
                    if (!newNetwork.addRequest(nri.request)) {
                        Slog.wtf(TAG, "BUG: " + newNetwork.name() + " already has " + nri.request);
                    }
                    addedRequests.add(nri);
                    keep = true;
                    // Tell NetworkFactories about the new score, so they can stop
                    // trying to connect if they know they cannot match it.
                    // TODO - this could get expensive if we have alot of requests for this
                    // network.  Think about if there is a way to reduce this.  Push
                    // netid->request mapping to each factory?
                    sendUpdatedScoreToFactories(nri.request, score);
                    if (isDefaultRequest(nri)) {
                        isNewDefault = true;
                        oldDefaultNetwork = currentNetwork;
                        if (currentNetwork != null) {
                            mLingerMonitor.noteLingerDefaultNetwork(currentNetwork, newNetwork);
                        }
                    }
                }
            } else if (newNetwork.isSatisfyingRequest(nri.request.requestId)) {
                // If "newNetwork" is listed as satisfying "nri" but no longer satisfies "nri",
                // mark it as no longer satisfying "nri".  Because networks are processed by
                // rematchAllNetworksAndRequests() in descending score order, "currentNetwork" will
                // match "newNetwork" before this loop will encounter a "currentNetwork" with higher
                // score than "newNetwork" and where "currentNetwork" no longer satisfies "nri".
                // This means this code doesn't have to handle the case where "currentNetwork" no
                // longer satisfies "nri" when "currentNetwork" does not equal "newNetwork".
                if (DBG) {
                    log("Network " + newNetwork.name() + " stopped satisfying" +
                            " request " + nri.request.requestId);
                }
                newNetwork.removeRequest(nri.request.requestId);
                if (currentNetwork == newNetwork) {
                    mNetworkForRequestId.remove(nri.request.requestId);
                    sendUpdatedScoreToFactories(nri.request, 0);
                } else {
                    Slog.wtf(TAG, "BUG: Removing request " + nri.request.requestId + " from " +
                            newNetwork.name() +
                            " without updating mNetworkForRequestId or factories!");
                }
                // TODO: Technically, sending CALLBACK_LOST here is
                // incorrect if there is a replacement network currently
                // connected that can satisfy nri, which is a request
                // (not a listen). However, the only capability that can both
                // a) be requested and b) change is NET_CAPABILITY_TRUSTED,
                // so this code is only incorrect for a network that loses
                // the TRUSTED capability, which is a rare case.
                callCallbackForRequest(nri, newNetwork, ConnectivityManager.CALLBACK_LOST, 0);
            }
        }
        if (isNewDefault) {
            // Notify system services that this network is up.
            makeDefault(newNetwork);
            // Log 0 -> X and Y -> X default network transitions, where X is the new default.
            logDefaultNetworkEvent(newNetwork, oldDefaultNetwork);
            // Have a new default network, release the transition wakelock in
            scheduleReleaseNetworkTransitionWakelock();
        }

        if (!newNetwork.networkCapabilities.equalRequestableCapabilities(nc)) {
            Slog.wtf(TAG, String.format("BUG: %s changed requestable capabilities during rematch: %s -> %s",nc, newNetwork.networkCapabilities));
        }
        if (newNetwork.getCurrentScore() != score) {
            Slog.wtf(TAG, String.format("BUG: %s changed score during rematch: %d -> %d",score, newNetwork.getCurrentScore()));
        }

        // Second pass: process all listens.
        if (wasBackgroundNetwork != newNetwork.isBackgroundNetwork()) {
            // If the network went from background to foreground or vice versa, we need to update
            // its foreground state. It is safe to do this after rematching the requests because
            // NET_CAPABILITY_FOREGROUND does not affect requests, as is not a requestable
            // capability and does not affect the network's score (see the Slog.wtf call above).
            updateCapabilities(score, newNetwork, newNetwork.networkCapabilities);
        } else {
            processListenRequests(newNetwork, false);
        }

        // do this after the default net is switched, but
        // before LegacyTypeTracker sends legacy broadcasts
        for (NetworkRequestInfo nri : addedRequests) notifyNetworkAvailable(newNetwork, nri);

        // Linger any networks that are no longer needed. This should be done after sending the
        // available callback for newNetwork.
        for (NetworkAgentInfo nai : affectedNetworks) {
            updateLingerState(nai, now);
        }
        // Possibly unlinger newNetwork. Unlingering a network does not send any callbacks so it
        // does not need to be done in any particular order.
        updateLingerState(newNetwork, now);

        if (isNewDefault) {
            // Maintain the illusion: since the legacy API only
            // understands one network at a time, we must pretend
            // that the current default network disconnected before
            // the new one connected.
            if (oldDefaultNetwork != null) {
                mLegacyTypeTracker.remove(oldDefaultNetwork.networkInfo.getType(),
                                          oldDefaultNetwork, true);
            }
            mDefaultInetConditionPublished = newNetwork.lastValidated ? 100 : 0;
            mLegacyTypeTracker.add(newNetwork.networkInfo.getType(), newNetwork);
            notifyLockdownVpn(newNetwork);
        }

        if (keep) {
            // Notify battery stats service about this network, both the normal
            // interface and any stacked links.
            // TODO: Avoid redoing this; this must only be done once when a network comes online.
            try {
                final IBatteryStats bs = BatteryStatsService.getService();
                final int type = newNetwork.networkInfo.getType();

                final String baseIface = newNetwork.linkProperties.getInterfaceName();
                bs.noteNetworkInterfaceType(baseIface, type);
                for (LinkProperties stacked : newNetwork.linkProperties.getStackedLinks()) {
                    final String stackedIface = stacked.getInterfaceName();
                    bs.noteNetworkInterfaceType(stackedIface, type);
                    NetworkStatsFactory.noteStackedIface(stackedIface, baseIface);
                }
            } catch (RemoteException ignored) {
            }

            // This has to happen after the notifyNetworkCallbacks as that tickles each
            // ConnectivityManager instance so that legacy requests correctly bind dns
            // requests to this network.  The legacy users are listening for this bcast
            // and will generally do a dns request so they can ensureRouteToHost and if
            // they do that before the callbacks happen they'll use the default network.
            //
            // TODO: Is there still a race here? We send the broadcast
            // after sending the callback, but if the app can receive the
            // broadcast before the callback, it might still break.
            //
            // This *does* introduce a race where if the user uses the new api
            // (notification callbacks) and then uses the old api (getNetworkInfo(type))
            // they may get old info.  Reverse this after the old startUsing api is removed.
            // This is on top of the multiple intent sequencing referenced in the todo above.
            for (int i = 0; i < newNetwork.numNetworkRequests(); i++) {
                NetworkRequest nr = newNetwork.requestAt(i);
                if (nr.legacyType != TYPE_NONE && nr.isRequest()) {
                    // legacy type tracker filters out repeat adds
                    mLegacyTypeTracker.add(nr.legacyType, newNetwork);
                }
            }

            // A VPN generally won't get added to the legacy tracker in the "for (nri)" loop above,
            // because usually there are no NetworkRequests it satisfies (e.g., mDefaultRequest
            // wants the NOT_VPN capability, so it will never be satisfied by a VPN). So, add the
            // newNetwork to the tracker explicitly (it's a no-op if it has already been added).
            if (newNetwork.isVPN()) {
                mLegacyTypeTracker.add(TYPE_VPN, newNetwork);
            }
        }
        if (reapUnvalidatedNetworks == ReapUnvalidatedNetworks.REAP) {
            for (NetworkAgentInfo nai : mNetworkAgentInfos.values()) {
                if (unneeded(nai, UnneededFor.TEARDOWN)) {
                    if (nai.getLingerExpiry() > 0) {
                        // This network has active linger timers and no requests, but is not
                        // lingering. Linger it.
                        //
                        // One way (the only way?) this can happen if this network is unvalidated
                        // and became unneeded due to another network improving its score to the
                        // point where this network will no longer be able to satisfy any requests
                        // even if it validates.
                        updateLingerState(nai, now);
                    } else {
                        if (DBG) log("Reaping " + nai.name());
                        teardownUnneededNetwork(nai);
                    }
                }
            }
        }
    }


    private void sendUpdatedScoreToFactories(NetworkRequest networkRequest, int score == 0) {
        if (VDBG) log("sending new Min Network Score(" + score + "): " + networkRequest.toString());
        // 这里遍历所有注册到 ConnectivityService中的 NetworkFactoryInfo，都对应的目标Handler发送一个android.net.NetworkFactory.CMD_REQUEST_NETWORK 消息 
        // 注意 networkRequest 包含了  NetworkCapabilities 信息  NetworkCapabilities.NET_CAPABILITY_MMS 
        for (NetworkFactoryInfo nfi : mNetworkFactoryInfos.values()) {
            // nfi.asyncChannel 为 AsyncChannel 在 NetworkFactoryInfo注册进 ConnectivityService时 连接 如下：
            // nfi.asyncChannel.connect(mContext, mTrackerHandler, nfi.messenger);
            // //通过这里调用根据参数属性NetworkCapabilities.NET_CAPABILITY_MMS，我们进入到  TelephonyNetworkFactory 中，只有 TelephonyNetworkFactory会处理
            nfi.asyncChannel.sendMessage(android.net.NetworkFactory.CMD_REQUEST_NETWORK, score == 0 , 0,networkRequest);
        }
    }


    // EVENT_TIMEOUT_NETWORK_REQUEST 处理超时
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



///////////////////////////////////////////////////////////////////////////////////////////////////
    //frameworks/base/core/java/android/net/NetworkAgent.java:225 	netId = cm.registerNetworkAgent(new Messenger(this), new NetworkInfo(ni),
    //分析NetworkAgentInfo 的得来
    public int registerNetworkAgent(Messenger messenger, NetworkInfo networkInfo,LinkProperties linkProperties, NetworkCapabilities networkCapabilities,int currentScore, NetworkMisc networkMisc) {
        enforceConnectivityInternalPermission();

        // TODO: Instead of passing mDefaultRequest, provide an API to determine whether a Network
        // satisfies mDefaultRequest.
        final NetworkAgentInfo nai = new NetworkAgentInfo(messenger, new AsyncChannel(),
                new Network(reserveNetId()), new NetworkInfo(networkInfo), new LinkProperties(linkProperties), new NetworkCapabilities(networkCapabilities), currentScore,
                mContext, mTrackerHandler, new NetworkMisc(networkMisc), mDefaultRequest, this);

        synchronized (this) {
            nai.networkMonitor.systemReady = mSystemReady;
        }
        addValidationLogs(nai.networkMonitor.getValidationLogs(), nai.network,networkInfo.getExtraInfo());
        if (DBG) log("registerNetworkAgent " + nai);
        mHandler.sendMessage(mHandler.obtainMessage(EVENT_REGISTER_NETWORK_AGENT, nai));
        return nai.network.netId;
    }

    // 处理 EVENT_REGISTER_NETWORK_AGENT
    private void handleRegisterNetworkAgent(NetworkAgentInfo na) {
        if (VDBG) log("Got NetworkAgent Messenger");
        mNetworkAgentInfos.put(na.messenger, na);
        synchronized (mNetworkForNetId) {
            mNetworkForNetId.put(na.network.netId, na);
        }
        na.asyncChannel.connect(mContext, mTrackerHandler, na.messenger);
        NetworkInfo networkInfo = na.networkInfo;
        na.networkInfo = null;
        updateNetworkInfo(na, networkInfo);
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////// 
    // frameworks/base/core/java/android/net/NetworkFactory.java:121:            ConnectivityManager.from(mContext).registerNetworkFactory(mMessenger, LOG_TAG);
 
    //分析 mNetworkFactoryInfos 得来
    public void registerNetworkFactory(Messenger messenger, String name) {
        enforceConnectivityInternalPermission();
        NetworkFactoryInfo nfi = new NetworkFactoryInfo(name, messenger, new AsyncChannel());
        mHandler.sendMessage(mHandler.obtainMessage(EVENT_REGISTER_NETWORK_FACTORY, nfi));
    }

    // 处理 EVENT_REGISTER_NETWORK_REQUEST
    private void handleRegisterNetworkFactory(NetworkFactoryInfo nfi) {
        if (DBG) log("Got NetworkFactory Messenger for " + nfi.name);
        mNetworkFactoryInfos.put(nfi.messenger, nfi);
        // mTrackerHandler 为 源端  当 nfi.messenger 回发消息时，在此Handler中处理
        // nfi.messenger 为 目标端  nfi.asyncChannel 调用消息发送 都是发送此 Handler
        nfi.asyncChannel.connect(mContext, mTrackerHandler, nfi.messenger);
    }







}
