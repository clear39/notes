

//这里以i2c_rpmsg_driver注册为例

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




static struct rpmsg_driver i2c_rpmsg_driver = {                                                                                                                                                                
    .drv.name   = "i2c-rpmsg",
    .drv.owner  = THIS_MODULE,
    .id_table   = i2c_rpmsg_id_table,
    .probe      = i2c_rpmsg_probe,
    .remove     = i2c_rpmsg_remove,
    .callback   = i2c_rpmsg_cb,
};


static struct rpmsg_device_id i2c_rpmsg_id_table[] = { 
    { .name = "rpmsg-i2c-channel" },
    { },
};

/* use a macro to avoid include chaining to get THIS_MODULE */
#define register_rpmsg_driver(drv)  __register_rpmsg_driver(drv, THIS_MODULE)


/** 
 * __register_rpmsg_driver() - register an rpmsg driver with the rpmsg bus
 * @rpdrv: pointer to a struct rpmsg_driver
 * @owner: owning module/driver
 *  
 * Returns 0 on success, and an appropriate error value on failure.
 */
int __register_rpmsg_driver(struct rpmsg_driver *rpdrv, struct module *owner)                                                                                                                                  
{
    rpdrv->drv.bus = &rpmsg_bus;
    rpdrv->drv.owner = owner;

    //最终通用的驱动注册函数
    return driver_register(&rpdrv->drv);
}
EXPORT_SYMBOL(__register_rpmsg_driver);



//  @   drivers/i2c/busses/i2c-rpmsg-imx.c
static int i2c_rpmsg_probe(struct rpmsg_device *rpdev)                                                                                                                                                         
{
    int ret = 0;

    if (!rpdev) {
        dev_info(&rpdev->dev, "%s failed, rpdev=NULL\n", __func__);
        return -EINVAL;
    }   

    //  static struct i2c_rpmsg_info i2c_rpmsg; 
    i2c_rpmsg.rpdev = rpdev;

    mutex_init(&i2c_rpmsg.lock);
    init_completion(&i2c_rpmsg.cmd_complete);

    dev_info(&rpdev->dev, "new channel: 0x%x -> 0x%x!\n",rpdev->src, rpdev->dst);

    return ret;
}


struct i2c_rpmsg_msg {
    struct imx_rpmsg_head header;

    /* Payload Start*/
    u8 bus_id;
    u8 ret_val;
    u16 addr;
    u16 flags;
    u16 len;
    u8 buf[I2C_RPMSG_MAX_BUF_SIZE];
} __packed __aligned(1);

struct i2c_rpmsg_info {
    struct rpmsg_device *rpdev;
    struct device *dev;
    struct i2c_rpmsg_msg *msg;
    struct completion cmd_complete;
    struct mutex lock;

    u8 bus_id;
    u16 addr;
};

struct completion {
    /* Hopefuly this won't overflow. */
    unsigned int count;
};


static inline void init_completion(struct completion *c)                                                                                                                                                       
{
    c->count = 0;
}
