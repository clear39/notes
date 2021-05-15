//  @   sound/core/pcm.c
/**
 * snd_pcm_new - create a new PCM instance
 * @card: the card instance
 * @id: the id string
 * @device: the device index (zero based)
 * @playback_count: the number of substreams for playback
 * @capture_count: the number of substreams for capture
 * @rpcm: the pointer to store the new pcm instance
 *
 * Creates a new PCM instance.
 *
 * The pcm operators have to be set afterwards to the new instance
 * via snd_pcm_set_ops().
 *
 * Return: Zero if successful, or a negative error code on failure.
 */
int snd_pcm_new(struct snd_card *card, const char *id, int device,                                                                                                                                             
        int playback_count, int capture_count, struct snd_pcm **rpcm)
{
    return _snd_pcm_new(card, id, device, playback_count, capture_count,false, rpcm);
}
EXPORT_SYMBOL(snd_pcm_new);



static int _snd_pcm_new(struct snd_card *card, const char *id, int device,
        int playback_count, int capture_count, bool internal,
        struct snd_pcm **rpcm)
{
    struct snd_pcm *pcm;
    int err;
    static struct snd_device_ops ops = {
        .dev_free = snd_pcm_dev_free,
        .dev_register = snd_pcm_dev_register,
        .dev_disconnect = snd_pcm_dev_disconnect,
    };
    
    if (snd_BUG_ON(!card))
        return -ENXIO;

    if (rpcm) 
        *rpcm = NULL;
        
    pcm = kzalloc(sizeof(*pcm), GFP_KERNEL);
    if (!pcm)
        return -ENOMEM;
    pcm->card = card;
    pcm->device = device;
    pcm->internal = internal;
    mutex_init(&pcm->open_mutex);
    init_waitqueue_head(&pcm->open_wait);
    INIT_LIST_HEAD(&pcm->list);
    if (id)
        strlcpy(pcm->id, id, sizeof(pcm->id));

    if ((err = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_PLAYBACK, playback_count)) < 0) {
        snd_pcm_free(pcm);
        return err;
    }
    if ((err = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_CAPTURE, capture_count)) < 0) {
        snd_pcm_free(pcm);
        return err;
    }
    if ((err = snd_device_new(card, SNDRV_DEV_PCM, pcm, &ops)) < 0) {
        snd_pcm_free(pcm);
        return err;
    }
    
    if (rpcm) 
        *rpcm = pcm;
    return 0;
}
