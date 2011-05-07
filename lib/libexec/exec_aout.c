#define _SYSTEM 1

#include "libexec.h"
#include <minix/const.h>
#include <sys/param.h>
#include <assert.h>
#include <errno.h>
#include <minix/a.out.h>
#include <sys/mman.h>

int read_header_aout(
  const char *exec_hdr,		/* executable header */
  size_t exec_len,		/* executable file size */
  struct image_memmap *p	/* place to return values */
)
{
/* Read the header and extract the text, data, bss and total sizes from it. */
  struct exec *hdr;		/* a.out header is read in here */

  /* Read the header and check the magic number.  The standard MINIX header
   * is defined in <a.out.h>.  It consists of 8 chars followed by 6 longs.
   * Then come 4 more longs that are not used here.
   *	Byte 0: magic number 0x01
   *	Byte 1: magic number 0x03
   *	Byte 2: normal = 0x10 (not checked, 0 is OK), separate I/D = 0x20
   *	Byte 3: CPU type, Intel 16 bit = 0x04, Intel 32 bit = 0x10,
   *            Motorola = 0x0B, Sun SPARC = 0x17
   *	Byte 4: Header length = 0x20
   *	Bytes 5-7 are not used.
   *
   *	Now come the 6 longs
   *	Bytes  8-11: size of text segments in bytes
   *	Bytes 12-15: size of initialized data segment in bytes
   *	Bytes 16-19: size of bss in bytes
   *	Bytes 20-23: program entry point
   *	Bytes 24-27: total memory allocated to program (text, data + stack)
   *	Bytes 28-31: size of symbol table in bytes
   * The longs are represented in a machine dependent order,
   * little-endian on the 8088, big-endian on the 68000.
   * The header is followed directly by the text and data segments, and the
   * symbol table (if any). The sizes are given in the header. Only the
   * text and data segments are copied into memory by exec. The header is
   * used here only. The symbol table is for the benefit of a debugger and
   * is ignored here.
   */

  assert(exec_hdr != NULL);
  assert(p != NULL);
  memset(p, 0, sizeof(struct image_memmap));

  hdr = (struct exec *)exec_hdr;
  if (exec_len < A_MINHDR) return(ENOEXEC);

  /* Check magic number, cpu type, and flags. */
  if (BADMAG(*hdr)) return(ENOEXEC);

  switch(hdr->a_cpu) {
#if _MINIX_CHIP == _CHIP_INTEL
  case A_I8086:	p->machine = MACHINE_I8086;	break;
  case A_I80386:p->machine = MACHINE_I80386;	break;
#endif
  default: return(ENOEXEC);	/* unrecognized */
  }

  if ((hdr->a_flags & ~(A_NSYM | A_EXEC | A_SEP | A_PAL | A_UZP)) != 0)
	return(ENOEXEC);
#if 0
  if ((hdr->a_flags & (A_EXEC | A_SEP)) == 0)  /* either bit should be set */
	return(ENOEXEC);
#else
  /* Some tools (like GNU binutils) do not set up correctly the A_EXEC
   * bit; so we skip this check. We ought to set the flag right too...
   */
#endif

  if (hdr->a_total == 0) return(ENOEXEC);

  /* Get text position and sizes. */
  p->text_.fileoffset =
	hdr->a_flags & A_PAL ? 0 : hdr->a_hdrlen & BYTE; /* header-in-text? */
  p->text_.vaddr = p->text_.paddr =
	hdr->a_flags & A_UZP ? PAGE_SIZE : 0;	/* unmapped zero-page? */
  p->text_.filebytes = p->text_.membytes =
	(hdr->a_flags & A_PAL ? hdr->a_hdrlen & BYTE : 0) /* header-in-text?*/
	+ (vir_bytes) hdr->a_text;		/* text size in bytes */
  p->text_.flags = 0;
  p->text_.prot = PROT_READ | PROT_EXEC;
  if (hdr->a_entry < p->text_.vaddr
   || hdr->a_entry >= (p->text_.vaddr + p->text_.membytes) )
	return(ENOEXEC);	/* entry point should be within .text */

  /* Get data position and sizes. */
  p->data_.fileoffset = p->text_.fileoffset + p->text_.filebytes;
  if (hdr->a_flags & A_SEP)
	  p->data_.vaddr = p->data_.paddr =
		hdr->a_flags & A_UZP ? PAGE_SIZE : 0; /* unmapped zero-page? */
  else /* common I+D */
	  p->data_.vaddr = p->data_.paddr = p->text_.vaddr + p->text_.membytes;
  p->data_.filebytes = (vir_bytes) hdr->a_data;	/* data size in bytes */
  p->data_.membytes = (vir_bytes) hdr->a_data + hdr->a_bss; /* to allocate */
  p->data_.flags = MAP_PRIVATE;
  p->data_.prot = PROT_READ | PROT_WRITE;

  p->top_alloc = hdr->a_total;	/* total bytes to allocate for prog */
  p->stack_bytes = hdr->a_total - (p->data_.vaddr + p->data_.membytes);
  p->entry = hdr->a_entry;	/* initial address to start execution */

  if (! (hdr->a_flags & A_SEP)) {
	/* If I & D spaces are not separated, all is considered data. Text=0*/
	p->data_.fileoffset = p->text_.fileoffset;
	p->data_.vaddr = p->data_.paddr = p->text_.vaddr;
	p->data_.filebytes += p->text_.filebytes;
	p->data_.membytes += p->text_.membytes;
	p->data_.flags |= p->text_.flags & ~MAP_SHARED;
	p->data_.prot |= p->text_.prot;
	p->text_.filebytes = p->text_.membytes = 0;
	p->nr_regions = 1;	/* just one region to allocate */
  } else
	p->nr_regions = 2;

  return(OK);
}
