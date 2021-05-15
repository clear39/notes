
//  @   include/linux/usb/otg.h
struct usb_otg {                                                                                                                                                                                               
    u8          default_a;

    struct phy      *phy;
    /* old usb_phy interface */
    struct usb_phy      *usb_phy;
    struct usb_bus      *host;
    struct usb_gadget   *gadget;

    enum usb_otg_state  state;

    /* bind/unbind the host controller */
    int (*set_host)(struct usb_otg *otg, struct usb_bus *host);

    /* bind/unbind the peripheral controller */
    int (*set_peripheral)(struct usb_otg *otg,struct usb_gadget *gadget);

    /* effective for A-peripheral, ignored for B devices */
    int (*set_vbus)(struct usb_otg *otg, bool enabled);

    /* for B devices only:  start session with A-Host */
    int (*start_srp)(struct usb_otg *otg);

    /* start or continue HNP role switch */
    int (*start_hnp)(struct usb_otg *otg);

};