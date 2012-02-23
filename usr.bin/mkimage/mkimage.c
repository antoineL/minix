/*
 * Update physical addresses of boot services
 */

#include <sys/param.h>

#include <err.h>
#include <fcntl.h>

#include <gelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sysexits.h>
#include <unistd.h>
#include <getopt.h>

#define BOOTPROG_LOAD_START 0x01000000ULL

int nflag = 0;

int stack_kbytes[] = {
    /*  ds  rs    pm  sched  vfs  memory  log  tty  mfs  vm   pfs  init */
        16, 8125, 32, 32,    16,  8,      32,  16,  128, 128, 128, 64
};

static void usage(void);

GElf_Addr
update_paddr(int nr, char *fname, GElf_Addr startaddr)
{
	int i, fd;
	Elf *e;
	size_t n;

	GElf_Phdr phdr;
	GElf_Addr endaddr = 0;
	GElf_Addr offset;

	if ((fd = open(fname, O_RDWR, 0)) < 0)
		err(EX_NOINPUT, "open \"%s\" failed", fname);

	if ((e = elf_begin(fd, ELF_C_RDWR, NULL)) == NULL)
		errx(EX_SOFTWARE, "elf_begin() failed: %s.", elf_errmsg(-1));

	if (elf_kind(e) != ELF_K_ELF)
		errx(EX_DATAERR, "\"%s\" is not an ELF object.", fname);

	if (elf_getphdrnum(e, &n) != 0)
		errx(EX_DATAERR, "elf_getphdrnum() failed: %s.", elf_errmsg(-1));

	for (i = 0; i < n; i++) {
		if (gelf_getphdr(e, i, &phdr) != &phdr)
			errx(EX_SOFTWARE, "getphdr() failed: %s.",
			    elf_errmsg(-1));

		if (phdr.p_type == PT_LOAD) {
			if (endaddr == 0)
				offset = startaddr - phdr.p_vaddr;
			phdr.p_paddr = offset + phdr.p_vaddr;

			if (endaddr < phdr.p_paddr + phdr.p_memsz)
				endaddr = phdr.p_paddr + phdr.p_memsz;

			if (gelf_update_phdr(e, i, &phdr) < 0)
				errx(EX_SOFTWARE,
				    "gelf_update_phdr failed: %s.",
				    elf_errmsg(-1));
		}

	}

	if (elf_update(e, ELF_C_WRITE) < 0)
		errx(EX_SOFTWARE, "elf_update failed: %s.", elf_errmsg(-1));

	(void) elf_end(e);
	(void) close(fd);

	if (endaddr == 0)
		errx(EX_DATAERR, "\"%s\" has no PT_LOAD section", fname);

	return round_page(endaddr) + round_page(stack_kbytes[nr] * 1024);
}

int
main(int argc, char **argv)
{
	int i, ch;
	GElf_Addr startaddr;
	char * e;

	startaddr = BOOTPROG_LOAD_START;

	while ((ch = getopt(argc, argv, "nS:")) != -1) {
		switch (ch) {
		case 'n':
			nflag = 1;
			break;
		case 'S':
			startaddr = strtoul(optarg, &e, 0);
			if (errno || startaddr==ULONG_MAX || !e || *e) {
				usage();
				exit(EX_USAGE);
			}
			break;
		case '?':
		default:
			usage();
			exit(EX_USAGE);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if (elf_version(EV_CURRENT) == EV_NONE)
		errx(EX_SOFTWARE, "ELF library intialization failed: %s",
		    elf_errmsg(-1));

	for (i = 0; i < argc; i++) {
		startaddr = update_paddr(i, argv[i], startaddr);
	}

	exit(EX_OK);
}

static void
usage(void)
{
	(void) fprintf(stderr, "usage: %s [-n] [-S startaddr] elf1 elf2...\n", getprogname());
}
