//  @   sound/soc/codecs/tlv320aic3x.c
#if defined(CONFIG_OF)
static const struct of_device_id tlv320aic3x_of_match[] = {
    { .compatible = "ti,tlv320aic3x", },
    { .compatible = "ti,tlv320aic33" },
    { .compatible = "ti,tlv320aic3007" },
    { .compatible = "ti,tlv320aic3106" },
    { .compatible = "ti,tlv320aic3104" },                                                                                                                                                                      
    {},  
};
MODULE_DEVICE_TABLE(of, tlv320aic3x_of_match);
#endif

/* machine i2c codec control layer */
static struct i2c_driver aic3x_i2c_driver = {
    .driver = {
        .name = "tlv320aic3x-codec",
        .of_match_table = of_match_ptr(tlv320aic3x_of_match),
    },   
    .probe  = aic3x_i2c_probe,
    .remove = aic3x_i2c_remove,
    .id_table = aic3x_i2c_id,
};

module_i2c_driver(aic3x_i2c_driver);

MODULE_DESCRIPTION("ASoC TLV320AIC3X codec driver");
MODULE_AUTHOR("Vladimir Barinov");
MODULE_LICENSE("GPL");


/**
 * module_i2c_driver() - Helper macro for registering a modular I2C driver
 * @__i2c_driver: i2c_driver struct
 *
 * Helper macro for I2C drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
#define module_i2c_driver(__i2c_driver) \
    module_driver(__i2c_driver, i2c_add_driver, i2c_del_driver)


/**
 * module_driver() - Helper macro for drivers that don't do anything
 * special in module init/exit. This eliminates a lot of boilerplate.
 * Each module may only use this macro once, and calling it replaces
 * module_init() and module_exit().
 *
 * @__driver: driver name
 * @__register: register function for this driver type
 * @__unregister: unregister function for this driver type
 * @...: Additional arguments to be passed to __register and __unregister.
 *
 * Use this macro to construct bus specific macros for registering
 * drivers, and do not use it on its own.
 */
#define module_driver(__driver, __register, __unregister, ...) \                                                                                                                                               
static int __init __driver##_init(void) \                                    // aic3x_i2c_driver_init(void)
{ \
    return __register(&(__driver) , ##__VA_ARGS__); \                        // i2c_add_driver(aic3x_i2c_driver)
} \
module_init(__driver##_init); \                                             // module_init(aic3x_i2c_driver_init)
static void __exit __driver##_exit(void) \                                  //  aic3x_i2c_driver_exit(void)
{ \
    __unregister(&(__driver) , ##__VA_ARGS__); \                            // i2c_del_driver(aic3x_i2c_driver)
} \
module_exit(__driver##_exit);                                               // module_exit(aic3x_i2c_driver_exit);




/*
 * If the i2c layer weren't so broken, we could pass this kind of data
 * around
 */
static int aic3x_i2c_probe(struct i2c_client *i2c,const struct i2c_device_id *id)
{
    struct aic3x_pdata *pdata = i2c->dev.platform_data;
    struct aic3x_priv *aic3x;
    struct aic3x_setup_data *ai3x_setup;
    struct device_node *np = i2c->dev.of_node;
    int ret, i;
    u32 value;

    aic3x = devm_kzalloc(&i2c->dev, sizeof(struct aic3x_priv), GFP_KERNEL);
    if (!aic3x)
        return -ENOMEM;

    aic3x->regmap = devm_regmap_init_i2c(i2c, &aic3x_regmap);
    if (IS_ERR(aic3x->regmap)) {
        ret = PTR_ERR(aic3x->regmap);
        return ret;
    }

    regcache_cache_only(aic3x->regmap, true);

    i2c_set_clientdata(i2c, aic3x);
    if (pdata) {
        aic3x->gpio_reset = pdata->gpio_reset;
        aic3x->setup = pdata->setup;
        aic3x->micbias_vg = pdata->micbias_vg;
    } else if (np) {
        ai3x_setup = devm_kzalloc(&i2c->dev, sizeof(*ai3x_setup),GFP_KERNEL);
        if (!ai3x_setup)
            return -ENOMEM;

        ret = of_get_named_gpio(np, "gpio-reset", 0);
        if (ret >= 0)
            aic3x->gpio_reset = ret;
        else
            aic3x->gpio_reset = -1;

        if (of_property_read_u32_array(np, "ai3x-gpio-func",ai3x_setup->gpio_func, 2) >= 0) {
            aic3x->setup = ai3x_setup;
        }

        if (!of_property_read_u32(np, "ai3x-micbias-vg", &value)) {
            switch (value) {
            case 1 :
                aic3x->micbias_vg = AIC3X_MICBIAS_2_0V;               
                break;
            case 2 :
                aic3x->micbias_vg = AIC3X_MICBIAS_2_5V;
                break;
            case 3 :
                aic3x->micbias_vg = AIC3X_MICBIAS_AVDDV;
                break;
            default :
                aic3x->micbias_vg = AIC3X_MICBIAS_OFF;
                dev_err(&i2c->dev, "Unsuitable MicBias voltage " "found in DT\n");
            }
        } else {
            aic3x->micbias_vg = AIC3X_MICBIAS_OFF;
        }

    } else {
        aic3x->gpio_reset = -1;
    }

    aic3x->model = id->driver_data;

    if (gpio_is_valid(aic3x->gpio_reset) && !aic3x_is_shared_reset(aic3x)) {
        ret = gpio_request(aic3x->gpio_reset, "tlv320aic3x reset");
        if (ret != 0)
            goto err;
        gpio_direction_output(aic3x->gpio_reset, 0);
    }

    for (i = 0; i < ARRAY_SIZE(aic3x->supplies); i++)
        aic3x->supplies[i].supply = aic3x_supply_names[i];

    ret = devm_regulator_bulk_get(&i2c->dev, ARRAY_SIZE(aic3x->supplies), ic3x->supplies);
    if (ret != 0) {
        dev_err(&i2c->dev, "Failed to request supplies: %d\n", ret);
        goto err_gpio;
    }

    aic3x_configure_ocmv(i2c);

    if (aic3x->model == AIC3X_MODEL_3007) {
        ret = regmap_register_patch(aic3x->regmap, aic3007_class_d, ARRAY_SIZE(aic3007_class_d));
        if (ret != 0)
            dev_err(&i2c->dev, "Failed to init class D: %d\n",ret);
    }

    ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_aic3x, &aic3x_dai, 1);                      
    if (ret != 0)
        goto err_gpio;

    list_add(&aic3x->list, &reset_list);

    return 0;

err_gpio:
    if (gpio_is_valid(aic3x->gpio_reset) && !aic3x_is_shared_reset(aic3x))
        gpio_free(aic3x->gpio_reset);
err:
    return ret;
}
