


usbotg1: usb@5b0d0000 {
    compatible = "fsl,imx8qm-usb", "fsl,imx27-usb";
    reg = <0x0 0x5b0d0000 0x0 0x200>;
    interrupt-parent = <&wu>;
    interrupts = <GIC_SPI 267 IRQ_TYPE_LEVEL_HIGH>;
    fsl,usbphy = <&usbphy1>;
    fsl,usbmisc = <&usbmisc1 0>;
    clocks = <&clk IMX8QXP_USB2_OH_AHB_CLK>;
    ahb-burst-config = <0x0>;
    tx-burst-size-dword = <0x10>;
    rx-burst-size-dword = <0x10>;
    #stream-id-cells = <1>;
    power-domains = <&pd_conn_usbotg0>;
    status = "disabled";
};

&usbotg1 {
	dr_mode = "peripheral";
	status = "okay";
};


//  @   drivers/usb/chipidea/ci_hdrc_imx.c
static const struct of_device_id ci_hdrc_imx_dt_ids[] = {                                                                                                                                                      
    { .compatible = "fsl,imx23-usb", .data = &imx23_usb_data},
    { .compatible = "fsl,imx28-usb", .data = &imx28_usb_data},
    { .compatible = "fsl,imx27-usb", .data = &imx27_usb_data},
    { .compatible = "fsl,imx6q-usb", .data = &imx6q_usb_data},
    { .compatible = "fsl,imx6sl-usb", .data = &imx6sl_usb_data},
    { .compatible = "fsl,imx6sx-usb", .data = &imx6sx_usb_data},
    { .compatible = "fsl,imx6ul-usb", .data = &imx6ul_usb_data},
    { .compatible = "fsl,imx7d-usb", .data = &imx7d_usb_data},
    { .compatible = "fsl,imx7ulp-usb", .data = &imx7ulp_usb_data},
    { .compatible = "fsl,imx8qm-usb", .data = &imx8qm_usb_data},
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ci_hdrc_imx_dt_ids);

struct ci_hdrc_imx_platform_flag {                                                                                                                                                                             
    unsigned int flags;
    bool runtime_pm;
};


static struct platform_driver ci_hdrc_imx_driver = { 
    .probe = ci_hdrc_imx_probe,
    .remove = ci_hdrc_imx_remove,
    .shutdown = ci_hdrc_imx_shutdown,
    .driver = { 
        .name = "imx_usb",
        .of_match_table = ci_hdrc_imx_dt_ids,
        .pm = &ci_hdrc_imx_pm_ops,
     },  
};

module_platform_driver(ci_hdrc_imx_driver);

MODULE_ALIAS("platform:imx-usb");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CI HDRC i.MX USB binding");
MODULE_AUTHOR("Marek Vasut <marex@denx.de>");
MODULE_AUTHOR("Richard Zhao <richard.zhao@freescale.com>"); 



static int ci_hdrc_imx_probe(struct platform_device *pdev)
{
    struct ci_hdrc_imx_data *data;
    struct ci_hdrc_platform_data pdata = {
        .name       = dev_name(&pdev->dev),
        .capoffset  = DEF_CAPOFFSET,
        .notify_event   = ci_hdrc_imx_notify_event,
    };
    int ret;
    const struct of_device_id *of_id;
    const struct ci_hdrc_imx_platform_flag *imx_platform_flag;
    struct device *dev = &pdev->dev;
    struct device_node *np = dev->of_node;
    struct pinctrl_state *pinctrl_hsic_idle;

    dev_dbg(&pdev->dev,"***%s@@@ \n",__func__);
    pr_info("***%s@@@ \n",__func__);

    // { .compatible = "fsl,imx8qm-usb", .data = &imx8qm_usb_data},
    of_id = of_match_device(ci_hdrc_imx_dt_ids, dev); 
    if (!of_id)
        return -ENODEV;

    //imx8qm_usb_data
    imx_platform_flag = of_id->data; 

    //构建 ci_hdrc_imx_data 
    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    // pdev->dev->driver_data = data(ci_hdrc_imx_data)
    platform_set_drvdata(pdev, data);

    data->data = imx_platform_flag;
    pdata.flags |= imx_platform_flag->flags;
    // 通过dts 获取 fsl,usbmisc = <&usbmisc1 0>; 
    /*
    usbmisc1: usbmisc@5b0d0200 {
		#index-cells = <1>;
		compatible = "fsl,imx7d-usbmisc", "fsl,imx6q-usbmisc";
		reg = <0x0 0x5b0d0200 0x0 0x200>;
	};
    */
    data->usbmisc_data = usbmisc_get_init_data(dev);
    if (IS_ERR(data->usbmisc_data))
        return PTR_ERR(data->usbmisc_data);
    
    //
    data->pinctrl = devm_pinctrl_get(dev);
    if (IS_ERR(data->pinctrl)) {
        dev_dbg(dev, "pinctrl get failed, err=%ld\n", PTR_ERR(data->pinctrl));
    } else {
        pinctrl_hsic_idle = pinctrl_lookup_state(data->pinctrl, "idle");
        if (IS_ERR(pinctrl_hsic_idle)) {
            dev_dbg(dev, "pinctrl_hsic_idle lookup failed, err=%ld\n", PTR_ERR(pinctrl_hsic_idle));
        } else {
            ret = pinctrl_select_state(data->pinctrl, pinctrl_hsic_idle);
            if (ret) {
                dev_err(dev,"hsic_idle select failed, err=%d\n",ret);                    
                return ret;
            }
        }

        data->pinctrl_hsic_active = pinctrl_lookup_state(data->pinctrl, "active");
        if (IS_ERR(data->pinctrl_hsic_active))
            dev_dbg(dev, "pinctrl_hsic_active lookup failed, err=%ld\n", PTR_ERR(data->pinctrl_hsic_active));
    }

    ret = imx_get_clks(dev);
    if (ret)
        return ret;

    request_bus_freq(BUS_FREQ_HIGH);
    if (pdata.flags & CI_HDRC_PMQOS)
        pm_qos_add_request(&data->pm_qos_req, PM_QOS_CPU_DMA_LATENCY, 0);

    /*
    */
    ret = imx_prepare_enable_clks(dev);
    if (ret)
        goto err_bus_freq;
    /*
    fsl,usbphy = <&usbphy1>;
    */
    data->phy = devm_usb_get_phy_by_phandle(dev, "fsl,usbphy", 0);
    if (IS_ERR(data->phy)) {
        ret = PTR_ERR(data->phy);
        /* Return -EINVAL if no usbphy is available */
        if (ret == -ENODEV)
            ret = -EINVAL;
        goto err_clk;
    }

    pdata.usb_phy = data->phy;
    data->usbmisc_data->usb_phy = data->phy;
    if (pdata.flags & CI_HDRC_SUPPORTS_RUNTIME_PM)
        data->supports_runtime_pm = true;

    // dts配置中不存在 ci-disable-lpm值，所以以下代码不执行 
    if (of_find_property(np, "ci-disable-lpm", NULL)) {
        data->supports_runtime_pm = false;
        pdata.flags &= ~CI_HDRC_SUPPORTS_RUNTIME_PM;
    }
    /*
       of_usb_get_phy_mode  @   drivers/usb/phy/of.c   //获取dts配置中phy_type的值
       这里 of_usb_get_phy_mode 返回 USBPHY_INTERFACE_MODE_UNKNOWN，所以以下代码不执行
    */
    if (of_usb_get_phy_mode(dev->of_node) == USBPHY_INTERFACE_MODE_HSIC) {
        pdata.flags |= CI_HDRC_IMX_IS_HSIC;
        data->usbmisc_data->hsic = 1;
        data->hsic_pad_regulator = devm_regulator_get(dev, "pad");
        if (PTR_ERR(data->hsic_pad_regulator) == -EPROBE_DEFER) {
            ret = -EPROBE_DEFER;
            goto err_clk;
        } else if (PTR_ERR(data->hsic_pad_regulator) == -ENODEV) {                       
            /* no pad regualator is needed */
            data->hsic_pad_regulator = NULL;
        } else if (IS_ERR(data->hsic_pad_regulator)) {
            dev_err(dev, "Get hsic pad regulator error: %ld\n",PTR_ERR(data->hsic_pad_regulator));
            ret = PTR_ERR(data->hsic_pad_regulator);
            goto err_clk;
        }

        if (data->hsic_pad_regulator) {
            ret = regulator_enable(data->hsic_pad_regulator);
            if (ret) {
                dev_err(dev, "Fail to enable hsic pad regulator\n");
                goto err_clk;
            }
        }
    }

    /**
        "fsl,anatop" 属性不存在,以下代码不执行
    */
    if (of_find_property(np, "fsl,anatop", NULL) && data->usbmisc_data) {
        data->anatop = syscon_regmap_lookup_by_phandle(np, "fsl,anatop");
        if (IS_ERR(data->anatop)) {
            dev_dbg(dev, "failed to find regmap for anatop\n");
            ret = PTR_ERR(data->anatop);
            goto disable_hsic_regulator;
        }
        data->usbmisc_data->anatop = data->anatop;
    }

    //
    ret = imx_usbmisc_init(data->usbmisc_data);
    if (ret) {
        dev_err(dev, "usbmisc init failed, ret=%d\n", ret);
        goto disable_hsic_regulator;
    }

    data->ci_pdev = ci_hdrc_add_device(dev, pdev->resource, pdev->num_resources, &pdata);
    if (IS_ERR(data->ci_pdev)) {
        ret = PTR_ERR(data->ci_pdev);
        if (ret != -EPROBE_DEFER)
            dev_err(dev, "ci_hdrc_add_device failed, err=%d\n", ret);
        goto disable_hsic_regulator;
    }

    ret = imx_usbmisc_init_post(data->usbmisc_data);
    if (ret) {
        dev_err(dev, "usbmisc post failed, ret=%d\n", ret);
        goto disable_device;
    }                                                           
    ret = imx_usbmisc_set_wakeup(data->usbmisc_data, false);
    if (ret) {
        dev_err(dev, "usbmisc set_wakeup failed, ret=%d\n", ret);
        goto disable_device;
    }

    /* usbmisc needs to know dr mode to choose wakeup setting */
    if (data->usbmisc_data)
        data->usbmisc_data->available_role = ci_hdrc_query_available_role(data->ci_pdev);

    if (data->supports_runtime_pm) {
        pm_runtime_set_active(dev);
        pm_runtime_enable(dev);
    }

    device_set_wakeup_capable(dev, true);

    return 0;

disable_device:
    ci_hdrc_remove_device(data->ci_pdev);
disable_hsic_regulator:
    if (data->hsic_pad_regulator)
        ret = regulator_disable(data->hsic_pad_regulator);
err_clk:
    imx_disable_unprepare_clks(&pdev->dev);
err_bus_freq:
    if (pdata.flags & CI_HDRC_PMQOS)
        pm_qos_remove_request(&data->pm_qos_req);
    release_bus_freq(BUS_FREQ_HIGH);
    return ret;
}


static int ci_hdrc_imx_notify_event(struct ci_hdrc *ci, unsigned event)                                                                                                                                        
{
    struct device *dev = ci->dev->parent;
    struct ci_hdrc_imx_data *data = dev_get_drvdata(dev);
    int ret = 0;
    struct imx_usbmisc_data *mdata = data->usbmisc_data;

    switch (event) {
    case CI_HDRC_CONTROLLER_VBUS_EVENT:
        if (ci->vbus_active)
            ret = imx_usbmisc_charger_detection(mdata, true);
        else
            ret = imx_usbmisc_charger_detection(mdata, false);
        break;
    case CI_HDRC_IMX_HSIC_ACTIVE_EVENT:
        if (!IS_ERR(data->pinctrl) &&
            !IS_ERR(data->pinctrl_hsic_active)) {
            ret = pinctrl_select_state(data->pinctrl,data->pinctrl_hsic_active);
            if (ret)
                dev_err(dev,"hsic_active select failed, err=%d\n",ret);
            return ret;
        }
        break;
    case CI_HDRC_IMX_HSIC_SUSPEND_EVENT:
        if (data->usbmisc_data) {
            ret = imx_usbmisc_hsic_set_connect(data->usbmisc_data);
            if (ret)
                dev_err(dev,"hsic_set_connect failed, err=%d\n",ret);
            return ret;
        }
        break;
    case CI_HDRC_IMX_TERM_SELECT_OVERRIDE_FS:
        if (data->usbmisc_data)
            return imx_usbmisc_term_select_override(data->usbmisc_data, true, 1);
        break;
    case CI_HDRC_IMX_TERM_SELECT_OVERRIDE_OFF:
        if (data->usbmisc_data)
            return imx_usbmisc_term_select_override(data->usbmisc_data, false, 0);
        break;
    default:
        dev_dbg(dev, "unknown event\n");
    }

    return ret;
}

/**
 * devm_usb_get_phy_by_phandle - find the USB PHY by phandle
 * @dev - device that requests this phy
 * @phandle - name of the property holding the phy phandle value
 * @index - the index of the phy
 *
 * Returns the phy driver associated with the given phandle value,
 * after getting a refcount to it, -ENODEV if there is no such phy or
 * -EPROBE_DEFER if there is a phandle to the phy, but the device is
 * not yet loaded. While at that, it also associates the device with
 * the phy using devres. On driver detach, release function is invoked
 * on the devres data, then, devres data is freed.
 *
 * For use by USB host and peripheral drivers.                                                                                                                                                                 
 */
struct usb_phy *devm_usb_get_phy_by_phandle(struct device *dev, const char *phandle, u8 index)
{
    struct device_node *node;
    struct usb_phy  *phy;

    if (!dev->of_node) {
        dev_dbg(dev, "device does not have a device node entry\n");
        return ERR_PTR(-EINVAL);
    }   

    node = of_parse_phandle(dev->of_node, phandle, index);
    if (!node) {
        dev_dbg(dev, "failed to get %s phandle in %pOF node\n", phandle, dev->of_node);
        return ERR_PTR(-ENODEV);
    }   
    phy = devm_usb_get_phy_by_node(dev, node, NULL);
    of_node_put(node);
    return phy;
}
EXPORT_SYMBOL_GPL(devm_usb_get_phy_by_phandle);



/*
// 通过dts 获取 fsl,usbmisc = <&usbmisc1 0>; 

usbmisc1: usbmisc@5b0d0200 {
    #index-cells = <1>;
    compatible = "fsl,imx7d-usbmisc", "fsl,imx6q-usbmisc";
    reg = <0x0 0x5b0d0200 0x0 0x200>;
};
*/
static struct imx_usbmisc_data *usbmisc_get_init_data(struct device *dev)
{
    struct platform_device *misc_pdev;
    struct device_node *np = dev->of_node;
    struct of_phandle_args args;
    struct imx_usbmisc_data *data;
    int ret;

    /*
     * In case the fsl,usbmisc property is not present this device doesn't
     * need usbmisc. Return NULL (which is no error here)
     */
    if (!of_get_property(np, "fsl,usbmisc", NULL))
        return NULL;

    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return ERR_PTR(-ENOMEM);

    //
    ret = of_parse_phandle_with_args(np, "fsl,usbmisc", "#index-cells",0, &args);
    if (ret) {
        dev_err(dev, "Failed to parse property fsl,usbmisc, errno %d\n",ret);
        return ERR_PTR(ret);
    }

    // #index-cells = <1>;
    data->index = args.args[0];

    misc_pdev = of_find_device_by_node(args.np);
    of_node_put(args.np);

    if (!misc_pdev || !platform_get_drvdata(misc_pdev))
        return ERR_PTR(-EPROBE_DEFER);

    data->dev = &misc_pdev->dev;

    if (of_find_property(np, "disable-over-current", NULL))
        data->disable_oc = 1;

    if (of_find_property(np, "over-current-active-high", NULL))
        data->oc_polarity = 1;

    if (of_find_property(np, "power-polarity-active-high", NULL))
        data->pwr_polarity = 1;

    if (of_find_property(np, "external-vbus-divider", NULL))
        data->evdo = 1;

    if (of_usb_get_phy_mode(np) == USBPHY_INTERFACE_MODE_ULPI)
         data->ulpi = 1;

    if (of_find_property(np, "osc-clkgate-delay", NULL)) {
        ret = of_property_read_u32(np, "osc-clkgate-delay", &data->osc_clkgate_delay);
        if (ret) {
            dev_err(dev, "failed to get osc-clkgate-delay value\n");
            return ERR_PTR(ret);
        }
        /*
         * 0 <= osc_clkgate_delay <=7
         * - 0x0 (default) is 0.5ms,
         * - 0x1-0x7: 1-7ms
         */
        if (data->osc_clkgate_delay > 7) {
            dev_err(dev, "value of osc-clkgate-delay is incorrect\n");
            return ERR_PTR(-EINVAL);
        }
    }

    of_property_read_u32(np, "picophy,pre-emp-curr-control",&data->emp_curr_control);
    of_property_read_u32(np, "picophy,dc-vol-level-adjust",&data->dc_vol_level_adjust);

    return data;
}

