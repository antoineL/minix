/*	image.h - Info between installboot and boot.	Author: Kees J. Bot
 */

#include <minix/a.out.h>

#define IM_NAME_MAX	63

struct image_header {
	char		name[IM_NAME_MAX + 1];	/* Null terminated. */
	struct exec	process;
};
