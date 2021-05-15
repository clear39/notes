//  @   sound/soc/codecs/wm8994.c

static const struct dev_pm_ops wm8994_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(wm8994_suspend, wm8994_resume)
};

static struct platform_driver wm8994_codec_driver = {
    .driver = {
        .name = "wm8994-codec",
        .pm = &wm8994_pm_ops,
    },   
    .probe = wm8994_probe,
    .remove = wm8994_remove,
};

module_platform_driver(wm8994_codec_driver);

MODULE_DESCRIPTION("ASoC WM8994 driver");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:wm8994-codec"); 




static int wm8994_probe(struct platform_device *pdev)                                                                                                                                                          
{
    struct wm8994_priv *wm8994;

    wm8994 = devm_kzalloc(&pdev->dev, sizeof(struct wm8994_priv),GFP_KERNEL);
    if (wm8994 == NULL)
        return -ENOMEM;

    platform_set_drvdata(pdev, wm8994);

    mutex_init(&wm8994->fw_lock);

    wm8994->wm8994 = dev_get_drvdata(pdev->dev.parent);

    pm_runtime_enable(&pdev->dev);
    pm_runtime_idle(&pdev->dev);

    return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_wm8994, wm8994_dai, ARRAY_SIZE(wm8994_dai));
}

static const struct snd_soc_codec_driver soc_codec_dev_wm8994 = {
    .probe =    wm8994_codec_probe,
    .remove =   wm8994_codec_remove,
    .suspend =  wm8994_codec_suspend,
    .resume =   wm8994_codec_resume,
    .get_regmap =   wm8994_get_regmap,
    .set_bias_level = wm8994_set_bias_level,
};


static struct snd_soc_dai_driver wm8994_dai[] = {
    {
        .name = "wm8994-aif1",
        .id = 1,
        .playback = {
            .stream_name = "AIF1 Playback",
            .channels_min = 1,
            .channels_max = 2,
            .rates = WM8994_RATES,
            .formats = WM8994_FORMATS,
            .sig_bits = 24,
        },
        .capture = {
            .stream_name = "AIF1 Capture",
            .channels_min = 1,
            .channels_max = 2,
            .rates = WM8994_RATES,
            .formats = WM8994_FORMATS,
            .sig_bits = 24,
         },
        .ops = &wm8994_aif1_dai_ops,
    },
    {
        .name = "wm8994-aif2",
        .id = 2,
        .playback = {
            .stream_name = "AIF2 Playback",
            .channels_min = 1,
            .channels_max = 2,
            .rates = WM8994_RATES,
            .formats = WM8994_FORMATS,
            .sig_bits = 24,
        },
        .capture = {
            .stream_name = "AIF2 Capture",
            .channels_min = 1,
            .channels_max = 2,
            .rates = WM8994_RATES,
            .formats = WM8994_FORMATS,
            .sig_bits = 24,
        },
        .probe = wm8994_aif2_probe,
        .ops = &wm8994_aif2_dai_ops,
    },
    {
        .name = "wm8994-aif3",
        .id = 3,
        .playback = {
            .stream_name = "AIF3 Playback",
            .channels_min = 1,
            .channels_max = 2,
            .rates = WM8994_RATES,
            .formats = WM8994_FORMATS,
            .sig_bits = 24,
        },
        .capture = {
            .stream_name = "AIF3 Capture",
            .channels_min = 1,
            .channels_max = 2,
            .rates = WM8994_RATES,
            .formats = WM8994_FORMATS,
            .sig_bits = 24,
         },
        .ops = &wm8994_aif3_dai_ops,
    }
};

static const struct snd_soc_dai_ops wm8994_aif1_dai_ops = {
    .set_sysclk = wm8994_set_dai_sysclk,
    .set_fmt    = wm8994_set_dai_fmt,
    .hw_params  = wm8994_hw_params,
    .digital_mute   = wm8994_aif_mute,
    .set_pll    = wm8994_set_fll,
    .set_tristate   = wm8994_set_tristate,
};

static const struct snd_soc_dai_ops wm8994_aif2_dai_ops = {
    .set_sysclk = wm8994_set_dai_sysclk,
    .set_fmt    = wm8994_set_dai_fmt,
    .hw_params  = wm8994_hw_params,
    .digital_mute   = wm8994_aif_mute,
    .set_pll    = wm8994_set_fll,
    .set_tristate   = wm8994_set_tristate,
};

static const struct snd_soc_dai_ops wm8994_aif3_dai_ops = {
    .hw_params  = wm8994_aif3_hw_params,
};

/**                                                                                                                                                                                                            
 * snd_soc_register_codec - Register a codec with the ASoC core
 *
 * @dev: The parent device for this codec
 * @codec_drv: Codec driver
 * @dai_drv: The associated DAI driver
 * @num_dai: Number of DAIs
 */
int snd_soc_register_codec(struct device *dev,
               const struct snd_soc_codec_driver *codec_drv,
               struct snd_soc_dai_driver *dai_drv,
               int num_dai)
{
    struct snd_soc_dapm_context *dapm;
    struct snd_soc_codec *codec;
    struct snd_soc_dai *dai;
    int ret, i;

    dev_dbg(dev, "codec register %s\n", dev_name(dev));

    codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
    if (codec == NULL)
        return -ENOMEM;

    codec->component.codec = codec;

    ret = snd_soc_component_initialize(&codec->component,&codec_drv->component_driver, dev);
    if (ret)
        goto err_free;

    if (codec_drv->probe)
        codec->component.probe = snd_soc_codec_drv_probe;

    if (codec_drv->remove)
        codec->component.remove = snd_soc_codec_drv_remove;

    if (codec_drv->write)
        codec->component.write = snd_soc_codec_drv_write;
        
    if (codec_drv->read)
        codec->component.read = snd_soc_codec_drv_read;

    codec->component.ignore_pmdown_time = codec_drv->ignore_pmdown_time;

    dapm = snd_soc_codec_get_dapm(codec);
    dapm->idle_bias_off = codec_drv->idle_bias_off;
    dapm->suspend_bias_off = codec_drv->suspend_bias_off;
    if (codec_drv->seq_notifier)
        dapm->seq_notifier = codec_drv->seq_notifier;
    if (codec_drv->set_bias_level)
        dapm->set_bias_level = snd_soc_codec_set_bias_level;
    codec->dev = dev;
    codec->driver = codec_drv;
    codec->component.val_bytes = codec_drv->reg_word_size;

#ifdef CONFIG_DEBUG_FS
    codec->component.init_debugfs = soc_init_codec_debugfs;
    codec->component.debugfs_prefix = "codec";
#endif

    if (codec_drv->get_regmap)
        codec->component.regmap = codec_drv->get_regmap(dev);

    for (i = 0; i < num_dai; i++) {
        fixup_codec_formats(&dai_drv[i].playback);
        fixup_codec_formats(&dai_drv[i].capture);
    }

    ret = snd_soc_register_dais(&codec->component, dai_drv, num_dai, false);
    if (ret < 0) {
        dev_err(dev, "ASoC: Failed to register DAIs: %d\n", ret);
        goto err_cleanup;
    }

    list_for_each_entry(dai, &codec->component.dai_list, list)
        dai->codec = codec;

    mutex_lock(&client_mutex);
    snd_soc_component_add_unlocked(&codec->component);
    list_add(&codec->list, &codec_list);
    mutex_unlock(&client_mutex);

    dev_dbg(codec->dev, "ASoC: Registered codec '%s'\n",codec->component.name);
    return 0;

err_cleanup:
    snd_soc_component_cleanup(&codec->component);
err_free:
    kfree(codec);
    return ret;
}
EXPORT_SYMBOL_GPL(snd_soc_register_codec);




















