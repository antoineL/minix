#define	_SYSTEM	1

#include <libexec.h>
#include <errno.h>

int exec_memmap(const char exec_hdr[], size_t hdr_length,
	struct image_memmap * out)
{
  int r;

  r = read_header_aout(exec_hdr, hdr_length, out);
  /* Loaded successfully, so no need to try other loaders */
  if (r == OK) return r;
  return read_header_elf(exec_hdr, hdr_length, out);
}
