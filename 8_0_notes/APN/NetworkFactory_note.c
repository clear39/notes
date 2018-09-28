/**
 * A NetworkFactory is an entity（实体） that creates NetworkAgent objects.
 * The bearers（持有） register with ConnectivityService using {@link #register} and
 * their factory will start receiving scored NetworkRequests.  NetworkRequests
 * can be filtered 3 ways: by NetworkCapabilities, by score and more complexly by
 * overridden function.  All of these can be dynamic - changing NetworkCapabilities
 * or score forces re-evaluation of all current requests.
 *
 * If any requests pass the filter some overrideable functions will be called.
 * If the bearer only cares about very simple start/stopNetwork callbacks, those
 * functions can be overridden.  If the bearer needs more interaction, it can
 * override addNetworkRequest and removeNetworkRequest which will give it each
 * request that passes their current filters.
 * @hide
 **/
public class NetworkFactory extends Handler {


	/*
	*  TelephonyNetworkFactory  
	*  super(looper, context, "TelephonyNetworkFactory[" + phoneId + "]", null);  // 其中 phoneId 表示 电话几个，一个为0； 双卡则为0,1
	*/

	public NetworkFactory(Looper looper, Context context, String logTag,NetworkCapabilities filter) {
        super(looper);
        LOG_TAG = logTag;
        mContext = context;
        mCapabilityFilter = filter;
    }

    public void register() {
        if (DBG) log("Registering NetworkFactory");
        if (mMessenger == null) {
            mMessenger = new Messenger(this);
            ConnectivityManager.from(mContext).registerNetworkFactory(mMessenger, LOG_TAG);
        }
    }

    public void unregister() {
        if (DBG) log("Unregistering NetworkFactory");
        if (mMessenger != null) {
            ConnectivityManager.from(mContext).unregisterNetworkFactory(mMessenger);
            mMessenger = null;
        }
    }



    @Override
    public void handleMessage(Message msg) {
        switch (msg.what) {
            case CMD_REQUEST_NETWORK: {
                handleAddRequest((NetworkRequest)msg.obj, msg.arg1);
                break;
            }
            case CMD_CANCEL_REQUEST: {
                handleRemoveRequest((NetworkRequest) msg.obj);
                break;
            }
            case CMD_SET_SCORE: {
                handleSetScore(msg.arg1);
                break;
            }
            case CMD_SET_FILTER: {
                handleSetFilter((NetworkCapabilities) msg.obj);
                break;
            }
        }
    }


     @VisibleForTesting
    protected void handleAddRequest(NetworkRequest request, int score) {
        NetworkRequestInfo n = mNetworkRequests.get(request.requestId);//第一次为null
        if (n == null) { // 执行这里
            if (DBG) log("got request " + request + " with score " + score);
            n = new NetworkRequestInfo(request, score);
            mNetworkRequests.put(n.request.requestId, n);
        } else {
            if (VDBG) log("new score " + score + " for exisiting request " + request);
            n.score = score;
        }
        if (VDBG) log("  my score=" + mScore + ", my filter=" + mCapabilityFilter);

        evalRequest(n);
    }

    private void evalRequest(NetworkRequestInfo n) {
        if (VDBG) log("evalRequest");
        // 由于第一次调用 n.requested == false ； 
        // n.score  == 0,TelephonyNetworkFactory.mScore == 50 ;
        // 
        if (n.requested == false && n.score < mScore && n.request.networkCapabilities.satisfiedByNetworkCapabilities(mCapabilityFilter) && acceptRequest(n.request, n.score)) {
            if (VDBG) log("  needNetworkFor");
            needNetworkFor(n.request, n.score);  //由于TelephonyNetworkFactory重载了，进入到 TelephonyNetworkFactory.needNetworkFor
            n.requested = true;
        } else if (n.requested == true && (n.score > mScore || n.request.networkCapabilities.satisfiedByNetworkCapabilities(mCapabilityFilter) == false || acceptRequest(n.request, n.score) == false)) {
            if (VDBG) log("  releaseNetworkFor");
            releaseNetworkFor(n.request);
            n.requested = false;
        } else {
            if (VDBG) log("  done");
        }
    }



}