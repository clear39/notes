

//sPhoneNotifier = new DefaultPhoneNotifier();

public class DefaultPhoneNotifier implements PhoneNotifier {
    
    public DefaultPhoneNotifier() {
        mRegistry = ITelephonyRegistry.Stub.asInterface(ServiceManager.getService("telephony.registry"));
    }

}



public interface PhoneNotifier {

    public void notifyPhoneState(Phone sender);

    public void notifyServiceState(Phone sender);

    public void notifyCellLocation(Phone sender);

    public void notifySignalStrength(Phone sender);

    public void notifyMessageWaitingChanged(Phone sender);

    public void notifyCallForwardingChanged(Phone sender);

    /** TODO - reason should never be null */
    public void notifyDataConnection(Phone sender, String reason, String apnType,
            PhoneConstants.DataState state);

    public void notifyDataConnectionFailed(Phone sender, String reason, String apnType);

    public void notifyDataActivity(Phone sender);

    public void notifyOtaspChanged(Phone sender, int otaspMode);

    public void notifyCellInfo(Phone sender, List<CellInfo> cellInfo);

    public void notifyPreciseCallState(Phone sender);

    public void notifyDisconnectCause(int cause, int preciseCause);

    public void notifyPreciseDataConnectionFailed(Phone sender, String reason, String apnType,
            String apn, String failCause);

    public void notifyVoLteServiceStateChanged(Phone sender, VoLteServiceState lteState);

    public void notifyVoiceActivationStateChanged(Phone sender, int activationState);

    public void notifyDataActivationStateChanged(Phone sender, int activationState);

    public void notifyOemHookRawEventForSubscriber(int subId, byte[] rawData);
}

