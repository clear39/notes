

class TunerCallbackAdapter extends ITunerCallback.Stub {

    TunerCallbackAdapter(@NonNull RadioTuner.Callback callback, @Nullable Handler handler) {
        mCallback = callback;
        if (handler == null) {
            mHandler = new Handler(Looper.getMainLooper());
        } else {
            mHandler = handler;
        }
    }


    @Override
    public void onConfigurationChanged(RadioManager.BandConfig config) {
        mHandler.post(() -> mCallback.onConfigurationChanged(config));
    }

    @Override
    public void onTuneFailed(int status, @Nullable ProgramSelector selector) {
        mHandler.post(() -> mCallback.onTuneFailed(status, selector));

        int errorCode;
        switch (status) {
            case RadioManager.STATUS_PERMISSION_DENIED:
            case RadioManager.STATUS_DEAD_OBJECT:
                errorCode = RadioTuner.ERROR_SERVER_DIED;
                break;
            case RadioManager.STATUS_ERROR:
            case RadioManager.STATUS_NO_INIT:
            case RadioManager.STATUS_BAD_VALUE:
            case RadioManager.STATUS_INVALID_OPERATION:
                Log.i(TAG, "Got an error with no mapping to the legacy API (" + status
                        + "), doing a best-effort conversion to ERROR_SCAN_TIMEOUT");
            // fall through
            case RadioManager.STATUS_TIMED_OUT:
            default:
                errorCode = RadioTuner.ERROR_SCAN_TIMEOUT;
        }
        mHandler.post(() -> mCallback.onError(errorCode));
    }


    @Override
    public void onCurrentProgramInfoChanged(RadioManager.ProgramInfo info) {
        if (info == null) {
            Log.e(TAG, "ProgramInfo must not be null");
            return;
        }

        synchronized (mLock) {
            mCurrentProgramInfo = info;
        }

        mHandler.post(() -> {
            mCallback.onProgramInfoChanged(info);

            RadioMetadata metadata = info.getMetadata();
            if (metadata != null) mCallback.onMetadataChanged(metadata);
        });
    }


    @Override
    public void onTrafficAnnouncement(boolean active) {
        mHandler.post(() -> mCallback.onTrafficAnnouncement(active));
    }

    @Override
    public void onEmergencyAnnouncement(boolean active) {
        mHandler.post(() -> mCallback.onEmergencyAnnouncement(active));
    }

    @Override
    public void onAntennaState(boolean connected) {
        mIsAntennaConnected = connected;
        mHandler.post(() -> mCallback.onAntennaState(connected));
    }

    @Override
    public void onBackgroundScanAvailabilityChange(boolean isAvailable) {
        mHandler.post(() -> mCallback.onBackgroundScanAvailabilityChange(isAvailable));
    }

    private void sendBackgroundScanCompleteLocked() {
        mDelayedCompleteCallback = false;
        mHandler.post(() -> mCallback.onBackgroundScanComplete());
    }

    @Override
    public void onBackgroundScanComplete() {
        synchronized (mLock) {
            if (mLastCompleteList == null) {
                Log.i(TAG, "Got onBackgroundScanComplete callback, but the "
                        + "program list didn't get through yet. Delaying it...");
                mDelayedCompleteCallback = true;
                return;
            }
            sendBackgroundScanCompleteLocked();
        }
    }

    @Override
    public void onProgramListChanged() {
        mHandler.post(() -> mCallback.onProgramListChanged());
    }

    @Override
    public void onProgramListUpdated(ProgramList.Chunk chunk) {
        synchronized (mLock) {
            if (mProgramList == null) return;
            mProgramList.apply(Objects.requireNonNull(chunk));
        }
    }

    @Override
    public void onParametersUpdated(Map parameters) {
        mHandler.post(() -> mCallback.onParametersUpdated(parameters));
    }

}