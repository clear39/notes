

// @    drivers/rpmsg/rpmsg_core.c

static int __init rpmsg_init(void)
{
    int ret;

    ret = bus_register(&rpmsg_bus);
    if (ret)
        pr_err("failed to register rpmsg bus: %d\n", ret);

    return ret;
}
postcore_initcall(rpmsg_init);



static struct bus_type rpmsg_bus = {                                                                                                                                                                           
    .name       = "rpmsg",
    .match      = rpmsg_dev_match,
    .dev_groups = rpmsg_dev_groups,
    .uevent     = rpmsg_uevent,
    .probe      = rpmsg_dev_probe,
    .remove     = rpmsg_dev_remove,
};


/* match rpmsg channel and rpmsg driver */
static int rpmsg_dev_match(struct device *dev, struct device_driver *drv)                                                                                                                                      
{
    struct rpmsg_device *rpdev = to_rpmsg_device(dev);
    struct rpmsg_driver *rpdrv = to_rpmsg_driver(drv);
    const struct rpmsg_device_id *ids = rpdrv->id_table;
    unsigned int i;

    if (rpdev->driver_override)
        return !strcmp(rpdev->driver_override, drv->name);

    if (ids)
        for (i = 0; ids[i].name[0]; i++)
            if (rpmsg_id_match(rpdev, &ids[i]))
                return 1;

    return of_driver_match_device(dev, drv);
}

/*
 * when an rpmsg driver is probed with a channel, we seamlessly create
 * it an endpoint, binding its rx callback to a unique local rpmsg
 * address.
 *
 * if we need to, we also announce about this channel to the remote
 * processor (needed in case the driver is exposing an rpmsg service).
 */
static int rpmsg_dev_probe(struct device *dev)
{
    struct rpmsg_device *rpdev = to_rpmsg_device(dev);
    struct rpmsg_driver *rpdrv = to_rpmsg_driver(rpdev->dev.driver);
    struct rpmsg_channel_info chinfo = {};
    struct rpmsg_endpoint *ept = NULL;
    int err;

    if (rpdrv->callback) {
        strncpy(chinfo.name, rpdev->id.name, RPMSG_NAME_SIZE);
        chinfo.src = rpdev->src;
        chinfo.dst = RPMSG_ADDR_ANY;

        ept = rpmsg_create_ept(rpdev, rpdrv->callback, NULL, chinfo);
        if (!ept) {
            dev_err(dev, "failed to create endpoint\n");
            err = -ENOMEM;
            goto out;
        }

        rpdev->ept = ept;
        rpdev->src = ept->addr;
    }

    err = rpdrv->probe(rpdev);
    if (err) {
        dev_err(dev, "%s: failed: %d\n", __func__, err);
        if (ept)
            rpmsg_destroy_ept(ept);
        goto out;
    }

    if (rpdev->ops->announce_create)
        err = rpdev->ops->announce_create(rpdev);
out:
    return err;
}


