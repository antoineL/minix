#include "syslib.h"
#include <minix/sighandled.h>

int
sighandled(endpoint_t who, int signo)
{
  message m;

  m.m1_i1 = who;
  m.m1_i2 = signo;
  return(_syscall(PM_PROC_NR, SIGHANDLED, &m));
}
