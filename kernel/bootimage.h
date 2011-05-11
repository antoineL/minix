#ifndef BOOTIMAGE_H
#define BOOTIMAGE_H 1

#include <minix/com.h>
#include "type.h"

struct boot_image_memmap {
  phys_bytes text_vaddr;		/* Virtual start address of text */
  phys_bytes text_paddr;		/* Physical start address of text */
  phys_bytes text_bytes;		/* Text segment's size (bytes) */
  phys_bytes data_vaddr;		/* Virtual start address of data */
  phys_bytes data_paddr;		/* Physical start address of data */
  phys_bytes data_bytes;		/* Data segment's size (bytes) */
  phys_bytes stack_bytes;		/* Size of stack set aside (bytes) */
  phys_bytes entry;			/* Entry point of executable */
};

struct boot_image {
  proc_nr_t proc_nr;			/* process number to use */
  int flags;				/* process flags */
  int stack_kbytes;			/* stack size (in KB) */
  char proc_name[P_NAME_LEN];		/* name in process table */
  endpoint_t endpoint;			/* endpoint number when started */
  struct boot_image_memmap memmap;	/* memory map info for boot image */
};

#ifdef __kernel__
/* Variables that are initialized elsewhere are just extern here. */
extern struct boot_image image[]; 	/* system image processes */
#endif

#endif /* BOOTIMAGE_H */
