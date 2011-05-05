#define _SYSTEM 1

#include <minix/type.h>
#include <minix/const.h>
#include <sys/param.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <libexec.h>
#include <machine/elf.h>
#include <sys/mman.h>

/* For verbose logging */
#ifndef ELF_DEBUG
#define ELF_DEBUG 0
#endif

/* Support only 32-bit ELF objects */
#define __ELF_WORD_SIZE 32

static int __elfN(check_header)(const Elf_Ehdr *hdr);

int read_header_elf(
  const char exec_hdr[],	/* executable header */
  size_t hdr_length,		/* size of header already read */
  struct image_memmap *p	/* place to return values */
)
{
  const Elf_Ehdr *hdr = NULL;
  const Elf_Phdr *phdr = NULL;
  unsigned long seg_filebytes, seg_membytes;
  int i = 0;
  struct region_infos * segp;

  assert(exec_hdr != NULL);
  assert(p != NULL);

  memset(p, 0, sizeof(struct image_memmap));

  hdr = (const Elf_Ehdr *)exec_hdr;
  if (__elfN(check_header)(hdr) != OK || (hdr->e_type != ET_EXEC))
  {
     return ENOEXEC;
  }

  if ((hdr->e_phoff > hdr_length) ||
      (hdr->e_phoff + hdr->e_phentsize * hdr->e_phnum) > hdr_length) {
     return ENOEXEC;
  }

  phdr = (const Elf_Phdr *)(exec_hdr + hdr->e_phoff);
  if (!_minix_aligned(phdr, Elf_Addr)) {
     return ENOEXEC;
  }

#if ELF_DEBUG
  printf("Program header file offset (phoff): %d\n", hdr->e_phoff);
  printf("Section header file offset (shoff): %d\n", hdr->e_shoff);
  printf("Program header entry size (phentsize): %d\n", hdr->e_phentsize);
  printf("Program header entry num (phnum): %d\n", hdr->e_phnum);
  printf("Section header entry size (shentsize): %d\n", hdr->e_shentsize);
  printf("Section header entry num (shnum): %d\n", hdr->e_shnum);
  printf("Section name strings index (shstrndx): %d\n", hdr->e_shstrndx);
  printf("Entry Point: 0x%x\n", hdr->e_entry);
#endif

  assert(EXEC_TARGET_MACHINE == ELF_TARG_MACH);
  if (hdr->e_machine != ELF_TARG_MACH) {
	return ENOEXEC;
  } else
	p->machine = hdr->e_machine;
  p->abi = hdr->e_ident[EI_OSABI] | (hdr->e_ident[EI_ABIVERSION]<<8);
  p->stack_bytes = 0;		/* uses default value. */

  for (i = 0; i < hdr->e_phnum; i++) {
      switch (phdr[i].p_type) {
      case PT_LOAD:
	  if (phdr[i].p_memsz == 0)
	      break;
	  seg_filebytes = phdr[i].p_filesz;
	  seg_membytes = round_page(phdr[i].p_memsz + phdr[i].p_vaddr -
				    trunc_page(phdr[i].p_vaddr));
	  if (phdr[i].p_vaddr + seg_membytes > p->top_alloc)
		p->top_alloc = phdr[i].p_vaddr + seg_membytes;

	  if (hdr->e_entry >= phdr[i].p_vaddr &&
	      hdr->e_entry < (phdr[i].p_vaddr + phdr[i].p_memsz)) {
		segp = &p->text_;
		p->entry = (vir_bytes)hdr->e_entry;
	  } else {
		segp = &p->data_;
	  }

	  segp->vaddr = phdr[i].p_vaddr;
	  segp->paddr = phdr[i].p_paddr;
	  segp->filebytes = seg_filebytes;
	  segp->membytes = seg_membytes;
	  segp->fileoffset = phdr[i].p_offset;
	  break;
      default:
	  break;
      }
  }

#if ELF_DEBUG
  printf("Text offset:    0x%lx\n", p->text_.fileoffset);
  printf("Text vaddr:     0x%lx\n", p->text_.vaddr);
  printf("Text paddr:     0x%lx\n", p->text_.paddr);
  printf("Text filebytes: 0x%lx\n", p->text_.filebytes);
  printf("Text membytes:  0x%lx\n", p->text_.membytes);
  printf("Data offset:    0x%lx\n", p->data_.fileoffset);
  printf("Data vaddr:     0x%lx\n", p->data_.vaddr);
  printf("Data paddr:     0x%lx\n", p->data_.paddr);
  printf("Data filebyte:  0x%lx\n", p->data_.filebytes);
  printf("Data membytes:  0x%lx\n", p->data_.membytes);
  printf("Tot bytes:      0x%lx\n", p->top_alloc );
  printf("PC:             0x%lx\n", p->entry);
#endif

  if (p->text_.membytes == 0) {
	return ENOEXEC;		/* no .text! */
  }
  /* Having no .data/.bss region is possible (ld -N). */
  p->nr_regions = 1 + (p->data_.membytes != 0);

  return OK;
}

static int __elfN(check_header)(const Elf_Ehdr *hdr)
{
  if (!IS_ELF(*hdr) ||
      hdr->e_ident[EI_CLASS] != ELF_TARG_CLASS ||
      hdr->e_ident[EI_DATA] != ELF_TARG_DATA ||
      hdr->e_ident[EI_VERSION] != EV_CURRENT ||
      hdr->e_phnum == 0 ||
      hdr->e_phentsize != sizeof(Elf_Phdr) ||
      hdr->e_version != ELF_TARG_VER)
      return ENOEXEC;

  return OK;
}
