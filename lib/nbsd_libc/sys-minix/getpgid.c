#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <unistd.h>
#include <string.h>

pid_t getpgid(pid_t p)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m1_i1 = (int)p;
  return(_syscall(PM_PROC_NR, GETPGID_SID, &m));
}
