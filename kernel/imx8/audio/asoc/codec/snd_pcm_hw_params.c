

//  @   include/uapi/sound/asound.h
struct snd_pcm_hw_params {                                                                                                                                                                                     
    unsigned int flags;
    struct snd_mask masks[SNDRV_PCM_HW_PARAM_LAST_MASK - 
                   SNDRV_PCM_HW_PARAM_FIRST_MASK + 1];
    struct snd_mask mres[5];    /* reserved masks */
    struct snd_interval intervals[SNDRV_PCM_HW_PARAM_LAST_INTERVAL -
                        SNDRV_PCM_HW_PARAM_FIRST_INTERVAL + 1];
    struct snd_interval ires[9];    /* reserved intervals */
    unsigned int rmask;     /* W: requested masks */
    unsigned int cmask;     /* R: changed masks */
    unsigned int info;      /* R: Info flags for returned setup */
    unsigned int msbits;        /* R: used most significant bits */
    unsigned int rate_num;      /* R: rate numerator */
    unsigned int rate_den;      /* R: rate denominator */
    snd_pcm_uframes_t fifo_size;    /* R: chip FIFO size in frames */
    unsigned char reserved[64]; /* reserved for future */
};
