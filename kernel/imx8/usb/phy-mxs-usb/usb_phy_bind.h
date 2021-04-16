
//  @   include/linux/usb/phy.h
/**
 * struct usb_phy_bind - represent the binding for the phy
 * @dev_name: the device name of the device that will bind to the phy
 * @phy_dev_name: the device name of the phy
 * @index: used if a single controller uses multiple phys
 * @phy: reference to the phy
 * @list: to maintain a linked list of the binding information
 */ 
struct usb_phy_bind {                                                                                                                                                                                          
    const char  *dev_name;
    const char  *phy_dev_name;
    u8      index;
    struct usb_phy  *phy;
    struct list_head list;   //添加到 phy_bind_list
}; 