
//  @   include/sound/soc.h

/* component interface */
struct snd_soc_component_driver {
    const char *name;

    /* Default control and setup, added after probe() is run */
    const struct snd_kcontrol_new *controls;
    unsigned int num_controls;
    const struct snd_soc_dapm_widget *dapm_widgets;
    unsigned int num_dapm_widgets;
    const struct snd_soc_dapm_route *dapm_routes;
    unsigned int num_dapm_routes;

    int (*probe)(struct snd_soc_component *);
    void (*remove)(struct snd_soc_component *);

    /* DT */
    int (*of_xlate_dai_name)(struct snd_soc_component *component,
                 struct of_phandle_args *args,
                 const char **dai_name);
    void (*seq_notifier)(struct snd_soc_component *, enum snd_soc_dapm_type,
        int subseq);
    int (*stream_event)(struct snd_soc_component *, int event);

    /* probe ordering - for components with runtime dependencies */
    int probe_order;
    int remove_order;
};
