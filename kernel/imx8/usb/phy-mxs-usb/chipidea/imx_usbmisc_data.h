struct imx_usbmisc_data {
    struct device *dev;
    int index;
    struct regmap *anatop;
    struct usb_phy *usb_phy;
    
    unsigned int disable_oc:1; /* over current detect disabled */
    unsigned int oc_polarity:1; /* over current polarity if oc enabled */
    unsigned int pwr_polarity:1; /* polarity of enable vbus from pmic */
    unsigned int evdo:1; /* set external vbus divider option */
    unsigned int ulpi:1; /* connected to an ULPI phy */
    unsigned int hsic:1; /* HSIC controlller */
    /*
     * Specifies the delay between powering up the xtal 24MHz clock
     * and release the clock to the digital logic inside the analog block
     */
    unsigned int osc_clkgate_delay;
    enum usb_dr_mode available_role;
    int emp_curr_control;
    int dc_vol_level_adjust;
};
