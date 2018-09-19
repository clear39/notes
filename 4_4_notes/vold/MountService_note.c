    /**
     * Callback from NativeDaemonConnector
     */
    public boolean onEvent(int code, String raw, String[] cooked) {

        if (DEBUG_EVENTS) {
            StringBuilder builder = new StringBuilder();
            builder.append("onEvent::");
            builder.append(" raw= " + raw);
            if (cooked != null) {
                builder.append(" cooked = " );
                for (String str : cooked) {
                    builder.append(" " + str);
                }
            }
            Slog.i(TAG, builder.toString());
        }

        if (code == VoldResponseCode.VolumeStateChange) {
            /*
             * One of the volumes we're managing has changed state.
             * Format: "NNN Volume <label> <path> state changed
             * from <old_#> (<old_str>) to <new_#> (<new_str>)"
             */
            notifyVolumeStateChange(cooked[2], cooked[3], Integer.parseInt(cooked[7]),Integer.parseInt(cooked[10]));
        } else if (code == VoldResponseCode.VolumeUuidChange) {
            // Format: nnn <label> <path> <uuid>
            final String path = cooked[2];
            final String uuid = (cooked.length > 3) ? cooked[3] : null;

            final StorageVolume vol = mVolumesByPath.get(path);
            if (vol != null) {
                vol.setUuid(uuid);
            }

        } else if (code == VoldResponseCode.VolumeUserLabelChange) {
            // Format: nnn <label> <path> <label>
            final String path = cooked[2];
            final String userLabel = (cooked.length > 3) ? cooked[3] : null;

            final StorageVolume vol = mVolumesByPath.get(path);
            if (vol != null) {
                vol.setUserLabel(userLabel);
            }
        } else if (code == VoldResponseCode.VolumeDiskConnected) {
            // FMT: NNN Volume <label> disk connected
            final String label = cooked[2];
            final String state = cooked[4];

            Slog.d(TAG, "VolumeDiskConnected, label:" + label + ", state:" + state);

            setStorageState(label, state);
            sendStorageIntent(Intent.ACTION_MEDIA_CONNECTED, label, state, UserHandle.ALL);

            playSound(VoldResponseCode.VolumeDiskConnected);
        } else if (code == VoldResponseCode.VolumeDiskDisconnected) {
            // FMT: NNN Volume <label> disk disconnected
            final String label = cooked[2];
            final String state = cooked[4];

            Slog.d(TAG, "VolumeDiskDisConnected, label:" + label + ", state:" + state);

            setStorageState(label, state);
            sendStorageIntent(Intent.ACTION_MEDIA_DISCONNECTED, label, state, UserHandle.ALL);
            //playSound(VoldResponseCode.VolumeDiskDisconnected);
        } else if(code == VoldResponseCode.VolumeIphoneConnected){
	    //tubao
	    final String label = cooked[2];
            final String state = cooked[4];

            Slog.d(TAG, "VolumeDiskConnected, label:" + label + ", state:" + state);

            setStorageState(label, state);
            sendStorageIntent(Intent.ACTION_IPHONE_CONNECTED, label, state, UserHandle.ALL);


        } else if(code == VoldResponseCode.VolumeIphoneCarlifeExit){
			//tubao
			final String label = cooked[2];
            final String state = cooked[4];

            Slog.d(TAG, "VolumeDiskConnected && carlife exit, label:" + label + ", state:" + state);

            setStorageState(label, state);
            sendStorageIntent(Intent.ACTION_IPHONE_EXIT_APP, label, state, UserHandle.ALL);


        } else if(code == VoldResponseCode.VolumeIphoneDisconnected){
	    //tubao
	    final String label = cooked[2];
            final String state = cooked[4];

            Slog.d(TAG, "VolumeDiskDisConnected, label:" + label + ", state:" + state);

            setStorageState(label, state);
            sendStorageIntent(Intent.ACTION_IPHONE_DISCONNECTED, label, state, UserHandle.ALL);

        } else if (code == VoldResponseCode.VolumeDiskCommFailure) {
            // FMT: NNN Volume <label> disk comm_failure
            final String label = cooked[2];
            final String state = cooked[4];

            Slog.d(TAG, "VolumeDiskCommFailure, label:" + label + ", state:" + state);

            setStorageState(label, state);
            sendStorageIntent(Intent.ACTION_MEDIA_COMM_FAILURE, label, state, UserHandle.ALL);

        } else if ((code == VoldResponseCode.VolumeDiskInserted) ||
                   (code == VoldResponseCode.VolumeDiskRemoved) ||
                   (code == VoldResponseCode.VolumeBadRemoval)) {
            // FMT: NNN Volume <label> <mountpoint> disk inserted (<major>:<minor>)
            // FMT: NNN Volume <label> <mountpoint> disk removed (<major>:<minor>)
            // FMT: NNN Volume <label> <mountpoint> bad removal (<major>:<minor>)
            String action = null;
            final String label = cooked[2];
            final String path = cooked[3];
            int major = -1;
            int minor = -1;

            try {
                String devComp = cooked[6].substring(1, cooked[6].length() -1);
                String[] devTok = devComp.split(":");
                major = Integer.parseInt(devTok[0]);
                minor = Integer.parseInt(devTok[1]);
            } catch (Exception ex) {
                Slog.e(TAG, "Failed to parse major/minor", ex);
            }

            final StorageVolume volume;
            final String state;
            synchronized (mVolumesLock) {
                volume = mVolumesByPath.get(path);
                state = mVolumeStates.get(path);
            }

            if (code == VoldResponseCode.VolumeDiskInserted) {
                new Thread("MountService#VolumeDiskInserted") {
                    @Override
                    public void run() {
                        try {
                            int rc;
                            if ((rc = doMountVolume(path)) != StorageResultCode.OperationSucceeded) {
                                Slog.w(TAG, String.format("Insertion mount failed (%d)", rc));
                            }
                        } catch (Exception ex) {
                            Slog.w(TAG, "Failed to mount media on insertion", ex);
                        }
                    }
                }.start();
            } else if (code == VoldResponseCode.VolumeDiskRemoved) {
                /*
                 * This event gets trumped if we're already in BAD_REMOVAL state
                 */
                if (getVolumeState(path).equals(Environment.MEDIA_BAD_REMOVAL)) {
                    return true;
                }

                setStorageState(label, StorageManager.StorageState.Unmounted);

                /* Send the media unmounted event first */
                if (DEBUG_EVENTS) Slog.i(TAG, "Sending unmounted event first");
                updatePublicVolumeState(volume, Environment.MEDIA_UNMOUNTED);
                sendStorageIntent(Environment.MEDIA_UNMOUNTED, volume, UserHandle.ALL);

                if (DEBUG_EVENTS) Slog.i(TAG, "Sending media removed");
                updatePublicVolumeState(volume, Environment.MEDIA_REMOVED);
                action = Intent.ACTION_MEDIA_REMOVED;
            } else if (code == VoldResponseCode.VolumeBadRemoval) {
                setStorageState(label, StorageManager.StorageState.Unmounted);

                if (DEBUG_EVENTS) Slog.i(TAG, "Sending unmounted event first");
                /* Send the media unmounted event first */
                updatePublicVolumeState(volume, Environment.MEDIA_UNMOUNTED);
                sendStorageIntent(Intent.ACTION_MEDIA_UNMOUNTED, volume, UserHandle.ALL);

                if (DEBUG_EVENTS) Slog.i(TAG, "Sending media bad removal");
                updatePublicVolumeState(volume, Environment.MEDIA_BAD_REMOVAL);
                action = Intent.ACTION_MEDIA_BAD_REMOVAL;
                playSound(VoldResponseCode.VolumeBadRemoval);
            } else if (code == VoldResponseCode.FstrimCompleted) {
                EventLogTags.writeFstrimFinish(SystemClock.elapsedRealtime());
            } else {
                Slog.e(TAG, String.format("Unknown code {%d}", code));
            }

            if (action != null) {
                sendStorageIntent(action, volume, UserHandle.ALL);
            }
        } else {
            return false;
        }

        return true;
    }
