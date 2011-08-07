#define tcgetsid _tcgetsid
#define ioctl _ioctl
#include <sys/types.h>
#include <termios.h>
#include <sys/ioctl.h>

pid_t tcgetsid(int fd)
{
  pid_t p;

  if (ioctl(fd, TIOCGSID, (void *)&p) < 0)
	return (pid_t)-1;
  return p;
}
