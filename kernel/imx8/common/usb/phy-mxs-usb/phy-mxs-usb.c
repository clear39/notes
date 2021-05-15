usbphy1: usbphy@0x5b100000 {
    compatible = "fsl,imx8qm-usbphy", "fsl,imx7ulp-usbphy", "fsl,imx6ul-usbphy", "fsl,imx23-usbphy";
    reg = <0x0 0x5b100000 0x0 0x1000>;
    clocks = <&clk IMX8QXP_USB2_PHY_IPG_CLK>;
    power-domains = <&pd_conn_usbotg0_phy>;
};


//  @   drivers/usb/phy/phy-mxs-usb.c
static struct platform_driver mxs_phy_driver = {
    .probe = mxs_phy_probe,
    .remove = mxs_phy_remove,
    .driver = {
        .name = DRIVER_NAME,                        // #define DRIVER_NAME "mxs_phy"
        .of_match_table = mxs_phy_dt_ids,
        .pm = &mxs_phy_pm,
     },
};

static int __init mxs_phy_module_init(void)
{
    pr_info("***%s@@@ \n",__func__);
    return platform_driver_register(&mxs_phy_driver);                                                                                                                                                          
}
postcore_initcall(mxs_phy_module_init);
    

static const struct of_device_id mxs_phy_dt_ids[] = {                                                                                                                                                          
    { .compatible = "fsl,imx7ulp-usbphy", .data = &imx7ulp_phy_data, },
    { .compatible = "fsl,imx6ul-usbphy", .data = &imx6sx_phy_data, },
    { .compatible = "fsl,imx6sx-usbphy", .data = &imx6sx_phy_data, },
    { .compatible = "fsl,imx6sl-usbphy", .data = &imx6sl_phy_data, },
    { .compatible = "fsl,imx6q-usbphy", .data = &imx6q_phy_data, },
    { .compatible = "fsl,imx23-usbphy", .data = &imx23_phy_data, },
    { .compatible = "fsl,vf610-usbphy", .data = &vf610_phy_data, },
    { .compatible = "fsl,imx6ul-usbphy", .data = &imx6ul_phy_data, },
    { /* sentinel */ }
};



static int mxs_phy_probe(struct platform_device *pdev)
{
    struct resource *res;
    void __iomem *base;
    struct clk *clk;
    struct mxs_phy *mxs_phy;
    int ret;
    const struct of_device_id *of_id;
    struct device_node *np = pdev->dev.of_node;
    u32 val;

    of_id = of_match_device(mxs_phy_dt_ids, &pdev->dev);
    if (!of_id)
        return -ENODEV;

    //  @   include/linux/ioport.h:38:#define IORESOURCE_MEM		0x00000200
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

    base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(base))
        return PTR_ERR(base);

    clk = devm_clk_get(&pdev->dev, NULL);
    if (IS_ERR(clk)) {
        dev_err(&pdev->dev,"can't get the clock, err=%ld", PTR_ERR(clk));
        return PTR_ERR(clk);
    }

    //  构建mxs_phy
    mxs_phy = devm_kzalloc(&pdev->dev, sizeof(*mxs_phy), GFP_KERNEL);
    if (!mxs_phy)
        return -ENOMEM;

    mxs_phy->clk_rate = clk_get_rate(clk);
    /* Some SoCs don't have anatop registers */
    if (of_get_property(np, "fsl,anatop", NULL)) {
        mxs_phy->regmap_anatop = syscon_regmap_lookup_by_phandle(np, "fsl,anatop");
        if (IS_ERR(mxs_phy->regmap_anatop)) {
            dev_dbg(&pdev->dev,"failed to find regmap for anatop\n");
            return PTR_ERR(mxs_phy->regmap_anatop);
        }
    }

    /* Currently, only imx7ulp has SIM module */
    if (of_get_property(np, "nxp,sim", NULL)) {
        mxs_phy->regmap_sim = syscon_regmap_lookup_by_phandle (np, "nxp,sim");
        if (IS_ERR(mxs_phy->regmap_sim)) {
            dev_dbg(&pdev->dev, "failed to find regmap for sim\n");
            return PTR_ERR(mxs_phy->regmap_sim);          
         }
    }

    /* Precompute which bits of the TX register are to be updated, if any */
    if (!of_property_read_u32(np, "fsl,tx-cal-45-dn-ohms", &val) &&
        val >= MXS_PHY_TX_CAL45_MIN && val <= MXS_PHY_TX_CAL45_MAX) {
        /* Scale to a 4-bit value */
        val = (MXS_PHY_TX_CAL45_MAX - val) * 0xF / (MXS_PHY_TX_CAL45_MAX - MXS_PHY_TX_CAL45_MIN);
        mxs_phy->tx_reg_mask |= GM_USBPHY_TX_TXCAL45DN(~0);
        mxs_phy->tx_reg_set  |= GM_USBPHY_TX_TXCAL45DN(val);
    }

    if (!of_property_read_u32(np, "fsl,tx-cal-45-dp-ohms", &val) &&
        val >= MXS_PHY_TX_CAL45_MIN && val <= MXS_PHY_TX_CAL45_MAX) {
        /* Scale to a 4-bit value. */
        val = (MXS_PHY_TX_CAL45_MAX - val) * 0xF / (MXS_PHY_TX_CAL45_MAX - MXS_PHY_TX_CAL45_MIN);
        mxs_phy->tx_reg_mask |= GM_USBPHY_TX_TXCAL45DP(~0);
        mxs_phy->tx_reg_set  |= GM_USBPHY_TX_TXCAL45DP(val);
    }

    if (!of_property_read_u32(np, "fsl,tx-d-cal", &val) &&
        val >= MXS_PHY_TX_D_CAL_MIN && val <= MXS_PHY_TX_D_CAL_MAX) {
        /* Scale to a 4-bit value.  Round up the values and heavily
         * weight the rounding by adding 2/3 of the denominator.
         */
        val = ((MXS_PHY_TX_D_CAL_MAX - val) * 0xF
            + (MXS_PHY_TX_D_CAL_MAX - MXS_PHY_TX_D_CAL_MIN) * 2/3)
            / (MXS_PHY_TX_D_CAL_MAX - MXS_PHY_TX_D_CAL_MIN);
        mxs_phy->tx_reg_mask |= GM_USBPHY_TX_D_CAL(~0);
        mxs_phy->tx_reg_set  |= GM_USBPHY_TX_D_CAL(val);
    }

    //
    ret = of_alias_get_id(np, "usbphy");
    if (ret < 0)
        dev_dbg(&pdev->dev, "failed to get alias id, errno %d\n", ret);
    mxs_phy->port_id = ret;
    mxs_phy->clk = clk;
    mxs_phy->data = of_id->data;

    mxs_phy->phy.io_priv        = base;
    mxs_phy->phy.dev        = &pdev->dev;
    mxs_phy->phy.label      = DRIVER_NAME;
    mxs_phy->phy.init       = mxs_phy_init;
    mxs_phy->phy.shutdown       = mxs_phy_shutdown;
    mxs_phy->phy.set_suspend    = mxs_phy_suspend;
    mxs_phy->phy.notify_connect = mxs_phy_on_connect;
    mxs_phy->phy.notify_disconnect  = mxs_phy_on_disconnect;
    mxs_phy->phy.type       = USB_PHY_TYPE_USB2;
    mxs_phy->phy.set_wakeup     = mxs_phy_set_wakeup;
    mxs_phy->phy.set_mode       = mxs_phy_set_mode;
    if (mxs_phy->data->flags & MXS_PHY_SENDING_SOF_TOO_FAST) {
        mxs_phy->phy.notify_suspend = mxs_phy_on_suspend;
        mxs_phy->phy.notify_resume = mxs_phy_on_resume;
    }

    if (mxs_phy->data->flags & MXS_PHY_HAS_DCD)
        mxs_phy->phy.charger_detect = mxs_phy_dcd_flow;
    else
        mxs_phy->phy.charger_detect = mxs_phy_charger_detect;

    mxs_phy->phy_3p0 = devm_regulator_get(&pdev->dev, "phy-3p0");
    if (PTR_ERR(mxs_phy->phy_3p0) == -EPROBE_DEFER) {
        return -EPROBE_DEFER;
    } else if (PTR_ERR(mxs_phy->phy_3p0) == -ENODEV) {
        /* not exist */
        mxs_phy->phy_3p0 = NULL;
    } else if (IS_ERR(mxs_phy->phy_3p0)) {
        dev_err(&pdev->dev, "Getting regulator error: %ld\n",PTR_ERR(mxs_phy->phy_3p0));
        return PTR_ERR(mxs_phy->phy_3p0);
    }
    if (mxs_phy->phy_3p0)
        regulator_set_voltage(mxs_phy->phy_3p0, 3200000, 3200000);

    if (mxs_phy->data->flags & MXS_PHY_HARDWARE_CONTROL_PHY2_CLK)
        mxs_phy->hardware_control_phy2_clk = true;

    platform_set_drvdata(pdev, mxs_phy);

    device_set_wakeup_capable(&pdev->dev, true);

    return usb_add_phy_dev(&mxs_phy->phy);
}


/** 
 * usb_add_phy_dev - declare the USB PHY
 * @x: the USB phy to be used; or NULL
 *      
 * This call is exclusively for use by phy drivers, which
 * coordinate the activities of drivers for host and peripheral
 * controllers, and in some cases for VBUS current regulation.
 */
int usb_add_phy_dev(struct usb_phy *x)
{
    struct usb_phy_bind *phy_bind;
    unsigned long flags;
    int ret;

    if (!x->dev) {
        dev_err(x->dev, "no device provided for PHY\n");
        return -EINVAL;
    }

    usb_charger_init(x);
    ret = usb_add_extcon(x);
    if (ret)
        return ret;

    ATOMIC_INIT_NOTIFIER_HEAD(&x->notifier);

    spin_lock_irqsave(&phy_lock, flags);
    //  @   drivers/usb/phy/phy.c:34:static LIST_HEAD(phy_bind_list);
    /**
    *  phy_bind_list 在 usb_bind_phy 中添加 
    */
    list_for_each_entry(phy_bind, &phy_bind_list, list)
        if (!(strcmp(phy_bind->phy_dev_name, dev_name(x->dev))))
            phy_bind->phy = x;

    //  @   drivers/usb/phy/phy.c:33:static LIST_HEAD(phy_list);
    list_add_tail(&x->head, &phy_list);

    spin_unlock_irqrestore(&phy_lock, flags);
    return 0;
}
EXPORT_SYMBOL_GPL(usb_add_phy_dev);


static void usb_charger_init(struct usb_phy *usb_phy)                                                                                                                                                          
{
    usb_phy->chg_type = UNKNOWN_TYPE;
    usb_phy->chg_state = USB_CHARGER_DEFAULT;
    usb_phy_set_default_current(usb_phy);
    INIT_WORK(&usb_phy->chg_work, usb_phy_notify_charger_work);
}

static void usb_phy_set_default_current(struct usb_phy *usb_phy)                                                                                                                                               
{
    usb_phy->chg_cur.sdp_min = DEFAULT_SDP_CUR_MIN;
    usb_phy->chg_cur.sdp_max = DEFAULT_SDP_CUR_MAX;
    usb_phy->chg_cur.dcp_min = DEFAULT_DCP_CUR_MIN;
    usb_phy->chg_cur.dcp_max = DEFAULT_DCP_CUR_MAX;
    usb_phy->chg_cur.cdp_min = DEFAULT_CDP_CUR_MIN;
    usb_phy->chg_cur.cdp_max = DEFAULT_CDP_CUR_MAX;
    usb_phy->chg_cur.aca_min = DEFAULT_ACA_CUR_MIN;
    usb_phy->chg_cur.aca_max = DEFAULT_ACA_CUR_MAX;
}

/**
 * usb_phy_notify_charger_work - notify the USB charger state
 * @work - the charger work to notify the USB charger state
 *
 * This work can be issued when USB charger state has been changed or
 * USB charger current has been changed, then we can notify the current
 * what can be drawn to power user and the charger state to userspace.
 *
 * If we get the charger type from extcon subsystem, we can notify the
 * charger state to power user automatically by usb_phy_get_charger_type()
 * issuing from extcon subsystem.
 *
 * If we get the charger type from ->charger_detect() instead of extcon
 * subsystem, the usb phy driver should issue usb_phy_set_charger_state()
 * to set charger state when the charger state has been changed.
 */
static void usb_phy_notify_charger_work(struct work_struct *work)
{
    struct usb_phy *usb_phy = container_of(work, struct usb_phy, chg_work);
    char uchger_state[50] = { 0 };
    char *envp[] = { uchger_state, NULL };
    unsigned int min, max;

    switch (usb_phy->chg_state) {
    case USB_CHARGER_PRESENT:
        usb_phy_get_charger_current(usb_phy, &min, &max);

        atomic_notifier_call_chain(&usb_phy->notifier, max, usb_phy);
        snprintf(uchger_state, ARRAY_SIZE(uchger_state), "USB_CHARGER_STATE=%s", "USB_CHARGER_PRESENT");
        break;
    case USB_CHARGER_ABSENT:
        usb_phy_set_default_current(usb_phy);

        atomic_notifier_call_chain(&usb_phy->notifier, 0, usb_phy);
        snprintf(uchger_state, ARRAY_SIZE(uchger_state), "USB_CHARGER_STATE=%s", "USB_CHARGER_ABSENT");
        break;
    default:
        dev_warn(usb_phy->dev, "Unknown USB charger state: %d\n", usb_phy->chg_state);
        return;
    }

    kobject_uevent_env(&usb_phy->dev->kobj, KOBJ_CHANGE, envp);
}

static int usb_add_extcon(struct usb_phy *x)
{
    int ret;

    if (of_property_read_bool(x->dev->of_node, "extcon")) {
        x->edev = extcon_get_edev_by_phandle(x->dev, 0);
        if (IS_ERR(x->edev))
            return PTR_ERR(x->edev);

        x->id_edev = extcon_get_edev_by_phandle(x->dev, 1);
        if (IS_ERR(x->id_edev)) {
            x->id_edev = NULL;
            dev_info(x->dev, "No separate ID extcon device\n");
        }

        if (x->vbus_nb.notifier_call) {
            ret = devm_extcon_register_notifier(x->dev, x->edev, EXTCON_USB, &x->vbus_nb);
            if (ret < 0) {
                dev_err(x->dev, "register VBUS notifier failed\n");
                return ret;
            }
        } else {
            x->type_nb.notifier_call = usb_phy_get_charger_type;

            ret = devm_extcon_register_notifier(x->dev, x->edev, EXTCON_CHG_USB_SDP, &x->type_nb);
            if (ret) {
                dev_err(x->dev, "register extcon USB SDP failed.\n");
                return ret;
            }

            ret = devm_extcon_register_notifier(x->dev, x->edev, EXTCON_CHG_USB_CDP, &x->type_nb);
            if (ret) {
                dev_err(x->dev, "register extcon USB CDP failed.\n");
                return ret;
            }

            ret = devm_extcon_register_notifier(x->dev, x->edev, EXTCON_CHG_USB_DCP, &x->type_nb);
            if (ret) {
                dev_err(x->dev, "register extcon USB DCP failed.\n");
                return ret;         
            }

            ret = devm_extcon_register_notifier(x->dev, x->edev, EXTCON_CHG_USB_ACA, &x->type_nb);
            if (ret) {
                dev_err(x->dev, "register extcon USB ACA failed.\n");
                return ret;
            }
        }

        if (x->id_nb.notifier_call) {
            struct extcon_dev *id_ext;

            if (x->id_edev)
                id_ext = x->id_edev;
            else
                id_ext = x->edev;

            ret = devm_extcon_register_notifier(x->dev, id_ext, EXTCON_USB_HOST, &x->id_nb);
            if (ret < 0) {
                dev_err(x->dev, "register ID notifier failed\n");
                return ret;
            }
        }
    }

    if (x->type_nb.notifier_call)
        __usb_phy_get_charger_type(x);

    return 0;
}
