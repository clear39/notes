



int fdt_first_property_offset(const void *fdt, int nodeoffset)
{
	int offset;

	if ((offset = _fdt_check_node_offset(fdt, nodeoffset)) < 0)
		return offset;

	return _nextprop(fdt, offset);
}

int _fdt_check_node_offset(const void *fdt, int offset)
{
	if ((offset < 0) || (offset % FDT_TAGSIZE)
	    || (fdt_next_tag(fdt, offset, &offset) != FDT_BEGIN_NODE))
		return -FDT_ERR_BADOFFSET;

	return offset;
}



int fdt_next_property_offset(const void *fdt, int offset)
{
	if ((offset = _fdt_check_prop_offset(fdt, offset)) < 0)
		return offset;

	return _nextprop(fdt, offset);
}