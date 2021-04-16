//  @   drivers/usb/cdns3/host.c

/**
*static int __init cdns3_driver_platform_register(void)  // drivers/usb/cdns3/core.c:1001:module_init(cdns3_driver_platform_register);
*-->
*/
void __init cdns3_host_driver_init(void) 
{
    printk("***%s@@@ \n",__func__);
    // xhci_cdns3_overrides  @   drivers/usb/cdns3/host.c
    xhci_init_driver(&xhci_cdns3_hc_driver, &xhci_cdns3_overrides);
}

static const struct xhci_driver_overrides xhci_cdns3_overrides __initconst = {                                                                                                                                 
    .extra_priv_size = sizeof(struct xhci_hcd),
    .reset = xhci_cdns3_setup,
    .bus_suspend = xhci_cdns3_bus_suspend,
};


/**
*
*/
void xhci_init_driver(struct hc_driver *drv,const struct xhci_driver_overrides *over)
{   
    BUG_ON(!over);

    /* Copy the generic table to drv then apply the overrides */
    *drv = xhci_hc_driver;
    
    if (over) { 
        drv->hcd_priv_size += over->extra_priv_size;
        if (over->reset)
            drv->reset = over->reset;
        if (over->start)
            drv->start = over->start;
        if (over->bus_suspend)
            drv->bus_suspend = over->bus_suspend;
    }
}
EXPORT_SYMBOL_GPL(xhci_init_driver);

static const struct hc_driver xhci_hc_driver = {
    .description =      "xhci-hcd",
    .product_desc =     "xHCI Host Controller",
    .hcd_priv_size =    sizeof(struct xhci_hcd),

    /*
     * generic hardware linkage
     */
    .irq =          xhci_irq,
    .flags =        HCD_MEMORY | HCD_USB3 | HCD_SHARED,

    /*
     * basic lifecycle operations
     */
    .reset =        NULL, /* set in xhci_init_driver() */
    .start =        xhci_run,
    .stop =         xhci_stop,
    .shutdown =     xhci_shutdown,

    /*
     * managing i/o requests and associated device resources
     */
    .urb_enqueue =      xhci_urb_enqueue,
    .urb_dequeue =      xhci_urb_dequeue,
    .alloc_dev =        xhci_alloc_dev,
    .free_dev =     xhci_free_dev,
    .alloc_streams =    xhci_alloc_streams,
    .free_streams =     xhci_free_streams,
    .add_endpoint =     xhci_add_endpoint,
    .drop_endpoint =    xhci_drop_endpoint,
    .endpoint_reset =   xhci_endpoint_reset,
    .check_bandwidth =  xhci_check_bandwidth,
    .reset_bandwidth =  xhci_reset_bandwidth,
    .address_device =   xhci_address_device,
    .enable_device =    xhci_enable_device,
    .update_hub_device =    xhci_update_hub_device,
    .reset_device =     xhci_discover_or_reset_device,

    /*
     * scheduling support
     */
    .get_frame_number = xhci_get_frame,

    /*
     * root hub support
     */
    .hub_control =      xhci_hub_control,
    .hub_status_data =  xhci_hub_status_data,
    .bus_suspend =      xhci_bus_suspend,
    .bus_resume =       xhci_bus_resume,
    
    /*
     * call back when device connected and addressed
     */
    .update_device =        xhci_update_device,
    .set_usb2_hw_lpm =  xhci_set_usb2_hardware_lpm,
    .enable_usb3_lpm_timeout =  xhci_enable_usb3_lpm_timeout,
    .disable_usb3_lpm_timeout = xhci_disable_usb3_lpm_timeout,
    .find_raw_port_number = xhci_find_raw_port_number,
    .submit_single_step_set_feature = xhci_submit_single_step_set_feature,
};

