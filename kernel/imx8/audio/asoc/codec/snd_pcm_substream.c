//  @   include/sound/soc.h

/* SoC PCM stream information */
struct snd_soc_pcm_stream {
    const char *stream_name;
    u64 formats;            /* SNDRV_PCM_FMTBIT_* */
    unsigned int rates;     /* SNDRV_PCM_RATE_* */
    unsigned int rate_min;      /* min rate */
    unsigned int rate_max;      /* max rate */
    unsigned int channels_min;  /* min channels */
    unsigned int channels_max;  /* max channels */
    unsigned int sig_bits;      /* number of bits of content */
};
