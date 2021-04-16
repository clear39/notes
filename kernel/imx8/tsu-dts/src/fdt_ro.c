

struct fdt_property {
	fdt32_t tag;
	fdt32_t len;
	fdt32_t nameoff;
	char data[0];
};

// nodeoffset = 0
// name = "model"
// lenp = NULL
const void *fdt_getprop(const void *fdt, int nodeoffset, const char *name, int *lenp)
{
	return fdt_getprop_namelen(fdt, nodeoffset, name, strlen(name), lenp);
}

const void *fdt_getprop_namelen(const void *fdt, int nodeoffset, const char *name, int namelen, int *lenp)
{
	const struct fdt_property *prop;

	prop = fdt_get_property_namelen(fdt, nodeoffset, name, namelen, lenp);
	if (! prop)
		return NULL;

	return prop->data;
}


// offset = 0
const struct fdt_property *fdt_get_property_namelen(const void *fdt, int offset, const char *name, int namelen, int *lenp)
{
	for (offset = fdt_first_property_offset(fdt, offset);
	     (offset >= 0);
	     (offset = fdt_next_property_offset(fdt, offset))) {
		const struct fdt_property *prop;

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


// nodeoffset = 0
// 
int fdt_first_property_offset(const void *fdt, int nodeoffset)
{
	int offset;

	if ((offset = _fdt_check_node_offset(fdt, nodeoffset)) < 0)
		return offset;

	return _nextprop(fdt, offset);
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
int fdt_next_property_offset(const void *fdt, int offset)
{
	if ((offset = _fdt_check_prop_offset(fdt, offset)) < 0)
		return offset;

	return _nextprop(fdt, offset);
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
const struct fdt_property *fdt_get_property_by_offset(const void *fdt, int offset, int *lenp)
{
	int err;
	const struct fdt_property *prop;

	if ((err = _fdt_check_prop_offset(fdt, offset)) < 0) {
		if (lenp)
			*lenp = err;
		return NULL;
	}

	prop = _fdt_offset_ptr(fdt, offset);

	if (lenp)
		*lenp = fdt32_to_cpu(prop->len);

	return prop;
}

static inline const void *_fdt_offset_ptr(const void *fdt, int offset)
{
	return (const char *)fdt + fdt_off_dt_struct(fdt) + offset;
}




int _fdt_check_node_offset(const void *fdt, int offset)
{
	// #define FDT_TAGSIZE	sizeof(fdt32_t)  // 4
	// #define FDT_BEGIN_NODE	0x1		/* Start node: full name */
	if ((offset < 0) || (offset % FDT_TAGSIZE)
	    || (fdt_next_tag(fdt, offset, &offset) != FDT_BEGIN_NODE))
		return -FDT_ERR_BADOFFSET;

	return offset;
}


static int _nextprop(const void *fdt, int offset)
{
	uint32_t tag;
	int nextoffset;

	do {
		tag = fdt_next_tag(fdt, offset, &nextoffset);

		switch (tag) {
		case FDT_END:
			if (nextoffset >= 0)
				return -FDT_ERR_BADSTRUCTURE;
			else
				return nextoffset;

		case FDT_PROP:
			return offset;
		}
		offset = nextoffset;
	} while (tag == FDT_NOP);

	return -FDT_ERR_NOTFOUND;
}




#define EXTRACT_BYTE(x, n)	((unsigned long long)((uint8_t *)&x)[n])
#define CPU_TO_FDT32(x) ((EXTRACT_BYTE(x, 0) << 24) | (EXTRACT_BYTE(x, 1) << 16) | (EXTRACT_BYTE(x, 2) << 8) | EXTRACT_BYTE(x, 3))

static inline uint32_t fdt32_to_cpu(fdt32_t x)
{
	return (FDT_FORCE uint32_t)CPU_TO_FDT32(x);
}

uint32_t fdt_next_tag(const void *fdt, int startoffset, int *nextoffset)
{
	const fdt32_t *tagp, *lenp;
	uint32_t tag;
	int offset = startoffset;
	const char *p;

	*nextoffset = -FDT_ERR_TRUNCATED;
	// #define FDT_TAGSIZE	sizeof(fdt32_t)  // 4
	tagp = fdt_offset_ptr(fdt, offset, FDT_TAGSIZE);
	if (!tagp)
		return FDT_END; /* premature end */
	// 
	tag = fdt32_to_cpu(*tagp);
    //
	offset += FDT_TAGSIZE;

	*nextoffset = -FDT_ERR_BADSTRUCTURE;
	switch (tag) {
	case FDT_BEGIN_NODE:
		/* skip name */
		do {
			p = fdt_offset_ptr(fdt, offset++, 1);
		} while (p && (*p != '\0'));
		if (!p)
			return FDT_END; /* premature end */
		break;

	case FDT_PROP:
		lenp = fdt_offset_ptr(fdt, offset, sizeof(*lenp));
		if (!lenp)
			return FDT_END; /* premature end */
		/* skip-name offset, length and value */
		offset += sizeof(struct fdt_property) - FDT_TAGSIZE
			+ fdt32_to_cpu(*lenp);
		break;

	case FDT_END:
	case FDT_END_NODE:
	case FDT_NOP:
		break;

	default:
		return FDT_END;
	}

	if (!fdt_offset_ptr(fdt, startoffset, offset - startoffset))
		return FDT_END; /* premature end */

	*nextoffset = FDT_TAGALIGN(offset);
	return tag;
}


const void *fdt_offset_ptr(const void *fdt, int offset, unsigned int len)
{
	// #define fdt_off_dt_struct(fdt)		(fdt_get_header(fdt, off_dt_struct))
	unsigned absoffset = offset + fdt_off_dt_struct(fdt);

    // #define fdt_totalsize(fdt)		(fdt_get_header(fdt, totalsize))
	if ((absoffset < offset)
	    || ((absoffset + len) < absoffset)
	    || (absoffset + len) > fdt_totalsize(fdt))
		return NULL;

	// #define fdt_version(fdt)		(fdt_get_header(fdt, version))
	// #define fdt_size_dt_struct(fdt)		(fdt_get_header(fdt, size_dt_struct))
	if (fdt_version(fdt) >= 0x11)
		if (((offset + len) < offset)
		    || ((offset + len) > fdt_size_dt_struct(fdt)))
			return NULL;

	// 
	return _fdt_offset_ptr(fdt, offset);
}

static inline const void *_fdt_offset_ptr(const void *fdt, int offset)
{
	// #define fdt_off_dt_struct(fdt)		(fdt_get_header(fdt, off_dt_struct))
	return (const char *)fdt + fdt_off_dt_struct(fdt) + offset;
}

