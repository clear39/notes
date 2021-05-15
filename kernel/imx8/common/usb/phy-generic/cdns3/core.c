//  @   drivers/usb/cdns3/core.c

#ifdef CONFIG_OF  // CONFIG_OF = y
static const struct of_device_id of_cdns3_match[] = {                                                                                                                                                          
    { .compatible = "Cadence,usb3" },
    { },
};
MODULE_DEVICE_TABLE(of, of_cdns3_match);
#endif


static struct platform_driver cdns3_driver = {
    .probe      = cdns3_probe,
    .remove     = cdns3_remove,
    .driver     = {
        .name   = "cdns-usb3",
        .of_match_table = of_match_ptr(of_cdns3_match),
        .pm = &cdns3_pm_ops,
    },
};

static int __init cdns3_driver_platform_register(void)
{
    pr_info("***%s@@@ \n",__func__);
    //  @  host.c
    // drivers/usb/cdns3/host.c:31:static struct hc_driver __read_mostly xhci_cdns3_hc_driver;
    // 在 cdns3_host_driver_init 中初始化 xhci_cdns3_hc_driver 在 
    // xhci_cdns3_hc_driver 在 cdns3_probe 调用 cdns3_core_init_role 再调用 cdns3_host_init 中，
    // 初始化的 cdns3_role_driver 中赋值的 cdns3_host_start 中使用
    cdns3_host_driver_init();
    // 其中的cdns3_probe函数
    return platform_driver_register(&cdns3_driver);
}
module_init(cdns3_driver_platform_register);

static void __exit cdns3_driver_platform_unregister(void)
{
    platform_driver_unregister(&cdns3_driver);
}
module_exit(cdns3_driver_platform_unregister);

MODULE_ALIAS("platform:cdns-usb3");
MODULE_AUTHOR("Peter Chen <peter.chen@nxp.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Cadence USB3 DRD Controller Driver");





/**
 * cdns3_probe - probe for cdns3 core device
 * @pdev: Pointer to cdns3 core platform device
 *
 * Returns 0 on success otherwise negative errno
 */
static int cdns3_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct resource *res;
    struct cdns3 *cdns;
    void __iomem *regs;
    int ret;

    
    cdns = devm_kzalloc(dev, sizeof(*cdns), GFP_KERNEL);
    if (!cdns)
        return -ENOMEM;

    cdns->dev = dev;
    // 将 cdns 赋值给pdev->dev->driver_data
    platform_set_drvdata(pdev, cdns);

    res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
    if (!res) {
        dev_err(dev, "missing IRQ\n");
        return -ENODEV;
    }
    cdns->irq = res->start;

    /*
     * Request memory region
     * region-0: nxp wrap registers
     * region-1: xHCI
     * region-2: Peripheral
     * region-3: PHY registers
     * region-4: OTG registers
     */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    regs = devm_ioremap_resource(dev, res);
    if (IS_ERR(regs))
        return PTR_ERR(regs);
    cdns->none_core_regs = regs;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    regs = devm_ioremap_resource(dev, res);
    if (IS_ERR(regs))
        return PTR_ERR(regs);
    cdns->xhci_regs = regs;
    cdns->xhci_res = res;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
    regs = devm_ioremap_resource(dev, res);
    if (IS_ERR(regs))
        return PTR_ERR(regs);
    cdns->dev_regs  = regs;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
    regs = devm_ioremap_resource(dev, res);
    if (IS_ERR(regs))
        return PTR_ERR(regs);
    cdns->phy_regs = regs;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
    regs = devm_ioremap_resource(dev, res);
    if (IS_ERR(regs))
        return PTR_ERR(regs);
    cdns->otg_regs = regs;

    mutex_init(&cdns->mutex);
    ret = cdns3_get_clks(dev);
    if (ret)
        return ret;

    ret = cdns3_prepare_enable_clks(dev);
    if (ret)
        return ret;

    cdns->usbphy = devm_usb_get_phy_by_phandle(dev, "cdns3,usbphy", 0);
    if (IS_ERR(cdns->usbphy)) {
        ret = PTR_ERR(cdns->usbphy);
        if (ret == -ENODEV)
            ret = -EINVAL;
        goto err1;
    }

    ret = usb_phy_init(cdns->usbphy);
    if (ret)
        goto err1;

    // 这里调用 cdns3_host_init 
    ret = cdns3_core_init_role(cdns);
    if (ret)
        goto err2;

    if (cdns->roles[CDNS3_ROLE_GADGET]) {
        INIT_WORK(&cdns->role_switch_wq, cdns3_role_switch);
        ret = cdns3_register_extcon(cdns);
        if (ret)
            goto err3;
    }

    cdns->role = cdns3_get_role(cdns);
    dev_dbg(dev, "the init role is %d\n", cdns->role);
    cdns_set_role(cdns, cdns->role);
    ret = cdns3_role_start(cdns, cdns->role);
    if (ret) {
        dev_err(dev, "can't start %s role\n", cdns3_role(cdns)->name);
        goto err3;                                
    }

    ret = devm_request_irq(dev, cdns->irq, cdns3_irq, IRQF_SHARED,
            dev_name(dev), cdns);
    if (ret)
        goto err4;

    device_set_wakeup_capable(dev, true);
    pm_runtime_set_active(dev);
    pm_runtime_enable(dev);
    /*
     * The controller needs less time between bus and controller suspend,
     * and we also needs a small delay to avoid frequently entering low
     * power mode.
     */
    pm_runtime_set_autosuspend_delay(dev, 20);
    pm_runtime_mark_last_busy(dev);
    pm_runtime_use_autosuspend(dev);
    dev_dbg(dev, "Cadence USB3 core: probe succeed\n");

    return 0;

err4:
    cdns3_role_stop(cdns);
err3:
    cdns3_remove_roles(cdns);
err2:
    usb_phy_shutdown(cdns->usbphy);
err1:
    cdns3_disable_unprepare_clks(dev);
    return ret;
}






