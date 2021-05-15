struct ci_hdrc_imx_data {                                                                                                                                                                                      
    struct usb_phy *phy;
    struct platform_device *ci_pdev;
    struct clk *clk;
    struct imx_usbmisc_data *usbmisc_data;
    bool supports_runtime_pm;
    bool in_lpm;
    struct regmap *anatop;
    struct pinctrl *pinctrl;
    struct pinctrl_state *pinctrl_hsic_active;
    struct regulator *hsic_pad_regulator;
    const struct ci_hdrc_imx_platform_flag *data;
    /* SoC before i.mx6 (except imx23/imx28) needs three clks */
    bool need_three_clks;
    struct clk *clk_ipg;
    struct clk *clk_ahb;
    struct clk *clk_per;
    /* --------------------------------- */
    struct pm_qos_request pm_qos_req;
};
