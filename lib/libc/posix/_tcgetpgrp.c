#define tcgetpgrp _tcgetpgrp
#define ioctl _ioctl
#include <unistd.h>
#include <sys/ioctl.h>

pid_t tcgetpgrp(int fd)
{
  pid_t p;

  if (ioctl(fd, TIOCGPGRP, (void *)&p) < 0)
	return (pid_t)-1;
  return p;
}
