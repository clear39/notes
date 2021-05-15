
//  @   include/linux/usb/phy.h

struct usb_phy {
    struct device       *dev; //
    const char      *label;
    unsigned int         flags;

    enum usb_phy_type   type;
    enum usb_phy_events last_event;

    struct usb_otg      *otg;

    struct device       *io_dev;
    struct usb_phy_io_ops   *io_ops;
    void __iomem        *io_priv;

    /* to support extcon device */
    struct extcon_dev   *edev;
    struct extcon_dev   *id_edev;
    struct notifier_block   vbus_nb;
    struct notifier_block   id_nb;
    struct notifier_block   type_nb;

    /* Support USB charger(充电器) */
    enum usb_charger_type   chg_type;  // 
    enum usb_charger_state  chg_state;
    struct usb_charger_current  chg_cur;
    struct work_struct      chg_work;

    /* for notification of usb_phy_events */
    struct atomic_notifier_head notifier;

    /* to pass extra port status to the root hub */
    u16         port_status;
    u16         port_change;

    /* to support controllers that have multiple phys */
    struct list_head    head;

    /* initialize/shutdown the phy */
    int (*init)(struct usb_phy *x);
    void    (*shutdown)(struct usb_phy *x);

    /* enable/disable VBUS */
    int (*set_vbus)(struct usb_phy *x, int on);

    /* effective for B devices, ignored for A-peripheral */
    int (*set_power)(struct usb_phy *x, unsigned mA);

    /* Set phy into suspend mode */
    int (*set_suspend)(struct usb_phy *x, int suspend);
    /*
     * Set wakeup enable for PHY, in that case, the PHY can be
     * woken up from suspend status due to external events,
     * like vbus change, dp/dm change and id.
     */
    int (*set_wakeup)(struct usb_phy *x, bool enabled);

    /* notify phy connect status change */
    int (*notify_connect)(struct usb_phy *x, enum usb_device_speed speed);
    int (*notify_disconnect)(struct usb_phy *x, enum usb_device_speed speed);

    /*
     * Charger detection method can be implemented if you need to
     * manually detect the charger type.
     */
    enum usb_charger_type (*charger_detect)(struct usb_phy *x);

    int (*notify_suspend)(struct usb_phy *x, enum usb_device_speed speed);
    int (*notify_resume)(struct usb_phy *x, enum usb_device_speed speed);

    int (*set_mode)(struct usb_phy *x, enum usb_current_mode mode);

};