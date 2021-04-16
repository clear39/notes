

//  @   include/sound/soc.h

/* codec driver */
struct snd_soc_codec_driver {

    /* driver ops */
    int (*probe)(struct snd_soc_codec *);
    int (*remove)(struct snd_soc_codec *);
    int (*suspend)(struct snd_soc_codec *);
    int (*resume)(struct snd_soc_codec *);
    struct snd_soc_component_driver component_driver;

    /* codec wide operations */
    int (*set_sysclk)(struct snd_soc_codec *codec,
              int clk_id, int source, unsigned int freq, int dir);
    int (*set_pll)(struct snd_soc_codec *codec, int pll_id, int source,
        unsigned int freq_in, unsigned int freq_out);

    /* codec IO */
    struct regmap *(*get_regmap)(struct device *);
    unsigned int (*read)(struct snd_soc_codec *, unsigned int);
    int (*write)(struct snd_soc_codec *, unsigned int, unsigned int);
    unsigned int reg_cache_size;
    short reg_cache_step;
    short reg_word_size;
    const void *reg_cache_default;

    /* codec bias level */
    int (*set_bias_level)(struct snd_soc_codec *, enum snd_soc_bias_level level);
    bool idle_bias_off;
    bool suspend_bias_off;

    void (*seq_notifier)(struct snd_soc_dapm_context *, enum snd_soc_dapm_type, int);

    bool ignore_pmdown_time;  /* Doesn't benefit from pmdown delay */
};