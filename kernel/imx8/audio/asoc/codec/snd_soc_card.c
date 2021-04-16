
//  @   include/sound/soc.h

/* SoC card */
struct snd_soc_card {
    const char *name;
    const char *long_name;
    const char *driver_name;
    struct device *dev;
    struct snd_card *snd_card; // 对应alsa中的snd_card
    struct module *owner;

    struct mutex mutex;
    struct mutex dapm_mutex;

    bool instantiated;

    int (*probe)(struct snd_soc_card *card);
    int (*late_probe)(struct snd_soc_card *card);
    int (*remove)(struct snd_soc_card *card);

    /* the pre and post PM functions are used to do any PM work before and
     * after the codec and DAI's do any PM work. */
    int (*suspend_pre)(struct snd_soc_card *card);
    int (*suspend_post)(struct snd_soc_card *card);
    int (*resume_pre)(struct snd_soc_card *card);
    int (*resume_post)(struct snd_soc_card *card);

    /* callbacks */
    int (*set_bias_level)(struct snd_soc_card *,
                  struct snd_soc_dapm_context *dapm,
                  enum snd_soc_bias_level level);
    int (*set_bias_level_post)(struct snd_soc_card *,
                   struct snd_soc_dapm_context *dapm,
                   enum snd_soc_bias_level level);

    int (*add_dai_link)(struct snd_soc_card *,
                struct snd_soc_dai_link *link);
    void (*remove_dai_link)(struct snd_soc_card *,
                struct snd_soc_dai_link *link);

    long pmdown_time;

    /* CPU <--> Codec DAI links  */
    struct snd_soc_dai_link *dai_link;  /* predefined links only */
    int num_links;  /* predefined links only */
    struct list_head dai_link_list; /* all links */
    int num_dai_links;

    struct list_head rtd_list;
    int num_rtd;

    /* optional codec specific configuration */
    struct snd_soc_codec_conf *codec_conf;
    int num_configs;

    /*
     * optional auxiliary devices such as amplifiers or codecs with DAI
     * link unused
     */
    struct snd_soc_aux_dev *aux_dev;
    int num_aux_devs;
    struct list_head aux_comp_list;

    const struct snd_kcontrol_new *controls;
    int num_controls;

    /*
     * Card-specific routes and widgets.
     * Note: of_dapm_xxx for Device Tree; Otherwise for driver build-in.
     */
    const struct snd_soc_dapm_widget *dapm_widgets;
    int num_dapm_widgets;
    const struct snd_soc_dapm_route *dapm_routes;
    int num_dapm_routes;
    const struct snd_soc_dapm_widget *of_dapm_widgets;
    int num_of_dapm_widgets;
    const struct snd_soc_dapm_route *of_dapm_routes;
    int num_of_dapm_routes;
    bool fully_routed;

    struct work_struct deferred_resume_work;

    /* lists of probed devices belonging to this card */
    struct list_head codec_dev_list;

    struct list_head widgets;
    struct list_head paths;
    struct list_head dapm_list;
    struct list_head dapm_dirty;

    /* attached dynamic objects */
    struct list_head dobj_list;

    /* Generic DAPM context for the card */
    struct snd_soc_dapm_context dapm;
    struct snd_soc_dapm_stats dapm_stats;
    struct snd_soc_dapm_update *update;

#ifdef CONFIG_DEBUG_FS
    struct dentry *debugfs_card_root;
    struct dentry *debugfs_pop_time;
#endif
    u32 pop_time;

    void *drvdata;
};



int snd_soc_register_card(struct snd_soc_card *card)
--> static int snd_soc_instantiate_card(struct snd_soc_card *card)
----> int snd_card_new(struct device *parent, int idx, const char *xid,struct module *module, int extra_size,struct snd_card **card_ret)
----> int snd_card_register(struct snd_card *card) 