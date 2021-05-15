
//  @   include/linux/device.h
static inline void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)                                                                                                                                   
{
    return devm_kmalloc(dev, size, gfp | __GFP_ZERO);
}


//  @   drivers/base/devres.c
/**
 * devm_kmalloc - Resource-managed kmalloc
 * @dev: Device to allocate memory for
 * @size: Allocation size
 * @gfp: Allocation gfp flags
 *
 * Managed kmalloc.  Memory allocated with this function is
 * automatically freed on driver detach.  Like all other devres
 * resources, guaranteed alignment is unsigned long long.
 *
 * RETURNS:
 * Pointer to allocated memory on success, NULL on failure.
 */
void * devm_kmalloc(struct device *dev, size_t size, gfp_t gfp)                                                                                                                                                
{
    struct devres *dr;

    /* use raw alloc_dr for kmalloc caller tracing */
    // 由于 CONFIG_NUMA 没有定义，dev_to_node 反回 -1
    dr = alloc_dr(devm_kmalloc_release, size, gfp, dev_to_node(dev));
    if (unlikely(!dr))
        return NULL; 

    /*
     * This is named devm_kzalloc_release for historical reasons
     * The initial implementation did not support kmalloc, only kzalloc
     */
    set_node_dbginfo(&dr->node, "devm_kzalloc_release", size);
    devres_add(dev, dr->data);
    return dr->data;
}
EXPORT_SYMBOL_GPL(devm_kmalloc); 


/*
struct devres_node {
    struct list_head        entry;
    dr_release_t            release;
#ifdef CONFIG_DEBUG_DEVRES
    const char          *name;
    size_t              size;
#endif
};

struct devres {                                                                                                                                                                                                
    struct devres_node      node;
    // -- 3 pointers
    unsigned long long      data[]; // guarantee ull alignment 
};
*/

static __always_inline struct devres * alloc_dr(dr_release_t release, size_t size, gfp_t gfp, int nid) 
{
    size_t tot_size = sizeof(struct devres) + size;
    struct devres *dr; 

    dr = kmalloc_node_track_caller(tot_size, gfp, nid);
    if (unlikely(!dr))
        return NULL;

    memset(dr, 0, offsetof(struct devres, data));

    INIT_LIST_HEAD(&dr->node.entry);
    dr->node.release = release;
    return dr;
}



static void add_dr(struct device *dev, struct devres_node *node)
{
    devres_log(dev, node, "ADD");
    BUG_ON(!list_empty(&node->entry)); 
    list_add_tail(&node->entry, &dev->devres_head);
}
