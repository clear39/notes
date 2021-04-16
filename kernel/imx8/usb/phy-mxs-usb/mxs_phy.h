//  @   drivers/usb/phy/phy-mxs-usb.c
struct mxs_phy {                                                                                                                                                                                               
    struct usb_phy phy; 
    struct clk *clk;
    const struct mxs_phy_data *data;
    struct regmap *regmap_anatop;
    struct regmap *regmap_sim;
    int port_id;
    u32 tx_reg_set;
    u32 tx_reg_mask;
    struct regulator *phy_3p0;
    bool hardware_control_phy2_clk;
    enum usb_current_mode mode;
    unsigned long clk_rate;
};