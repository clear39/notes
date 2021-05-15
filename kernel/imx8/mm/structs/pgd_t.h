


//	@	arch/arm64/include/asm/pgtable-types.h
// pgd 为物理页面的基地址
typedef struct { pgdval_t pgd; } pgd_t;

typedef u64 pgdval_t;