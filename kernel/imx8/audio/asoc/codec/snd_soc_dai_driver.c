

//  @   include/sound/soc-dai.h
/*
 * Digital Audio Interface Driver.
 *
 * Describes the Digital Audio Interface in terms of its ALSA, DAI and AC97
 * operations and capabilities. Codec and platform drivers will register this
 * structure for every DAI they have.
 *
 * This structure covers the clocking, formating and ALSA operations for each
 * interface.
 */
struct snd_soc_dai_driver {
    /* DAI description */
    const char *name;
    unsigned int id;
    unsigned int base;
    struct snd_soc_dobj dobj;

    /* DAI driver callbacks */
    int (*probe)(struct snd_soc_dai *dai);
    int (*remove)(struct snd_soc_dai *dai);
    int (*suspend)(struct snd_soc_dai *dai);
    int (*resume)(struct snd_soc_dai *dai);
    /* compress dai */
    int (*compress_new)(struct snd_soc_pcm_runtime *rtd, int num);
    /* DAI is also used for the control bus */
    bool bus_control;

    /* ops */
    const struct snd_soc_dai_ops *ops;

    /* DAI capabilities */
    struct snd_soc_pcm_stream capture;
    struct snd_soc_pcm_stream playback;
    unsigned int symmetric_rates:1;
    unsigned int symmetric_channels:1;
    unsigned int symmetric_samplebits:1;

    /* probe ordering - for components with runtime dependencies */
    int probe_order;
    int remove_order;
};
