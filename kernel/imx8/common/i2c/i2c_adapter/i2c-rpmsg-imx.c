//  @   drivers/i2c/busses/i2c-rpmsg-imx.c
static const struct of_device_id imx_rpmsg_i2c_dt_ids[] = { 
    { .compatible = "fsl,i2c-rpbus", },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_rpmsg_i2c_dt_ids);

static struct platform_driver imx_rpmsg_i2c_driver = {
    .driver = {
        .name   = "imx_rpmsg_i2c",
        .of_match_table = imx_rpmsg_i2c_dt_ids,
    },
    .probe      = i2c_rpbus_probe,
    .remove     = i2c_rpbus_remove
};

static int __init imx_rpmsg_i2c_driver_init(void)
{
    int ret = 0;

    //
    ret = register_rpmsg_driver(&i2c_rpmsg_driver);
    if (ret < 0)
        return ret;

    // 
    return platform_driver_register(&(imx_rpmsg_i2c_driver));
}
subsys_initcall(imx_rpmsg_i2c_driver_init);

MODULE_AUTHOR("Clark Wang<xiaoning.wang@nxp.com>");
MODULE_DESCRIPTION("Driver for i2c over rpmsg");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:i2c-rpbus");  




static int i2c_rpbus_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_node *np = dev->of_node;
    struct imx_rpmsg_i2c_data *rdata;
    struct i2c_adapter *adapter;
    int ret;

    rdata = devm_kzalloc(&pdev->dev, sizeof(*rdata), GFP_KERNEL);
    if (!rdata)
        return -ENOMEM;

    adapter = &rdata->adapter;
    /* setup i2c adapter description */
    adapter->owner = THIS_MODULE;
    adapter->class = I2C_CLASS_HWMON;
    adapter->algo = &i2c_rpbus_algorithm;
    adapter->dev.parent = dev;
    adapter->dev.of_node = np;
    adapter->nr = of_alias_get_id(np, "i2c");
    /*
     * The driver will send the adapter->nr as BUS ID to the other
     * side, and the other side will check the BUS ID to see whether
     * the BUS has been registered. If there is alias id for this
     * virtual adapter, linux kernel will automatically allocate one
     * id which might be not the same number used in the other side,
     * cause i2c slave probe failure under this virtual I2C bus.
     * So let's add a BUG_ON to catch this issue earlier.
     */
    BUG_ON(adapter->nr < 0);
    adapter->quirks = &i2c_rpbus_quirks;  //
    snprintf(rdata->adapter.name, sizeof(rdata->adapter.name), "%s", "i2c-rpmsg-adapter");
    platform_set_drvdata(pdev, rdata);

    ret = i2c_add_adapter(&rdata->adapter);
    if (ret < 0) {
        dev_err(dev, "failed to add I2C adapter: %d\n", ret);
        return ret;
    }

    dev_info(dev, "add I2C adapter %s successfully\n", rdata->adapter.name);

    return 0;
}
