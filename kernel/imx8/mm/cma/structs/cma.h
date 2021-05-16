
//	@	mm/cma.h

struct cma {
	unsigned long   base_pfn;
	unsigned long   count;
	unsigned long   *bitmap;
	unsigned int 	order_per_bit; /* Order of pages represented by one bit */
	struct 	mutex    lock;

#ifdef CONFIG_CMA_DEBUGFS
	struct hlist_head mem_head;
	spinlock_t mem_head_lock;
#endif

	const char *name;
};



#define MAX_CMA_AREAS   (1 + CONFIG_CMA_AREAS)   // CONFIG_CMA_AREAS=7