#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <unistd.h>
#include <string.h>

pid_t setpgid(pid_t pid, pid_t pgid)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m1_i1 = (int)pid;
  m.m1_i2 = (int)pgid;
  return(_syscall(PM_PROC_NR, SETPGID, &m));
}
