#include <lib.h>
#define setpgid	_setpgid
#include <unistd.h>
#include <string.h>

PUBLIC pid_t setpgid(pid_t pid, pid_t pgid)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m1_i1 = pid;
  m.m1_i2 = pgid;
  return(_syscall(PM_PROC_NR, SETPGID, &m));
}
