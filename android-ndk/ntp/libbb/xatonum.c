/* vi: set sw=4 ts=4: */
/*
 * ascii-to-numbers implementations for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"


int xatou(const char *buf)
{
	char c;
	int i;
	int j=0;
	int retval=0;
	char mod[3]={1,10,100};
	int len=strlen(buf);
	if( (!len)||(len > 3) )
	{
		return -1;
	}
	for (i=len-1; i>=0; i--)
	{
		c=buf[i];
		retval += atoi(&c)*mod[j++];
	}
	return retval;
}

int xatoul_range(const char *buf,int low,int top){
	int retval=xatou(buf);
	if(retval<low){retval=low;}
	if(retval>top){retval=top;}
	ALOGV("buf = %s\n",buf);
	ALOGV("###retval = %d\n",retval);
	return retval;
}


/* A few special cases */

int FAST_FUNC xatoi_positive(const char *numstr)
{
	return xatoul_range(numstr, 0, INT_MAX);
}
