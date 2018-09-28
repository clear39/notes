/**
 * Maintain the Apn context
 */
public class ApnContext {

	public static int apnIdForNetworkRequest(NetworkRequest nr) {
        NetworkCapabilities nc = nr.networkCapabilities;
        // For now, ignore the bandwidth stuff
        if (nc.getTransportTypes().length > 0 && nc.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) == false) {
            return DctConstants.APN_INVALID_ID;
        }

        // in the near term just do 1-1 matches.
        // TODO - actually try to match the set of capabilities
        int apnId = DctConstants.APN_INVALID_ID;
        boolean error = false;

        if (nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)) {
            apnId = DctConstants.APN_DEFAULT_ID;
        }
        if (nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_MMS)) {
            if (apnId != DctConstants.APN_INVALID_ID) error = true;
            apnId = DctConstants.APN_MMS_ID; // 彩信这里返回
        }
        if (nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_SUPL)) {
            if (apnId != DctConstants.APN_INVALID_ID) error = true;
            apnId = DctConstants.APN_SUPL_ID;
        }
        if (nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_DUN)) {
            if (apnId != DctConstants.APN_INVALID_ID) error = true;
            apnId = DctConstants.APN_DUN_ID;
        }
        if (nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_FOTA)) {
            if (apnId != DctConstants.APN_INVALID_ID) error = true;
            apnId = DctConstants.APN_FOTA_ID;
        }
        if (nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_IMS)) {
            if (apnId != DctConstants.APN_INVALID_ID) error = true;
            apnId = DctConstants.APN_IMS_ID;
        }
        if (nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_CBS)) {
            if (apnId != DctConstants.APN_INVALID_ID) error = true;
            apnId = DctConstants.APN_CBS_ID;
        }
        if (nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_IA)) {
            if (apnId != DctConstants.APN_INVALID_ID) error = true;
            apnId = DctConstants.APN_IA_ID;
        }
        if (nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_RCS)) {
            if (apnId != DctConstants.APN_INVALID_ID) error = true;

            Rlog.d(SLOG_TAG, "RCS APN type not yet supported");
        }
        if (nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_XCAP)) {
            if (apnId != DctConstants.APN_INVALID_ID) error = true;

            Rlog.d(SLOG_TAG, "XCAP APN type not yet supported");
        }
        if (nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_EIMS)) {
            if (apnId != DctConstants.APN_INVALID_ID) error = true;
            apnId = DctConstants.APN_EMERGENCY_ID;
        }
        if (error) {
            // TODO: If this error condition is removed, the framework's handling of
            // NET_CAPABILITY_NOT_RESTRICTED will need to be updated so requests for
            // say FOTA and INTERNET are marked as restricted.  This is not how
            // NetworkCapabilities.maybeMarkCapabilitiesRestricted currently works.
            Rlog.d(SLOG_TAG, "Multiple apn types specified in request - result is unspecified!");
        }
        if (apnId == DctConstants.APN_INVALID_ID) {
            Rlog.d(SLOG_TAG, "Unsupported NetworkRequest in Telephony: nr=" + nr);
        }
        return apnId;
    }


     // TODO - kill The use of these strings
    public static int apnIdForApnName(String type) {
        switch (type) {
            case PhoneConstants.APN_TYPE_DEFAULT:
                return DctConstants.APN_DEFAULT_ID;
            case PhoneConstants.APN_TYPE_MMS:
                return DctConstants.APN_MMS_ID;
            case PhoneConstants.APN_TYPE_SUPL:
                return DctConstants.APN_SUPL_ID;
            case PhoneConstants.APN_TYPE_DUN:
                return DctConstants.APN_DUN_ID;
            case PhoneConstants.APN_TYPE_HIPRI:
                return DctConstants.APN_HIPRI_ID;
            case PhoneConstants.APN_TYPE_IMS:
                return DctConstants.APN_IMS_ID;
            case PhoneConstants.APN_TYPE_FOTA:
                return DctConstants.APN_FOTA_ID;
            case PhoneConstants.APN_TYPE_CBS:
                return DctConstants.APN_CBS_ID;
            case PhoneConstants.APN_TYPE_IA:
                return DctConstants.APN_IA_ID;
            case PhoneConstants.APN_TYPE_EMERGENCY:
                return DctConstants.APN_EMERGENCY_ID;
            default:
                return DctConstants.APN_INVALID_ID;
        }
    }





    public void requestNetwork(NetworkRequest networkRequest, LocalLog log) {
        synchronized (mRefCountLock) {
            if (mLocalLogs.contains(log) || mNetworkRequests.contains(networkRequest)) {
                log.log("ApnContext.requestNetwork has duplicate add - " + mNetworkRequests.size());
            } else {
                mLocalLogs.add(log);
                mNetworkRequests.add(networkRequest);
                mDcTracker.setEnabled(apnIdForApnName(mApnType), true);
            }
        }
    }



    /**
     * Check if ready for data call connection
     * @return True if ready, otherwise false.
     */
    public boolean isReady() {
        return mDataEnabled.get() && mDependencyMet.get();
    }

}