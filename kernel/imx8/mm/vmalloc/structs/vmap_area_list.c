// @ mm/vmalloc.c

LIST_HEAD(vmap_area_list);


struct list_head {
	struct list_head *next, *prev;
};


#define LIST_HEAD(name)  struct list_head name = LIST_HEAD_INIT(name)   

// struct list_head vmap_area_list = LIST_HEAD_INIT(vmap_area_list)


#define LIST_HEAD_INIT(name) { &(name), &(name) }




