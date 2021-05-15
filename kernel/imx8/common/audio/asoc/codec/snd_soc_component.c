
//  @   include/sound/soc.h

struct snd_soc_component {
    const char *name;
    int id;
    const char *name_prefix;
    struct device *dev;
    struct snd_soc_card *card;

    unsigned int active;

    unsigned int ignore_pmdown_time:1; /* pmdown_time is ignored at stop */
    unsigned int registered_as_component:1;

    struct list_head list;
    struct list_head list_aux; /* for auxiliary component of the card */


    struct snd_soc_dai_driver *dai_drv;
    int num_dai;

    const struct snd_soc_component_driver *driver;

    struct list_head dai_list;

    int (*read)(struct snd_soc_component *, unsigned int, unsigned int *);
    int (*write)(struct snd_soc_component *, unsigned int, unsigned int);

    struct regmap *regmap;
    int val_bytes;

    struct mutex io_mutex;

    /* attached dynamic objects */
    struct list_head dobj_list;

#ifdef CONFIG_DEBUG_FS
    struct dentry *debugfs_root;
#endif

    /*
    * DO NOT use any of the fields below in drivers, they are temporary and
    * are going to be removed again soon. If you use them in driver code the
    * driver will be marked as BROKEN when these fields are removed.
    */

    /* Don't use these, use snd_soc_component_get_dapm() */
    struct snd_soc_dapm_context dapm;

    const struct snd_kcontrol_new *controls;
    unsigned int num_controls;
    const struct snd_soc_dapm_widget *dapm_widgets;
    unsigned int num_dapm_widgets;
    const struct snd_soc_dapm_route *dapm_routes;
    unsigned int num_dapm_routes;
    struct snd_soc_codec *codec;

    int (*probe)(struct snd_soc_component *);
    void (*remove)(struct snd_soc_component *);

    /* machine specific init */
    int (*init)(struct snd_soc_component *component);

#ifdef CONFIG_DEBUG_FS
    void (*init_debugfs)(struct snd_soc_component *component);
    const char *debugfs_prefix;
#endif
};  