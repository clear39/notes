
//  @   include/sound/soc-dai.h

struct snd_soc_dai_ops {
    /*
     * DAI clocking configuration, all optional.
     * Called by soc_card drivers, normally in their hw_params.
     */
    int (*set_sysclk)(struct snd_soc_dai *dai,
        int clk_id, unsigned int freq, int dir);
    int (*set_pll)(struct snd_soc_dai *dai, int pll_id, int source,
        unsigned int freq_in, unsigned int freq_out);
    int (*set_clkdiv)(struct snd_soc_dai *dai, int div_id, int div);
    int (*set_bclk_ratio)(struct snd_soc_dai *dai, unsigned int ratio);

    /*
     * DAI format configuration
     * Called by soc_card drivers, normally in their hw_params.
     */
    int (*set_fmt)(struct snd_soc_dai *dai, unsigned int fmt);
    int (*xlate_tdm_slot_mask)(unsigned int slots,
        unsigned int *tx_mask, unsigned int *rx_mask);
    int (*set_tdm_slot)(struct snd_soc_dai *dai,
        unsigned int tx_mask, unsigned int rx_mask,
        int slots, int slot_width);
    int (*set_channel_map)(struct snd_soc_dai *dai,
        unsigned int tx_num, unsigned int *tx_slot,
        unsigned int rx_num, unsigned int *rx_slot);
    int (*set_tristate)(struct snd_soc_dai *dai, int tristate);

    /*
     * DAI digital mute - optional.
     * Called by soc-core to minimise any pops.
     */
    int (*digital_mute)(struct snd_soc_dai *dai, int mute);
    int (*mute_stream)(struct snd_soc_dai *dai, int mute, int stream);

    /*
     * ALSA PCM audio operations - all optional.
     * Called by soc-core during audio PCM operations.
     */
    int (*startup)(struct snd_pcm_substream *,
        struct snd_soc_dai *);
    void (*shutdown)(struct snd_pcm_substream *,
        struct snd_soc_dai *);
    int (*hw_params)(struct snd_pcm_substream *,
        struct snd_pcm_hw_params *, struct snd_soc_dai *);
    int (*hw_free)(struct snd_pcm_substream *,
        struct snd_soc_dai *);
    int (*prepare)(struct snd_pcm_substream *,
        struct snd_soc_dai *);
    /*
     * NOTE: Commands passed to the trigger function are not necessarily
     * compatible with the current state of the dai. For example this
     * sequence of commands is possible: START STOP STOP.
     * So do not unconditionally use refcounting functions in the trigger
     * function, e.g. clk_enable/disable.
     */
    int (*trigger)(struct snd_pcm_substream *, int,
        struct snd_soc_dai *);
    int (*bespoke_trigger)(struct snd_pcm_substream *, int,
        struct snd_soc_dai *);
    /*
     * For hardware based FIFO caused delay reporting.
     * Optional.
     */
    snd_pcm_sframes_t (*delay)(struct snd_pcm_substream *,
        struct snd_soc_dai *);
};