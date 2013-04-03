/*
minix/svrctl.h

Created:	Feb 15, 1994 by Philip Homburg <philip@cs.vu.nl>
*/

#ifndef _MINIX_SVRCTL_H
#define _MINIX_SVRCTL_H

#include <sys/types.h>

/* Server control commands have the same encoding as the commands for ioctls. */
#include <minix/ioctl.h>

/* PM controls. */
#define PMGETPARAM	_IOW('M',  5, struct sysgetenv)
	/* XXX really   _IORW('M', 5,... */
/*define PMSETPARAM	_IOR('M',  7, struct sysgetenv) * dropped 2013-04-02 */

/* VFS controls */
#define VFSSETPARAM	_IOR('F', 130, struct sysgetenv)
#define _VFSSETPARAM_M	_IOR('M', 130, struct sysgetenv) /* compat 2013-04-02 */
#define VFSGETPARAM	_IORW('F',131, struct sysgetenv)
#define _VFSGETPARAM_M	_IOR('M', 131, struct sysgetenv) /* compat 2013-04-02 */

struct sysgetenv {
	char		*key;		/* Name requested. */
	size_t		keylen;		/* Length of name including \0. */
	char		*val;		/* Buffer for returned data. */
	size_t		vallen;		/* Size of return data buffer. */
};

int svrctl(int _request, void *_data);

#endif /* _MINIX_SVRCTL_H */
