/*	genmap - output binary keymap			Author: Marcus Hampel
 *
 * This program should be compiled for the host machine, then run locally.
 */

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE 1
#endif

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>              /* _O_BINARY */
#include <io.h>
#else
#include <unistd.h>
#endif

#include <minix/keymap.h>

uint16_t keymap[NR_SCAN_CODES * MAP_COLS] = {
#include KEYSRC
};

uint8_t comprmap[4 + NR_SCAN_CODES * MAP_COLS * 9/8 * 2 + 1];

void tell(const char *s)
{
  write(2, s, strlen(s));
}

int main(void)
{
  uint8_t *cm, *fb;
  uint16_t *km;
  int n;

  /* Compress the keymap. */
  memcpy(comprmap, KEY_MAGIC, 4);
  cm = comprmap + 4;
  n = 8;
  for (km = keymap; km < keymap + NR_SCAN_CODES * MAP_COLS; km++) {
	if (n == 8) {
		/* Allocate a new flag byte. */
		fb = cm;
		*cm++ = 0;
		n= 0;
	}
	*cm++ = (*km & 0x00FF);		/* Low byte. */
	if (*km & 0xFF00) {
		*cm++ = (*km >> 8);	/* High byte only when set. */
		*fb |= (1 << n);	/* Set a flag if so. */
	}
	n++;
  }

  /* Don't store trailing zeros. */
  while (cm > comprmap && cm[-1] == 0) cm--;

# ifdef _WIN32
  setmode( 1, _O_BINARY );	/* stdout */
# endif

  /* Emit the compressed keymap. */
  if (write(1, comprmap, cm - comprmap) < 0) {
	int err = errno;

	tell("genmap: ");
	tell(strerror(err));
	tell("\n");
	exit(1);
  }
  exit(0);
}
