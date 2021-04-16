/**
 * struct rpmsg_driver - rpmsg driver struct
 * @drv: underlying device driver
 * @id_table: rpmsg ids serviced by this driver
 * @probe: invoked when a matching rpmsg channel (i.e. device) is found
 * @remove: invoked when the rpmsg channel is removed
 * @callback: invoked when an inbound message is received on the channel
 */                                                                                                                                                                                                            
struct rpmsg_driver {
    struct device_driver drv;
    const struct rpmsg_device_id *id_table;
    int (*probe)(struct rpmsg_device *dev);
    void (*remove)(struct rpmsg_device *dev);
    int (*callback)(struct rpmsg_device *, void *, int, void *, u32);
};


/* rpmsg */

#define RPMSG_NAME_SIZE         32
#define RPMSG_DEVICE_MODALIAS_FMT   "rpmsg:%s"

struct rpmsg_device_id {                                                                                                                                                                                       
    char name[RPMSG_NAME_SIZE];
}; 



/**
 * rpmsg_device - device that belong to the rpmsg bus
 * @dev: the device struct
 * @id: device id (used to match between rpmsg drivers and devices)
 * @src: local address
 * @dst: destination address
 * @ept: the rpmsg endpoint of this channel
 * @announce: if set, rpmsg will announce the creation/removal of this channel
 */
struct rpmsg_device {                                                                                                                                                                                          
    struct device dev;
    struct rpmsg_device_id id;
    u32 src;
    u32 dst;
    struct rpmsg_endpoint *ept;
    bool announce;

    const struct rpmsg_device_ops *ops;
}

/**
 * struct rpmsg_device_ops - indirection table for the rpmsg_device operations
 * @create_ept:     create backend-specific endpoint, requried
 * @announce_create:    announce presence of new channel, optional
 * @announce_destroy:   announce destruction of channel, optional
 *  
 * Indirection table for the operations that a rpmsg backend should implement.
 * @announce_create and @announce_destroy are optional as the backend might
 * advertise new channels implicitly by creating the endpoints.
 */ 
struct rpmsg_device_ops {                                                                                                                                                                                      
    struct rpmsg_endpoint *(*create_ept)(struct rpmsg_device *rpdev,
                        rpmsg_rx_cb_t cb, void *priv,
                        struct rpmsg_channel_info chinfo);

    int (*announce_create)(struct rpmsg_device *ept);
    int (*announce_destroy)(struct rpmsg_device *ept);
};

/**
 * struct rpmsg_channel_info - channel info representation
 * @name: name of service
 * @src: local address
 * @dst: destination address
 */
struct rpmsg_channel_info {
    char name[RPMSG_NAME_SIZE];
    u32 src;
    u32 dst;
};









/**
 * struct rpmsg_endpoint - binds a local rpmsg address to its user
 * @rpdev: rpmsg channel device
 * @refcount: when this drops to zero, the ept is deallocated
 * @cb: rx callback handler
 * @cb_lock: must be taken before accessing/changing @cb
 * @addr: local rpmsg address
 * @priv: private data for the driver's use
 *
 * In essence, an rpmsg endpoint represents a listener on the rpmsg bus, as
 * it binds an rpmsg address with an rx callback handler.
 *
 * Simple rpmsg drivers shouldn't use this struct directly, because
 * things just work: every rpmsg driver provides an rx callback upon
 * registering to the bus, and that callback is then bound to its rpmsg
 * address when the driver is probed. When relevant inbound messages arrive
 * (i.e. messages which their dst address equals to the src address of
 * the rpmsg channel), the driver's handler is invoked to process it.
 *
 * More complicated drivers though, that do need to allocate additional rpmsg
 * addresses, and bind them to different rx callbacks, must explicitly
 * create additional endpoints by themselves (see rpmsg_create_ept()).
 */
struct rpmsg_endpoint {
    struct rpmsg_device *rpdev;
    struct kref refcount;
    rpmsg_rx_cb_t cb;
    struct mutex cb_lock;
    u32 addr;
    void *priv;

    const struct rpmsg_endpoint_ops *ops;
};

typedef int (*rpmsg_rx_cb_t)(struct rpmsg_device *, void *, int, void *, u32);


/** 
 * struct rpmsg_endpoint_ops - indirection table for rpmsg_endpoint operations
 * @destroy_ept:    destroy the given endpoint, required
 * @send:       see @rpmsg_send(), required
 * @sendto:     see @rpmsg_sendto(), optional
 * @send_offchannel:    see @rpmsg_send_offchannel(), optional
 * @trysend:        see @rpmsg_trysend(), required
 * @trysendto:      see @rpmsg_trysendto(), optional
 * @trysend_offchannel: see @rpmsg_trysend_offchannel(), optional
 *
 * Indirection table for the operations that a rpmsg backend should implement.
 * In addition to @destroy_ept, the backend must at least implement @send and
 * @trysend, while the variants sending data off-channel are optional.
 */
struct rpmsg_endpoint_ops {
    void (*destroy_ept)(struct rpmsg_endpoint *ept);

    int (*send)(struct rpmsg_endpoint *ept, void *data, int len);
    int (*sendto)(struct rpmsg_endpoint *ept, void *data, int len, u32 dst);
    int (*send_offchannel)(struct rpmsg_endpoint *ept, u32 src, u32 dst,
                  void *data, int len);

    int (*trysend)(struct rpmsg_endpoint *ept, void *data, int len);
    int (*trysendto)(struct rpmsg_endpoint *ept, void *data, int len, u32 dst);
    int (*trysend_offchannel)(struct rpmsg_endpoint *ept, u32 src, u32 dst,
                 void *data, int len);
};