
// const char *type = of_get_flat_dt_prop(node, "device_type", NULL);

// node 为偏移
// name 获取键值对
// size 返回获取的大小
const void *__init of_get_flat_dt_prop(unsigned long node, const char *name,int *size)
{
	// initial_boot_params ？？
	return fdt_getprop(initial_boot_params, node, name, size);
}


const void *fdt_getprop(const void *fdt, int nodeoffset, const char *name, int *lenp)
{
	return fdt_getprop_namelen(fdt, nodeoffset, name, strlen(name), lenp);
}

/*
typedef __be32 fdt32_t;

struct fdt_property {
	fdt32_t tag;
	fdt32_t len;
	fdt32_t nameoff;
	char data[0];
};
*/
const void *fdt_getprop_namelen(const void *fdt, int nodeoffset, const char *name, int namelen, int *lenp)
{
	const struct fdt_property *prop;

	prop = fdt_get_property_namelen(fdt, nodeoffset, name, namelen, lenp);
	if (! prop)
		return NULL;

	return prop->data;
}

const struct fdt_property *fdt_get_property_namelen(const void *fdt, int offset, const char *name, int namelen, int *lenp)
{
	for (offset = fdt_first_property_offset(fdt, offset);
	     (offset >= 0);
	     (offset = fdt_next_property_offset(fdt, offset))) {
		const struct fdt_property *prop;

		// fdt_get_property_by_offset.c
		if (!(prop = fdt_get_property_by_offset(fdt, offset, lenp))) {
			offset = -FDT_ERR_INTERNAL;
			break;
		}
		if (_fdt_string_eq(fdt, fdt32_to_cpu(prop->nameoff), name, namelen))
			return prop;
	}

	if (lenp)
		*lenp = offset;
	return NULL;
}


