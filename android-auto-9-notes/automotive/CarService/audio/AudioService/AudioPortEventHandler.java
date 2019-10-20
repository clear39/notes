

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/media/java/android/media/AudioPortEventHandler.java
class AudioPortEventHandler {


    void init() {
        synchronized (this) {
            /***
             * 静态全局定义，单实例判断
             */
            if (mHandler != null) {
                return;
            }
            // create a new thread for our new event handler
            /***
             * private static final String TAG = "AudioPortEventHandler";
             */
            mHandlerThread = new HandlerThread(TAG);
            mHandlerThread.start();

            if (mHandlerThread.getLooper() != null) {
                mHandler = new Handler(mHandlerThread.getLooper()) {
                    @Override
                    public void handleMessage(Message msg) {
                        ArrayList<AudioManager.OnAudioPortUpdateListener> listeners;
                        synchronized (this) {
                            if (msg.what == AUDIOPORT_EVENT_NEW_LISTENER) {
                                listeners = new ArrayList<AudioManager.OnAudioPortUpdateListener>();
                                if (mListeners.contains(msg.obj)) {
                                    listeners.add((AudioManager.OnAudioPortUpdateListener)msg.obj);
                                }
                            } else {
                                listeners = mListeners;
                            }
                        }
                        // reset audio port cache if the event corresponds to a change coming
                        // from audio policy service or if mediaserver process died.
                        if (msg.what == AUDIOPORT_EVENT_PORT_LIST_UPDATED ||
                                msg.what == AUDIOPORT_EVENT_PATCH_LIST_UPDATED ||
                                msg.what == AUDIOPORT_EVENT_SERVICE_DIED) {
                            AudioManager.resetAudioPortGeneration();
                        }

                        if (listeners.isEmpty()) {
                            return;
                        }

                        ArrayList<AudioPort> ports = new ArrayList<AudioPort>();
                        ArrayList<AudioPatch> patches = new ArrayList<AudioPatch>();
                        if (msg.what != AUDIOPORT_EVENT_SERVICE_DIED) {
                            int status = AudioManager.updateAudioPortCache(ports, patches, null);
                            if (status != AudioManager.SUCCESS) {
                                // Since audio ports and audio patches are not null, the return
                                // value could be ERROR due to inconsistency between port generation
                                // and patch generation. In this case, we need to reschedule the
                                // message to make sure the native callback is done.
                                sendMessageDelayed(obtainMessage(msg.what, msg.obj),
                                        RESCHEDULE_MESSAGE_DELAY_MS);
                                return;
                            }
                        }

                        switch (msg.what) {
                        case AUDIOPORT_EVENT_NEW_LISTENER:
                        case AUDIOPORT_EVENT_PORT_LIST_UPDATED:
                            AudioPort[] portList = ports.toArray(new AudioPort[0]);
                            for (int i = 0; i < listeners.size(); i++) {
                                listeners.get(i).onAudioPortListUpdate(portList);
                            }
                            if (msg.what == AUDIOPORT_EVENT_PORT_LIST_UPDATED) {
                                break;
                            }
                            // FALL THROUGH

                        case AUDIOPORT_EVENT_PATCH_LIST_UPDATED:
                            AudioPatch[] patchList = patches.toArray(new AudioPatch[0]);
                            for (int i = 0; i < listeners.size(); i++) {
                                listeners.get(i).onAudioPatchListUpdate(patchList);
                            }
                            break;

                        case AUDIOPORT_EVENT_SERVICE_DIED:
                            for (int i = 0; i < listeners.size(); i++) {
                                listeners.get(i).onServiceDied();
                            }
                            break;

                        default:
                            break;
                        }
                    }
                };
                native_setup(new WeakReference<AudioPortEventHandler>(this));
            } else {
                mHandler = null;
            }
        }
    }


}