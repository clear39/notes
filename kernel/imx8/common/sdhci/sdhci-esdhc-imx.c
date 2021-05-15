//  @   drivers/mmc/host/sdhci-esdhc-imx.c
static struct platform_driver sdhci_esdhc_imx_driver = {
    .driver     = {  
        .name   = "sdhci-esdhc-imx",
        .of_match_table = imx_esdhc_dt_ids,
        .pm = &sdhci_esdhc_pmops,
    },   
    .id_table   = imx_esdhc_devtype,                                                                                                                                                                           
    .probe      = sdhci_esdhc_imx_probe,
    .remove     = sdhci_esdhc_imx_remove,
};

module_platform_driver(sdhci_esdhc_imx_driver);


static const struct of_device_id imx_esdhc_dt_ids[] = {                                                                                                                                                        
    { .compatible = "fsl,imx25-esdhc", .data = &esdhc_imx25_data, },
    { .compatible = "fsl,imx35-esdhc", .data = &esdhc_imx35_data, },
    { .compatible = "fsl,imx51-esdhc", .data = &esdhc_imx51_data, },
    { .compatible = "fsl,imx53-esdhc", .data = &esdhc_imx53_data, },
    { .compatible = "fsl,imx6sx-usdhc", .data = &usdhc_imx6sx_data, },
    { .compatible = "fsl,imx6sl-usdhc", .data = &usdhc_imx6sl_data, },
    { .compatible = "fsl,imx6q-usdhc", .data = &usdhc_imx6q_data, },
    { .compatible = "fsl,imx6ull-usdhc", .data = &usdhc_imx6ull_data, },
    { .compatible = "fsl,imx7d-usdhc", .data = &usdhc_imx7d_data, },
    { .compatible = "fsl,imx7ulp-usdhc", .data = &usdhc_imx7ulp_data, },
    { .compatible = "fsl,imx8qm-usdhc", .data = &usdhc_imx8qm_data, },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_esdhc_dt_ids);


static const struct platform_device_id imx_esdhc_devtype[] = {                                                                                                                                                 
    {
        .name = "sdhci-esdhc-imx25",
        .driver_data = (kernel_ulong_t) &esdhc_imx25_data,
    }, {
        .name = "sdhci-esdhc-imx35",
        .driver_data = (kernel_ulong_t) &esdhc_imx35_data,
    }, {
        .name = "sdhci-esdhc-imx51",
        .driver_data = (kernel_ulong_t) &esdhc_imx51_data,
    }, {
        /* sentinel */
    }
};
MODULE_DEVICE_TABLE(platform, imx_esdhc_devtype);


static int sdhci_esdhc_imx_probe(struct platform_device *pdev)
{
    const struct of_device_id *of_id = of_match_device(imx_esdhc_dt_ids, &pdev->dev);
    struct sdhci_pltfm_host *pltfm_host;
    struct sdhci_host *host;
    struct cqhci_host *cq_host;
    int err; 
    struct pltfm_imx_data *imx_data;

    host = sdhci_pltfm_init(pdev, &sdhci_esdhc_imx_pdata, sizeof(*imx_data));
    if (IS_ERR(host))
        return PTR_ERR(host);

    pltfm_host = sdhci_priv(host);

    imx_data = sdhci_pltfm_priv(pltfm_host);

    imx_data->socdata = of_id ? of_id->data : (struct esdhc_soc_data *) pdev->id_entry->driver_data;

    imx_data->clk_ipg = devm_clk_get(&pdev->dev, "ipg");
    if (IS_ERR(imx_data->clk_ipg)) {
        err = PTR_ERR(imx_data->clk_ipg);
        goto free_sdhci;
    }    

    imx_data->clk_ahb = devm_clk_get(&pdev->dev, "ahb");
    if (IS_ERR(imx_data->clk_ahb)) {
        err = PTR_ERR(imx_data->clk_ahb);
        goto free_sdhci;
    }    

    imx_data->clk_per = devm_clk_get(&pdev->dev, "per");
    if (IS_ERR(imx_data->clk_per)) {
        err = PTR_ERR(imx_data->clk_per);
        goto free_sdhci;
    }    

    pltfm_host->clk = imx_data->clk_per;
    pltfm_host->clock = clk_get_rate(pltfm_host->clk);

    if (imx_data->socdata->flags & ESDHC_FLAG_BUSFREQ)
        request_bus_freq(BUS_FREQ_HIGH);

    if (imx_data->socdata->flags & ESDHC_FLAG_PMQOS)
        pm_qos_add_request(&imx_data->pm_qos_req,
            PM_QOS_CPU_DMA_LATENCY, 0);

    err = clk_prepare_enable(imx_data->clk_per);
    if (err)
        goto free_sdhci;
    err = clk_prepare_enable(imx_data->clk_ipg);
    if (err)
        goto disable_per_clk;
    err = clk_prepare_enable(imx_data->clk_ahb);
    if (err)
        goto disable_ipg_clk;

    imx_data->pinctrl = devm_pinctrl_get(&pdev->dev);
    if (IS_ERR(imx_data->pinctrl)) {
        err = PTR_ERR(imx_data->pinctrl);
        dev_warn(mmc_dev(host->mmc), "could not get pinctrl\n");
        imx_data->pins_default = ERR_PTR(-EINVAL);
    } else {
        imx_data->pins_default = pinctrl_lookup_state(imx_data->pinctrl,PINCTRL_STATE_DEFAULT);
        if (IS_ERR(imx_data->pins_default))
            dev_warn(mmc_dev(host->mmc), "could not get default state\n");
    }

    if (esdhc_is_usdhc(imx_data)) {
        host->quirks2 |= SDHCI_QUIRK2_PRESET_VALUE_BROKEN;
        host->mmc->caps |= MMC_CAP_1_8V_DDR;
        if (!(imx_data->socdata->flags & ESDHC_FLAG_HS200))
            host->quirks2 |= SDHCI_QUIRK2_BROKEN_HS200;

        /* clear tuning bits in case ROM has set it already */
        writel(0x0, host->ioaddr + ESDHC_MIX_CTRL);
        writel(0x0, host->ioaddr + SDHCI_ACMD12_ERR);
        writel(0x0, host->ioaddr + ESDHC_TUNE_CTRL_STATUS);
    }

    host->tuning_delay = 1;

    if (imx_data->socdata->flags & ESDHC_FLAG_MAN_TUNING)
        sdhci_esdhc_ops.platform_execute_tuning = esdhc_executing_tuning;

    if (imx_data->socdata->flags & ESDHC_FLAG_ERR004536)
        host->quirks |= SDHCI_QUIRK_BROKEN_ADMA;

    if (imx_data->socdata->flags & ESDHC_FLAG_HS400)
        host->quirks2 |= SDHCI_QUIRK2_CAPS_BIT63_FOR_HS400;

    if (imx_data->socdata->flags & ESDHC_FLAG_HS400_ES) {
        host->mmc->caps2 |= MMC_CAP2_HS400_ES;
        host->mmc_host_ops.hs400_enhanced_strobe = esdhc_hs400_enhanced_strobe;
    }
    if (imx_data->socdata->flags & ESDHC_FLAG_CQHCI) {
        host->mmc->caps2 |= MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD;
        cq_host = devm_kzalloc(&pdev->dev, sizeof(*cq_host), GFP_KERNEL);
        if (IS_ERR(cq_host)) {
            err = PTR_ERR(cq_host);
            goto disable_ahb_clk;
        }

        cq_host->mmio = host->ioaddr + ESDHC_CQHCI_ADDR_OFFSET;
        cq_host->ops = &esdhc_cqhci_ops;

        err = cqhci_init(cq_host, host->mmc, false);
        if (err)
            goto disable_ahb_clk;
    }

    if (of_id)
        err = sdhci_esdhc_imx_probe_dt(pdev, host, imx_data);
    else
        err = sdhci_esdhc_imx_probe_nondt(pdev, host, imx_data);
    if (err)
        goto disable_ahb_clk;

    sdhci_esdhc_imx_hwinit(host);

    device_set_wakeup_capable(&pdev->dev, 1);

    err = sdhci_add_host(host);
    if (err)
        goto disable_ahb_clk;

    pm_runtime_set_active(&pdev->dev);
    pm_runtime_set_autosuspend_delay(&pdev->dev, 50);
    pm_runtime_use_autosuspend(&pdev->dev);
    pm_suspend_ignore_children(&pdev->dev, 1);
    pm_runtime_enable(&pdev->dev);

    return 0;

disable_ahb_clk:
    clk_disable_unprepare(imx_data->clk_ahb);
disable_ipg_clk:
    clk_disable_unprepare(imx_data->clk_ipg);
disable_per_clk:
    clk_disable_unprepare(imx_data->clk_per);

    if (imx_data->socdata->flags & ESDHC_FLAG_BUSFREQ)
        release_bus_freq(BUS_FREQ_HIGH);

    if (imx_data->socdata->flags & ESDHC_FLAG_PMQOS)
        pm_qos_remove_request(&imx_data->pm_qos_req);

free_sdhci:
    sdhci_pltfm_free(pdev);
    return err;
}
                                                
