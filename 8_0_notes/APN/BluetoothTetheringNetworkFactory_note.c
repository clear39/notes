/**
 * This class tracks the data connection associated with Bluetooth
 * reverse tethering. PanService calls it when a reverse tethered
 * connection needs to be activated or deactivated.
 *
 * @hide
 */
public class BluetoothTetheringNetworkFactory extends NetworkFactory {
	
	private static final String NETWORK_TYPE = "Bluetooth Tethering";

	public BluetoothTetheringNetworkFactory(Context context, Looper looper, PanService panService) {
        super(looper, context, NETWORK_TYPE, new NetworkCapabilities());

        mContext = context;
        mPanService = panService;

        mNetworkInfo = new NetworkInfo(ConnectivityManager.TYPE_BLUETOOTH, 0, NETWORK_TYPE, "");
        mNetworkCapabilities = new NetworkCapabilities();
        initNetworkCapabilities();
        setCapabilityFilter(mNetworkCapabilities);
    }

    private void initNetworkCapabilities() {
        mNetworkCapabilities.addTransportType(NetworkCapabilities.TRANSPORT_BLUETOOTH);
        mNetworkCapabilities.addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET);
        mNetworkCapabilities.addCapability(NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED);
        // Bluetooth v3 and v4 go up to 24 Mbps.
        // TODO: Adjust this to actual connection bandwidth.
        mNetworkCapabilities.setLinkUpstreamBandwidthKbps(24 * 1000);
        mNetworkCapabilities.setLinkDownstreamBandwidthKbps(24 * 1000);
    }




    // Called by PanService when a network interface for Bluetooth reverse-tethering
    // becomes available.  We register our NetworkFactory at this point.
    public void startReverseTether(final String iface) {
        if (iface == null || TextUtils.isEmpty(iface)) {
            Slog.e(TAG, "attempted to reverse tether with empty interface");
            return;
        }
        synchronized(this) {
            if (!TextUtils.isEmpty(mInterfaceName)) {
                Slog.e(TAG, "attempted to reverse tether while already in process");
                return;
            }
            mInterfaceName = iface;
            // Advertise ourselves to ConnectivityService.
            register();
            setScoreFilter(NETWORK_SCORE);
        }
    }







}