

/**
 * API to the broadcast radio service.
 *
 * {@hide}
 */
interface IRadioService {
    List<RadioManager.ModuleProperties> listModules();

    ITuner openTuner(int moduleId, in RadioManager.BandConfig bandConfig, boolean withAudio,in ITunerCallback callback);

    ICloseHandle addAnnouncementListener(in int[] enabledTypes,in IAnnouncementListener listener);
}
