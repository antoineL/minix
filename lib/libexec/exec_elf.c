#define _SYSTEM 1

#include "libexec.h"
#include <minix/type.h>
#include <minix/const.h>
#include <sys/param.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <machine/elf.h>
#include <sys/mman.h>

/* For verbose logging */
#ifndef ELF_DEBUG
#define ELF_DEBUG 0
#endif

/* Support only 32-bit ELF objects; should it change, the %d might be wrong */
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
  int i = 0;
  struct region_infos * segp;

  assert(exec_hdr != NULL);
  assert(p != NULL);

  memset(p, 0, sizeof(struct image_memmap));

  hdr = (const Elf_Ehdr *)exec_hdr;
  if (__elfN(check_header)(hdr) != OK || (hdr->e_type != ET_EXEC)) {
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
		if (phdr[i].p_vaddr + phdr[i].p_memsz > p->top_alloc)
			p->top_alloc = phdr[i].p_vaddr + phdr[i].p_memsz;

		if (phdr[i].p_flags & PF_X)
			segp = &p->text_;
		else if (phdr[i].p_filesz==0 && p->data_.membytes>0) {
		/* Supplementary NOBITS segment: should be stack! */
			p->stack_bytes = phdr[i].p_memsz;
			break;
		}
		else
			segp = &p->data_;

		if (segp->membytes != 0) {
#if ELF_DEBUG
			printf("Segment is allocated twice. "
				"Do not know how to handle.\n");
#endif
			return ENOEXEC;
		}

		segp->vaddr = phdr[i].p_vaddr;
		segp->paddr = phdr[i].p_paddr;
		segp->filebytes = phdr[i].p_filesz;
		segp->membytes = phdr[i].p_memsz;
		segp->fileoffset = phdr[i].p_offset;
		segp->prot = (phdr[i].p_flags & PF_X ? PROT_EXEC : 0)
		           | (phdr[i].p_flags & PF_W ? PROT_WRITE : 0)
		           | (phdr[i].p_flags & PF_R ? PROT_READ : 0);
		break;
	default:
		break;
	}
  }

  if (p->text_.membytes == 0) {
#if ELF_DEBUG
			printf("No text!\n");
#endif
	return ENOEXEC;
  }
  if (hdr->e_entry >= p->text_.vaddr &&
      hdr->e_entry < (p->text_.vaddr + p->text_.filebytes))
	p->entry = (vir_bytes)hdr->e_entry;
  else {
#if ELF_DEBUG
			printf("Entry point not within text!\n");
#endif
	return ENOEXEC;
  }

  /* If the executable has only one segment (ld -N), let it be data to match
   * the behaviour for aout-format combined I+D spaces.
   */
  if (p->data_.membytes == 0 && (p->text_.prot & PROT_WRITE)) {
	p->data_ = p->text_;
	p->text_.filebytes = p->text_.membytes = 0;
	p->nr_regions = 1;
  } else
	/* Having no .data/.bss region is otherwise possible. */

	p->nr_regions = 1 + (p->data_.membytes != 0);

#if ELF_DEBUG
  printf("Text offset:    0x%x\n", p->text_.fileoffset);
  printf("Text vaddr:     0x%x\n", p->text_.vaddr);
  printf("Text paddr:     0x%x\n", p->text_.paddr);
  printf("Text filebytes: 0x%x\n", p->text_.filebytes);
  printf("Text membytes:  0x%x\n", p->text_.membytes);
  printf("Data offset:    0x%x\n", p->data_.fileoffset);
  printf("Data vaddr:     0x%x\n", p->data_.vaddr);
  printf("Data paddr:     0x%x\n", p->data_.paddr);
  printf("Data filebyte:  0x%x\n", p->data_.filebytes);
  printf("Data membytes:  0x%x\n", p->data_.membytes);
  printf("Tot bytes:      0x%x\n", p->top_alloc );
  printf("PC:             0x%x\n", p->entry);
#endif

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
