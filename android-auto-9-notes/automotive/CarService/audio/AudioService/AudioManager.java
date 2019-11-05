


public class AudioManager {


    /**
     * Returns an array of {@link AudioDeviceInfo} objects corresponding to the audio devices
     * currently connected to the system and meeting the criteria specified in the
     * <code>flags</code> parameter.
     * @param flags A set of bitflags specifying the criteria to test.
     * @see #GET_DEVICES_OUTPUTS
     * @see #GET_DEVICES_INPUTS
     * @see #GET_DEVICES_ALL
     * @return A (possibly zero-length) array of AudioDeviceInfo objects.
     */
    public AudioDeviceInfo[] getDevices(int flags) {
        return getDevicesStatic(flags);
    }



    /**
     * Generates a list of AudioDeviceInfo objects corresponding to the audio devices currently
     * connected to the system and meeting the criteria specified in the <code>flags</code>
     * parameter.
     * This is an internal function. The public API front is getDevices(int).
     * @param flags A set of bitflags specifying the criteria to test.
     * @see #GET_DEVICES_OUTPUTS
     * @see #GET_DEVICES_INPUTS
     * @see #GET_DEVICES_ALL
     * @return A (possibly zero-length) array of AudioDeviceInfo objects.
     * @hide
     */
    public static AudioDeviceInfo[] getDevicesStatic(int flags) {
        ArrayList<AudioDevicePort> ports = new ArrayList<AudioDevicePort>();
        int status = AudioManager.listAudioDevicePorts(ports);
        if (status != AudioManager.SUCCESS) {
            // fail and bail!
            return new AudioDeviceInfo[0];  // Always return an array.
        }

        return infoListFromPortList(ports, flags);
    }


    /**
     * Specialized version of listAudioPorts() listing only audio devices (AudioDevicePort)
     * @see listAudioPorts(ArrayList<AudioPort>)
     * @hide
     */
    public static int listAudioDevicePorts(ArrayList<AudioDevicePort> devices) {
        if (devices == null) {
            return ERROR_BAD_VALUE;
        }
        ArrayList<AudioPort> ports = new ArrayList<AudioPort>();
        int status = updateAudioPortCache(ports, null, null);
        if (status == SUCCESS) {
            filterDevicePorts(ports, devices);
        }
        return status;
    }



    static int updateAudioPortCache(ArrayList<AudioPort> ports, ArrayList<AudioPatch> patches /* = null */,
                                    ArrayList<AudioPort> previousPorts /* = null */) {
        /***
         * 
         */
        sAudioPortEventHandler.init();
        synchronized (sAudioPortGeneration) {

            if (sAudioPortGeneration == AUDIOPORT_GENERATION_INIT) {
                int[] patchGeneration = new int[1];
                int[] portGeneration = new int[1];
                int status;
                ArrayList<AudioPort> newPorts = new ArrayList<AudioPort>();
                ArrayList<AudioPatch> newPatches = new ArrayList<AudioPatch>();

                do {
                    newPorts.clear();
                    /**
                     * 通过 AudioSystem::listAudioPorts 先查找个数，分配内存，再获取信息
                     * 
                     *AudioSystem::listAudioPorts 
                     *--> AudioPolicyService::listAudioPorts
                     *---> AudioPolicyManager::listAudioPorts
                     *
                     * 直接看 AudioPolicyManager::listAudioPorts 
                     */
                    status = AudioSystem.listAudioPorts(newPorts, portGeneration);
                    if (status != SUCCESS) {
                        Log.w(TAG, "updateAudioPortCache: listAudioPorts failed");
                        return status;
                    }
                    newPatches.clear();
                    /**
                     * AudioSystem::listAudioPatches 
                     * --> AudioPolicyService::listAudioPatches 
                     * --> AudioPolicyManager::listAudioPatches
                     * 
                     * 直接看 AudioPolicyManager::listAudioPatches 
                     */
                    status = AudioSystem.listAudioPatches(newPatches, patchGeneration);
                    if (status != SUCCESS) {
                        Log.w(TAG, "updateAudioPortCache: listAudioPatches failed");
                        return status;
                    }
                    // Loop until patch generation is the same as port generation unless audio ports
                    // and audio patches are not null.
                } while (patchGeneration[0] != portGeneration[0]  && (ports == null || patches == null));
                
                // If the patch generation doesn't equal port generation, return ERROR here in case
                // of mismatch between audio ports and audio patches.
                if (patchGeneration[0] != portGeneration[0]) {
                    return ERROR;
                }

                for (int i = 0; i < newPatches.size(); i++) {
                    for (int j = 0; j < newPatches.get(i).sources().length; j++) {
                        AudioPortConfig portCfg = updatePortConfig(newPatches.get(i).sources()[j], newPorts);
                        newPatches.get(i).sources()[j] = portCfg;
                    }
                    for (int j = 0; j < newPatches.get(i).sinks().length; j++) {
                        AudioPortConfig portCfg = updatePortConfig(newPatches.get(i).sinks()[j],newPorts);
                        newPatches.get(i).sinks()[j] = portCfg;
                    }
                }
                for (Iterator<AudioPatch> i = newPatches.iterator(); i.hasNext(); ) {
                    AudioPatch newPatch = i.next();
                    boolean hasInvalidPort = false;
                    for (AudioPortConfig portCfg : newPatch.sources()) {
                        if (portCfg == null) {
                            hasInvalidPort = true;
                            break;
                        }
                    }
                    for (AudioPortConfig portCfg : newPatch.sinks()) {
                        if (portCfg == null) {
                            hasInvalidPort = true;
                            break;
                        }
                    }
                    if (hasInvalidPort) {
                        // Temporarily remove patches with invalid ports. One who created the patch
                        // is responsible for dealing with the port change.
                        i.remove();
                    }
                }

                sPreviousAudioPortsCached = sAudioPortsCached;
                sAudioPortsCached = newPorts;
                sAudioPatchesCached = newPatches;
                sAudioPortGeneration = portGeneration[0];
            }
            if (ports != null) {
                ports.clear();
                ports.addAll(sAudioPortsCached);
            }
            if (patches != null) {
                patches.clear();
                patches.addAll(sAudioPatchesCached);
            }
            if (previousPorts != null) {
                previousPorts.clear();
                previousPorts.addAll(sPreviousAudioPortsCached);
            }
        }
        return SUCCESS;
    }









     /**
     * @hide
     * Register the given {@link AudioPolicy}.
     * This call is synchronous and blocks until the registration process successfully completed
     * or failed to complete.
     * @param policy the non-null {@link AudioPolicy} to register.
     * @return {@link #ERROR} if there was an error communicating with the registration service
     *    or if the user doesn't have the required
     *    {@link android.Manifest.permission#MODIFY_AUDIO_ROUTING} permission,
     *    {@link #SUCCESS} otherwise.
     */
    @SystemApi
    @RequiresPermission(android.Manifest.permission.MODIFY_AUDIO_ROUTING)
    public int registerAudioPolicy(@NonNull AudioPolicy policy) {
        if (policy == null) {
            throw new IllegalArgumentException("Illegal null AudioPolicy argument");
        }
        final IAudioService service = getService();
        try {
            String regId = service.registerAudioPolicy(policy.getConfig(), 
                policy.cb(),policy.hasFocusListener(), policy.isFocusPolicy(), policy.isVolumeController());
            if (regId == null) {
                return ERROR;
            } else {
                policy.setRegistration(regId);
            }
            // successful registration
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
        return SUCCESS;
    }

















}