#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

int utimensat(int fd, const char *name, const struct timespec tv[2],
    int flags)
{
  message m;
  static const struct timespec now[2] = { {0, UTIME_NOW}, {0, UTIME_NOW} };

  if (tv == NULL) tv = now;

  if (name == NULL) return EINVAL;
  if (fd != AT_FDCWD && name[0] != '/') return EINVAL; /* Not supported */
  m.m2_i1 = strlen(name) + 1;
  m.m2_p1 = (char *) __UNCONST(name);
  m.m2_l1 = tv[0].tv_sec;
  m.m2_l2 = tv[1].tv_sec;
  m.m2_i2 = tv[0].tv_nsec;
  m.m2_i3 = tv[1].tv_nsec;
  if ((unsigned)flags > SHRT_MAX)
	return EINVAL;
  else
	m.m2_s1 = flags;

  return(_syscall(VFS_PROC_NR, UTIMENS, &m));
}
