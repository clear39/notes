
//  @   sound/soc/samsung/smdk_wm8994.c

static struct platform_driver smdk_audio_driver = { 
    .driver     = { 
        .name   = "smdk-audio-wm8994",
        .of_match_table = of_match_ptr(samsung_wm8994_of_match),
        .pm = &snd_soc_pm_ops,
    },  
    .probe      = smdk_audio_probe,
};

module_platform_driver(smdk_audio_driver);

MODULE_DESCRIPTION("ALSA SoC SMDK WM8994");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:smdk-audio-wm8994"); 


static const struct of_device_id samsung_wm8994_of_match[] = {                                                                                                                                                 
    { .compatible = "samsung,smdk-wm8994", .data = &smdk_board_data },
    {}, 
};


//  @   sound/soc/soc-core.c
const struct dev_pm_ops snd_soc_pm_ops = {                                                                                                                                                                     
    .suspend = snd_soc_suspend,
    .resume = snd_soc_resume,
    .freeze = snd_soc_suspend,
    .thaw = snd_soc_resume,
    .poweroff = snd_soc_poweroff,
    .restore = snd_soc_resume,
};
EXPORT_SYMBOL_GPL(snd_soc_pm_ops);

static struct snd_soc_card smdk = {
    .name = "SMDK-I2S",
    .owner = THIS_MODULE,
    .dai_link = smdk_dai,
    .num_links = ARRAY_SIZE(smdk_dai),                                                                                                                                                                         
};

static struct snd_soc_dai_link smdk_dai[] = {
    { /* Primary DAI i/f */
        .name = "WM8994 AIF1",
        .stream_name = "Pri_Dai",
        .cpu_dai_name = "samsung-i2s.0",
        .codec_dai_name = "wm8994-aif1",
        .platform_name = "samsung-i2s.0",
        .codec_name = "wm8994-codec",
        .init = smdk_wm8994_init_paiftx,
        .dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM,
        .ops = &smdk_ops,
    }, { /* Sec_Fifo Playback i/f */
        .name = "Sec_FIFO TX",
        .stream_name = "Sec_Dai",
        .cpu_dai_name = "samsung-i2s-sec",
        .codec_dai_name = "wm8994-aif1",
        .platform_name = "samsung-i2s-sec",
        .codec_name = "wm8994-codec",
        .dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM,
        .ops = &smdk_ops,
    },
};

/*                                                                                                                                                                                                             
 * SMDK WM8994 DAI operations.
 */
static struct snd_soc_ops smdk_ops = {
    .hw_params = smdk_hw_params,
};
   

static int smdk_audio_probe(struct platform_device *pdev)
{
    int ret;
    struct device_node *np = pdev->dev.of_node;
    struct snd_soc_card *card = &smdk;
    /**
    struct smdk_wm8994_data {                                                                                                                                                                                      
        int mclk1_rate;
    };
    */
    struct smdk_wm8994_data *board;
    const struct of_device_id *id;

    card->dev = &pdev->dev;

    board = devm_kzalloc(&pdev->dev, sizeof(*board), GFP_KERNEL);
    if (!board)
        return -ENOMEM;

    if (np) {
        smdk_dai[0].cpu_dai_name = NULL;
        smdk_dai[0].cpu_of_node = of_parse_phandle(np, "samsung,i2s-controller", 0); 
        if (!smdk_dai[0].cpu_of_node) {
            dev_err(&pdev->dev, "Property 'samsung,i2s-controller' missing or invalid\n");
            ret = -EINVAL;
        }

        smdk_dai[0].platform_name = NULL;
        smdk_dai[0].platform_of_node = smdk_dai[0].cpu_of_node;
    }

    id = of_match_device(of_match_ptr(samsung_wm8994_of_match), &pdev->dev);
    if (id)
        *board = *((struct smdk_wm8994_data *)id->data);

    platform_set_drvdata(pdev, board);

    ret = devm_snd_soc_register_card(&pdev->dev, card);

    if (ret)
        dev_err(&pdev->dev, "snd_soc_register_card() failed:%d\n", ret);

    return ret;
}


/** 
 * devm_snd_soc_register_card - resource managed card registration
 * @dev: Device used to manage card
 * @card: Card to register
 *  
 * Register a card with automatic unregistration when the device is
 * unregistered.
 */ 
int devm_snd_soc_register_card(struct device *dev, struct snd_soc_card *card)                                                                                                                                  
{
    struct snd_soc_card **ptr;
    int ret;
    /**
     * 
    */
    ptr = devres_alloc(devm_card_release, sizeof(*ptr), GFP_KERNEL);
    if (!ptr)
        return -ENOMEM;
    
    /**
     * 
    */
    ret = snd_soc_register_card(card);
    if (ret == 0) {
        *ptr = card;
        devres_add(dev, ptr);
    } else {
        devres_free(ptr);
    }
    
    return ret;
}       
EXPORT_SYMBOL_GPL(devm_snd_soc_register_card);


/**
 * snd_soc_register_card - Register a card with the ASoC core
 *
 * @card: Card to register
 *
 */
int snd_soc_register_card(struct snd_soc_card *card)
{
    int i, ret;
    struct snd_soc_pcm_runtime *rtd;

    if (!card->name || !card->dev)
        return -EINVAL;

    for (i = 0; i < card->num_links; i++) {
        struct snd_soc_dai_link *link = &card->dai_link[i];

        ret = soc_init_dai_link(card, link);
        if (ret) {
            dev_err(card->dev, "ASoC: failed to init link %s\n",
                link->name);
            return ret;
        }
    }

    dev_set_drvdata(card->dev, card);

    snd_soc_initialize_card_lists(card);

    INIT_LIST_HEAD(&card->dai_link_list);
    card->num_dai_links = 0;

    INIT_LIST_HEAD(&card->rtd_list);
    card->num_rtd = 0;

    INIT_LIST_HEAD(&card->dapm_dirty);
    INIT_LIST_HEAD(&card->dobj_list);
    card->instantiated = 0;
    mutex_init(&card->mutex);
    mutex_init(&card->dapm_mutex);

    ret = snd_soc_instantiate_card(card);
    if (ret != 0)
        return ret;

    /* deactivate pins to sleep state */
    list_for_each_entry(rtd, &card->rtd_list, list)  {
        struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
        int j;

        for (j = 0; j < rtd->num_codecs; j++) {
            struct snd_soc_dai *codec_dai = rtd->codec_dais[j];
            if (!codec_dai->active)
                pinctrl_pm_select_sleep_state(codec_dai->dev);
        }

        if (!cpu_dai->active)
            pinctrl_pm_select_sleep_state(cpu_dai->dev);
    }

    return ret;
}
EXPORT_SYMBOL_GPL(snd_soc_register_card);


static int snd_soc_instantiate_card(struct snd_soc_card *card)
{
    struct snd_soc_codec *codec;
    struct snd_soc_pcm_runtime *rtd;
    struct snd_soc_dai_link *dai_link;
    int ret, i, order;

    mutex_lock(&client_mutex);
    mutex_lock_nested(&card->mutex, SND_SOC_CARD_CLASS_INIT);

    /* bind DAIs */
    for (i = 0; i < card->num_links; i++) {
        ret = soc_bind_dai_link(card, &card->dai_link[i]);
        if (ret != 0)
            goto base_error;
    }

    /* bind aux_devs too */
    for (i = 0; i < card->num_aux_devs; i++) {
        ret = soc_bind_aux_dev(card, i);
        if (ret != 0)
            goto base_error;
    }

    /* add predefined DAI links to the list */
    for (i = 0; i < card->num_links; i++)
        snd_soc_add_dai_link(card, card->dai_link+i);

    /* initialize the register cache for each available codec */
    list_for_each_entry(codec, &codec_list, list) {
        if (codec->cache_init)
            continue;
        ret = snd_soc_init_codec_cache(codec);
        if (ret < 0)
            goto base_error;
    }

    /* card bind complete so register a sound card */
    ret = snd_card_new(card->dev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,card->owner, 0, &card->snd_card);
    if (ret < 0) {
        dev_err(card->dev,
            "ASoC: can't create sound card for card %s: %d\n",
            card->name, ret);
        goto base_error;
    }

    soc_init_card_debugfs(card);

    card->dapm.bias_level = SND_SOC_BIAS_OFF;
    card->dapm.dev = card->dev;
    card->dapm.card = card;
    list_add(&card->dapm.list, &card->dapm_list);

#ifdef CONFIG_DEBUG_FS
    snd_soc_dapm_debugfs_init(&card->dapm, card->debugfs_card_root);
#endif

#ifdef CONFIG_PM_SLEEP
    /* deferred resume work */
    INIT_WORK(&card->deferred_resume_work, soc_resume_deferred);
#endif

    if (card->dapm_widgets)
        snd_soc_dapm_new_controls(&card->dapm, card->dapm_widgets,card->num_dapm_widgets);

    if (card->of_dapm_widgets)
        snd_soc_dapm_new_controls(&card->dapm, card->of_dapm_widgets,card->num_of_dapm_widgets);

    /* initialise the sound card only once */
    if (card->probe) {
        ret = card->probe(card);
        if (ret < 0)
            goto card_probe_error;
    }

    /* probe all components used by DAI links on this card */
    for (order = SND_SOC_COMP_ORDER_FIRST; order <= SND_SOC_COMP_ORDER_LAST;order++) {
        list_for_each_entry(rtd, &card->rtd_list, list) {
            ret = soc_probe_link_components(card, rtd, order);
            if (ret < 0) {
                dev_err(card->dev,
                    "ASoC: failed to instantiate card %d\n",
                    ret);
                goto probe_dai_err;
            }
        }
    }

    /* probe auxiliary components */
    ret = soc_probe_aux_devices(card);
    if (ret < 0)
        goto probe_dai_err;

    /* Find new DAI links added during probing components and bind them.
     * Components with topology may bring new DAIs and DAI links.
     */
    list_for_each_entry(dai_link, &card->dai_link_list, list) {
        if (soc_is_dai_link_bound(card, dai_link))
            continue;

        ret = soc_init_dai_link(card, dai_link);
        if (ret)
            goto probe_dai_err;
        ret = soc_bind_dai_link(card, dai_link);
        if (ret)
            goto probe_dai_err;
    }

    /* probe all DAI links on this card */
    for (order = SND_SOC_COMP_ORDER_FIRST; order <= SND_SOC_COMP_ORDER_LAST;
            order++) {
        list_for_each_entry(rtd, &card->rtd_list, list) {
            ret = soc_probe_link_dais(card, rtd, order);
            if (ret < 0) {
                dev_err(card->dev,
                    "ASoC: failed to instantiate card %d\n",
                    ret);
                goto probe_dai_err;
            }
        }
    }

    snd_soc_dapm_link_dai_widgets(card);
    snd_soc_dapm_connect_dai_link_widgets(card);

    if (card->controls)
        snd_soc_add_card_controls(card, card->controls, card->num_controls);

    if (card->dapm_routes)
        snd_soc_dapm_add_routes(&card->dapm, card->dapm_routes,card->num_dapm_routes);

    if (card->of_dapm_routes)
        snd_soc_dapm_add_routes(&card->dapm, card->of_dapm_routes,card->num_of_dapm_routes);

    snprintf(card->snd_card->shortname, sizeof(card->snd_card->shortname),"%s", card->name);
    snprintf(card->snd_card->longname, sizeof(card->snd_card->longname),"%s", card->long_name ? card->long_name : card->name);    
    snprintf(card->snd_card->driver, sizeof(card->snd_card->driver),"%s", card->driver_name ? card->driver_name : card->name);
    
    for (i = 0; i < ARRAY_SIZE(card->snd_card->driver); i++) {
        switch (card->snd_card->driver[i]) {
        case '_':
        case '-':
        case '\0':
            break;
        default:
            if (!isalnum(card->snd_card->driver[i]))
                card->snd_card->driver[i] = '_';
            break;
        }
    }

    if (card->late_probe) {
        ret = card->late_probe(card);
        if (ret < 0) {
            dev_err(card->dev, "ASoC: %s late_probe() failed: %d\n",
                card->name, ret);
            goto probe_aux_dev_err;
        }
    }

    snd_soc_dapm_new_widgets(card);

    ret = snd_card_register(card->snd_card);
    if (ret < 0) {
        dev_err(card->dev, "ASoC: failed to register soundcard %d\n",
                ret);
        goto probe_aux_dev_err;
    }

    card->instantiated = 1;
    snd_soc_dapm_sync(&card->dapm);
    mutex_unlock(&card->mutex);
    mutex_unlock(&client_mutex);

    return 0;

probe_aux_dev_err:
    soc_remove_aux_devices(card);

probe_dai_err:
    soc_remove_dai_links(card);

card_probe_error:
    if (card->remove)
        card->remove(card);

    snd_soc_dapm_free(&card->dapm);
    soc_cleanup_card_debugfs(card);
    snd_card_free(card->snd_card);

base_error:
    soc_remove_pcm_runtimes(card);
    mutex_unlock(&card->mutex);
    mutex_unlock(&client_mutex);

    return ret;
}





int devm_snd_soc_register_card(struct device *dev, struct snd_soc_card *card)
--> int snd_soc_register_card(struct snd_soc_card *card)
----> static int snd_soc_instantiate_card(struct snd_soc_card *card)
------> int snd_card_new(struct device *parent, int idx, const char *xid,struct module *module, int extra_size,struct snd_card **card_ret)
------> int snd_card_register(struct snd_card *card) 


