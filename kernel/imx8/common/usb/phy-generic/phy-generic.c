//  @   drivers/usb/phy/phy-generic.c
static struct platform_driver usb_phy_generic_driver = { 
    .probe      = usb_phy_generic_probe,
    .remove     = usb_phy_generic_remove,
    .driver     = { 
        .name   = "usb_phy_generic",
        .of_match_table = nop_xceiv_dt_ids,
    },  
};

static int __init usb_phy_generic_init(void)                                                                                                                                                                   
{
    pr_info("***%s@@@ \n",__func__);
    return platform_driver_register(&usb_phy_generic_driver);
}
subsys_initcall(usb_phy_generic_init);

static const struct of_device_id nop_xceiv_dt_ids[] = {                                                                                                                                                        
    { .compatible = "usb-nop-xceiv" },
    { } 
};



struct usb_phy_generic {                                                                                                                                                                                       
    struct usb_phy phy;
    struct device *dev;
    struct clk *clk;
    struct regulator *vcc;
    struct gpio_desc *gpiod_reset;
    struct gpio_desc *gpiod_vbus;
    struct regulator *vbus_draw;
    bool vbus_draw_enabled;
    unsigned long mA;
    unsigned int vbus;
};

static int usb_phy_generic_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct usb_phy_generic  *nop;
    int err;

    dev_dbg(&pdev->dev,"***%s@@@ \n",__func__);

    nop = devm_kzalloc(dev, sizeof(*nop), GFP_KERNEL);
    if (!nop)
        return -ENOMEM;

    err = usb_phy_gen_create_phy(dev, nop, dev_get_platdata(&pdev->dev));
    if (err)
        return err;
    if (nop->gpiod_vbus) {
        err = devm_request_threaded_irq(&pdev->dev,
                        gpiod_to_irq(nop->gpiod_vbus),
                        NULL, nop_gpio_vbus_thread,
                        VBUS_IRQ_FLAGS, "vbus_detect",
                        nop);
        if (err) {
            dev_err(&pdev->dev, "can't request irq %i, err: %d\n", gpiod_to_irq(nop->gpiod_vbus), err);
            return err;
        }
        nop->phy.otg->state = gpiod_get_value(nop->gpiod_vbus) ? OTG_STATE_B_PERIPHERAL : OTG_STATE_B_IDLE;
    }

    nop->phy.init       = usb_gen_phy_init;
    nop->phy.shutdown   = usb_gen_phy_shutdown;

    err = usb_add_phy_dev(&nop->phy);
    if (err) {
        dev_err(&pdev->dev, "can't register transceiver, err: %d\n",
            err);
        return err;
    }

    platform_set_drvdata(pdev, nop);

    return 0;
}


int usb_phy_gen_create_phy(struct device *dev, struct usb_phy_generic *nop, struct usb_phy_generic_platform_data *pdata)
{
    enum usb_phy_type type = USB_PHY_TYPE_USB2;
    int err = 0;

    u32 clk_rate = 0;
    bool needs_vcc = false;

    if (dev->of_node) {
        struct device_node *node = dev->of_node;

        if (of_property_read_u32(node, "clock-frequency", &clk_rate))
            clk_rate = 0;

        needs_vcc = of_property_read_bool(node, "vcc-supply");
        nop->gpiod_reset = devm_gpiod_get_optional(dev, "reset", GPIOD_ASIS);
        err = PTR_ERR_OR_ZERO(nop->gpiod_reset);
        if (!err) {
            nop->gpiod_vbus = devm_gpiod_get_optional(dev, "vbus-detect", GPIOD_ASIS);
            err = PTR_ERR_OR_ZERO(nop->gpiod_vbus);
        }
    } else if (pdata) {
        type = pdata->type;
        clk_rate = pdata->clk_rate;
        needs_vcc = pdata->needs_vcc;
        if (gpio_is_valid(pdata->gpio_reset)) {
            err = devm_gpio_request_one(dev, pdata->gpio_reset, GPIOF_ACTIVE_LOW, dev_name(dev));
            if (!err)
                nop->gpiod_reset = gpio_to_desc(pdata->gpio_reset);
        }
        nop->gpiod_vbus = pdata->gpiod_vbus;
    }

    if (err == -EPROBE_DEFER)
        return -EPROBE_DEFER;
    if (err) {
        dev_err(dev, "Error requesting RESET or VBUS GPIO\n");
        return err;
    }
    if (nop->gpiod_reset)
        gpiod_direction_output(nop->gpiod_reset, 1);

    nop->phy.otg = devm_kzalloc(dev, sizeof(*nop->phy.otg), GFP_KERNEL);
    if (!nop->phy.otg)
        return -ENOMEM;

    nop->clk = devm_clk_get(dev, "main_clk");
    if (IS_ERR(nop->clk)) {
        dev_dbg(dev, "Can't get phy clock: %ld\n", PTR_ERR(nop->clk));
    }

    if (!IS_ERR(nop->clk) && clk_rate) {
        err = clk_set_rate(nop->clk, clk_rate);
        if (err) {
            dev_err(dev, "Error setting clock rate\n");
            return err;
        }
    }

    nop->vcc = devm_regulator_get(dev, "vcc");
    if (IS_ERR(nop->vcc)) {
        dev_dbg(dev, "Error getting vcc regulator: %ld\n", PTR_ERR(nop->vcc));
        if (needs_vcc)
            return -EPROBE_DEFER;
    }

    nop->dev        = dev;
    nop->phy.dev        = nop->dev;
    nop->phy.label      = "nop-xceiv";
    nop->phy.set_suspend    = nop_set_suspend;
    nop->phy.type       = type;

    nop->phy.otg->state     = OTG_STATE_UNDEFINED;
    nop->phy.otg->usb_phy       = &nop->phy;
    nop->phy.otg->set_host      = nop_set_host;
    nop->phy.otg->set_peripheral    = nop_set_peripheral;

    return 0;
}
EXPORT_SYMBOL_GPL(usb_phy_gen_create_phy);
                                                    