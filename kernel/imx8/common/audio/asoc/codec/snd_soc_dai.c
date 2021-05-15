
//  @   include/sound/soc-dai.h
/*
 * Digital Audio Interface runtime data.
 *
 * Holds runtime data for a DAI.
 */
struct snd_soc_dai {
    const char *name;
    int id;
    struct device *dev;

    /* driver ops */
    struct snd_soc_dai_driver *driver;

    /* DAI runtime info */
    unsigned int capture_active:1;      /* stream is in use */
    unsigned int playback_active:1;     /* stream is in use */
    unsigned int symmetric_rates:1;
    unsigned int symmetric_channels:1;
    unsigned int symmetric_samplebits:1;
    unsigned int active;
    unsigned char probed:1;

    struct snd_soc_dapm_widget *playback_widget;
    struct snd_soc_dapm_widget *capture_widget;

    /* DAI DMA data */
    void *playback_dma_data;
    void *capture_dma_data;

    /* Symmetry data - only valid if symmetry is being enforced */
    unsigned int rate;
    unsigned int channels;
    unsigned int sample_bits;

    /* parent platform/codec */
    struct snd_soc_codec *codec;
    struct snd_soc_component *component;

    /* CODEC TDM slot masks and params (for fixup) */
    unsigned int tx_mask;
    unsigned int rx_mask;

    struct list_head list;
};
