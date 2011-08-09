#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>
#include <string.h>

pid_t getsid(pid_t p)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m1_i1 = (int)p;
  if (_syscall(PM_PROC_NR, GETPGID_SID, &m) < 0) return ( (pid_t) -1);
  return( (pid_t) m.m2_i1);
}
