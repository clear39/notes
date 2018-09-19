/** @see AudioManager#requestAudioFocus(AudioManager.OnAudioFocusChangeListener, int, int, int) */
protected int requestAudioFocus(AudioAttributes aa, int focusChangeHint, IBinder cb, IAudioFocusDispatcher fd, String clientId, String callingPackageName, int flags,int sdk) {
	Log.i(TAG, " AudioFocus  requestAudioFocus() from uid/pid " + Binder.getCallingUid() + "/" + Binder.getCallingPid() + 
			" clientId=" + clientId + " req=" + focusChangeHint + " flags=0x" + Integer.toHexString(flags));
	// we need a valid binder callback for clients
	if (!cb.pingBinder()) {
	    Log.e(TAG, " AudioFocus DOA client for requestAudioFocus(), aborting.");
	    return AudioManager.AUDIOFOCUS_REQUEST_FAILED;
	}

	if (mAppOps.noteOp(AppOpsManager.OP_TAKE_AUDIO_FOCUS, Binder.getCallingUid(),callingPackageName) != AppOpsManager.MODE_ALLOWED) {
	    return AudioManager.AUDIOFOCUS_REQUEST_FAILED;
	}

	synchronized(mAudioFocusLock) {
            // public final static String IN_VOICE_COMM_FOCUS_ID = "AudioFocus_For_Phone_Ring_And_Calls";
	    boolean enteringRingOrCall = !mRingOrCallActive & (AudioSystem.IN_VOICE_COMM_FOCUS_ID.compareTo(clientId) == 0);
	    if (enteringRingOrCall) { 
		mRingOrCallActive = true; 
	    }

	    final AudioFocusInfo afiForExtPolicy;
	    if (mFocusPolicy != null) {
		// construct AudioFocusInfo as it will be communicated to audio focus policy
		afiForExtPolicy = new AudioFocusInfo(aa, Binder.getCallingUid(),clientId, callingPackageName, focusChangeHint, 0 /*lossReceived*/,flags, sdk);
	    } else {
		afiForExtPolicy = null;
	    }

	    // handle delayed focus
	    boolean focusGrantDelayed = false;
	    if (!canReassignAudioFocus()) {
		if ((flags & AudioManager.AUDIOFOCUS_FLAG_DELAY_OK) == 0) {
		    final int result = AudioManager.AUDIOFOCUS_REQUEST_FAILED;
		    notifyExtFocusPolicyFocusRequest_syncAf(afiForExtPolicy, result, fd, cb);
		    return result;
		} else {
		    // request has AUDIOFOCUS_FLAG_DELAY_OK: focus can't be
		    // granted right now, so the requester will be inserted in the focus stack
		    // to receive focus later
		    focusGrantDelayed = true;
		}
	    }

	    // external focus policy: delay request for focus gain?
	    final int resultWithExtPolicy = AudioManager.AUDIOFOCUS_REQUEST_DELAYED;
	    if (notifyExtFocusPolicyFocusRequest_syncAf(afiForExtPolicy, resultWithExtPolicy, fd, cb)) {
		// stop handling focus request here as it is handled by external audio focus policy
		return resultWithExtPolicy;
	    }

	    // handle the potential premature death of the new holder of the focus
	    // (premature death == death before abandoning focus)
	    // Register for client death notification
	    AudioFocusDeathHandler afdh = new AudioFocusDeathHandler(cb);

	    try {
		cb.linkToDeath(afdh, 0);
	    } catch (RemoteException e) {
		// client has already died!
		Log.w(TAG, "AudioFocus  requestAudioFocus() could not link to "+cb+" binder death");
		return AudioManager.AUDIOFOCUS_REQUEST_FAILED;
	    }

	    if (!mFocusStack.empty() && mFocusStack.peek().hasSameClient(clientId)) {
		// if focus is already owned by this client and the reason for acquiring the focus
		// hasn't changed, don't do anything
		final FocusRequester fr = mFocusStack.peek();
		if (fr.getGainRequest() == focusChangeHint && fr.getGrantFlags() == flags) {
		    // unlink death handler so it can be gc'ed.
		    // linkToDeath() creates a JNI global reference preventing collection.
		    cb.unlinkToDeath(afdh, 0);
		    notifyExtPolicyFocusGrant_syncAf(fr.toAudioFocusInfo(),AudioManager.AUDIOFOCUS_REQUEST_GRANTED);
		    return AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
		}
		// the reason for the audio focus request has changed: remove the current top of
		// stack and respond as if we had a new focus owner
		if (!focusGrantDelayed) {
		    mFocusStack.pop();
		    // the entry that was "popped" is the same that was "peeked" above
		    fr.release();
		}
	    }

	    // focus requester might already be somewhere below in the stack, remove it
	    removeFocusStackEntry(clientId, false /* signal */, false /*notifyFocusFollowers*/);

	    final FocusRequester nfr = new FocusRequester(aa, focusChangeHint, flags, fd, cb,clientId, afdh, callingPackageName, Binder.getCallingUid(), this, sdk);
	    if (focusGrantDelayed) {
		// focusGrantDelayed being true implies we can't reassign focus right now
		// which implies the focus stack is not empty.
		final int requestResult = pushBelowLockedFocusOwners(nfr);
		if (requestResult != AudioManager.AUDIOFOCUS_REQUEST_FAILED) {
		    notifyExtPolicyFocusGrant_syncAf(nfr.toAudioFocusInfo(), requestResult);
		}
		return requestResult;
	    } else {
		// propagate the focus change through the stack
		if (!mFocusStack.empty()) {
		    propagateFocusLossFromGain_syncAf(focusChangeHint, nfr);
		}

		// push focus requester at the top of the audio focus stack
		mFocusStack.push(nfr);
		nfr.handleFocusGainFromRequest(AudioManager.AUDIOFOCUS_REQUEST_GRANTED);
	    }
	    notifyExtPolicyFocusGrant_syncAf(nfr.toAudioFocusInfo(),AudioManager.AUDIOFOCUS_REQUEST_GRANTED);

	    if (ENFORCE_MUTING_FOR_RING_OR_CALL & enteringRingOrCall) {
		runAudioCheckerForRingOrCallAsync(true/*enteringRingOrCall*/);
	    }
	}//synchronized(mAudioFocusLock)

	return AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
}
