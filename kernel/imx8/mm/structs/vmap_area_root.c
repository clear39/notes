// @ mm/vmalloc.c

static struct rb_root vmap_area_root = RB_ROOT;


// @ include/linux/rbtree.h
#define RB_ROOT	(struct rb_root) { NULL, }

struct rb_node {
	unsigned long  __rb_parent_color;
	struct rb_node *rb_right;
	struct rb_node *rb_left;
} __attribute__((aligned(sizeof(long))));
    /* The alignment might seem pointless, but allegedly CRIS needs it */

struct rb_root {
	struct rb_node *rb_node;
};
