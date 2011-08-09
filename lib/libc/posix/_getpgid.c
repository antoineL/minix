#include <lib.h>
#define getpgid	_getpgid
#include <unistd.h>
#include <string.h>

PUBLIC pid_t getpgid(pid_t p)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m1_i1 = p;
  return(_syscall(PM_PROC_NR, GETPGID_SID, &m));
}
