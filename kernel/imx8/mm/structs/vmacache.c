

// @	include/linux/mm_types_task.h

#define VMACACHE_BITS 2
#define VMACACHE_SIZE (1U << VMACACHE_BITS)

// 在struct task_struct结构中有 struct vmacache	 vmacache;成员

struct vmacache {
	u64 seqnum;
	struct vm_area_struct *vmas[VMACACHE_SIZE];  //struct vm_area_struct *vmas[4];
};