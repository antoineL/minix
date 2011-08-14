#define tcsetpgrp _tcsetpgrp
#define ioctl _ioctl
#include <unistd.h>
#include <sys/ioctl.h>

int tcsetpgrp(int fd, pid_t pgid)
{
  return ioctl(fd, TIOCSPGRP, (void *) &pgid);
}
