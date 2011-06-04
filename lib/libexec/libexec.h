#ifndef _LIBEXEC_H_
#define _LIBEXEC_H_ 1

#include <sys/types.h>
#include <minix/type.h>

struct region_infos {
  off_t 	fileoffset;	/* offset into file */
  phys_bytes	paddr;		/* physical address space, loading addr. */
  vir_bytes	vaddr;		/* virtual address space */
  vir_bytes	filebytes;	/* bytes as part of the file */
  vir_bytes	membytes;	/* bytes as reserved space; .data+.bss */
  unsigned	flags;		/* flags (as passed with mmap(2); unused */
  int   	prot;		/* protection required; unused */
};

struct image_memmap {
  vir_bytes	top_alloc;	/* maximum virtual address used, if known */
  vir_bytes	stack_bytes;
  vir_bytes	entry;		/* starting PC */
  short 	machine;	/* required architecture */
  short 	abi;		/* required OS ABI; unused so far */
#define	ABI_UNSPECIFIED	0	/* same as ELFODABI_NONE */
  int   	nr_regions;	/* number of regions to allocate */
  struct region_infos text_;	/* characteristics about .text */
  struct region_infos data_;	/* characteristics about .data and .bss */
};

/* Values for 'machine' member above: */
#define	MACHINE_I8086	8086
#define	MACHINE_I80386	3	/* same as EM_386 */
#define	MACHINE_M68K	4	/* same as EM_68K */
#define	MACHINE_SPARC	2	/* same as EM_SPARC */

#if (_MINIX_CHIP == _CHIP_INTEL && _WORD_SIZE == 2)
#define	EXEC_TARGET_MACHINE	MACHINE_I8086
#endif
#if (_MINIX_CHIP == _CHIP_INTEL && _WORD_SIZE == 4)
#define	EXEC_TARGET_MACHINE	MACHINE_I80386
#endif

/* recognize the exec format and fill the memory map. */
int exec_memmap(
  const char exec_hdr[],	/* header read from file */
  size_t hdr_length,		/* size of exec_hdr as read */
  struct image_memmap * out	/* cooked infos if returning OK */
);

/* (PC/IX-inherited) MINIX a.out routine */
int read_header_aout(const char [], size_t, struct image_memmap *);

/* ELF routine */
int read_header_elf(const char [], size_t, struct image_memmap *);

#endif /* !_LIBEXEC_H_ */
