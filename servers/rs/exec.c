#include "inc.h"
#include <assert.h>
#include <libexec.h>

#if DEAD_CODE
#define BLOCK_SIZE	1024
#endif

static int do_exec(int proc_e, const char *exec, size_t exec_len, char *progname,
	char *frame, int frame_len);
static int exec_newmem(int proc_e, vir_bytes text_addr,
	vir_bytes text_bytes, vir_bytes data_addr,
	vir_bytes data_bytes, vir_bytes tot_bytes,
	vir_bytes frame_len, int sep_id, int is_elf,
	dev_t st_dev, ino_t st_ino, time_t st_ctime, char *progname,
	int new_uid, int new_gid, vir_bytes *stack_topp,
	int *load_textp, int *allow_setuidp);
static void patch_ptr(char stack[ARG_MAX], vir_bytes base);
static int exec_restart(int proc_e, int result, vir_bytes pc);
static int read_seg(const char *image, size_t image_len, off_t off,
        int proc_e, int seg, vir_bytes seg_addr, phys_bytes seg_bytes);

/*===========================================================================*
 *				srv_execve				     *
 *===========================================================================*/
int srv_execve(int proc_e, char *exec, size_t exec_len, char **argv,
	char **Xenvp)
{
	char * const *ap;
	char * const *ep;
	char *frame;
	char **vp;
	char *sp, *progname;
	size_t argc;
	size_t frame_size;
	size_t string_off;
	size_t n;
	int ov;
	int r;

	/* Assumptions: size_t and char *, it's all the same thing. */

	/* Create a stack image that only needs to be patched up slightly
	 * by the kernel to be used for the process to be executed.
	 */

	ov= 0;			/* No overflow yet. */
	frame_size= 0;		/* Size of the new initial stack. */
	string_off= 0;		/* Offset to start of the strings. */
	argc= 0;		/* Argument count. */

	for (ap= argv; *ap != NULL; ap++) {
		n = sizeof(*ap) + strlen(*ap) + 1;
		frame_size+= n;
		if (frame_size < n) ov= 1;
		string_off+= sizeof(*ap);
		argc++;
	}

	/* Add an argument count and two terminating nulls. */
	frame_size+= sizeof(argc) + sizeof(*ap) + sizeof(*ep);
	string_off+= sizeof(argc) + sizeof(*ap) + sizeof(*ep);

	/* Align. */
	frame_size= (frame_size + sizeof(char *) - 1) & ~(sizeof(char *) - 1);

	/* The party is off if there is an overflow. */
	if (ov || frame_size < 3 * sizeof(char *)) {
		errno= E2BIG;
		return -1;
	}

	/* Allocate space for the stack frame. */
	frame = (char *) malloc(frame_size);
	if (!frame) {
		errno = E2BIG;
		return -1;
	}

	/* Set arg count, init pointers to vector and string tables. */
	* (size_t *) frame = argc;
	vp = (char **) (frame + sizeof(argc));
	sp = frame + string_off;

	/* Load the argument vector and strings. */
	for (ap= argv; *ap != NULL; ap++) {
		*vp++= (char *) (sp - frame);
		n= strlen(*ap) + 1;
		memcpy(sp, *ap, n);
		sp+= n;
	}
	*vp++= NULL;

#if 0
	/* Load the environment vector and strings. */
	for (ep= envp; *ep != NULL; ep++) {
		*vp++= (char *) (sp - frame);
		n= strlen(*ep) + 1;
		memcpy(sp, *ep, n);
		sp+= n;
	}
#endif
	*vp++= NULL;

	/* Padding. */
	while (sp < frame + frame_size) *sp++= 0;

	(progname=strrchr(argv[0], '/')) ? progname++ : (progname=argv[0]);
	r = do_exec(proc_e, exec, exec_len, progname, frame, frame_size);

	/* Return the memory used for the frame and exit. */
	free(frame);
	return r;
}

/*===========================================================================*
 *				do_exec					     *
 *===========================================================================*/
static int do_exec(int proc_e, const char *exec, size_t exec_len, char *progname,
	char *frame, int frame_len)
{
	int r;
	vir_bytes vsp, tot_bytes;
	struct image_memmap emap;	/* Map read from executable */ 
	vir_bytes pc;			/* Entry point of exec file */
	vir_bytes stack_top;		/* Top of the stack */
	int sep_id, is_elf, load_text, allow_setuid;
	uid_t new_uid;
	gid_t new_gid;

	/* Read the file header and extract the segment sizes. */
	r = exec_memmap(exec, exec_len, &emap);
	/* Recognized the format but for another architecture */
	if (r == OK && emap.machine != EXEC_TARGET_MACHINE) r=ENOEXEC;

	/* No exec loader could load the object */
	if (r != OK) {
		printf("RS: do_exec: error reading header%d\n", r);
		return r;
	}

	pc = emap.entry;
	if (emap.stack_bytes > 0)
		tot_bytes = emap.top_alloc;
	else
		tot_bytes = 0; /* Use default stack size */
	sep_id = emap.nr_regions > 1;
#if TEMP_CODE
	is_elf = emap.text_.vaddr > 0;
#endif
	new_uid= getuid();
	new_gid= getgid();

	/* XXX what should we use to identify the executable? */

#if TEMP_CODE
	r = exec_newmem(proc_e,
		  trunc_page(emap.text_.vaddr), emap.text_.membytes,
		  trunc_page(emap.data_.vaddr), emap.data_.membytes,
		  tot_bytes, frame_len, sep_id, is_elf,
		  0 /*dev*/, proc_e /*inum*/, 0 /*ctime*/,
		  progname, new_uid, new_gid,
		  &stack_top, &load_text, &allow_setuid);
#else
	r = exec_newmem(proc_e, progname, &emap, frame_len, new_uid, new_gid,
		        0 /*dev*/, proc_e /*inum*/, 0 /*ctime*/,
		        &stack_top, &load_text, &allow_setuid);
#endif
	if (r != OK) {
		printf("RS: do_exec: exec_newmem failed: %d\n", r);
		exec_restart(proc_e, r, pc);
		return r;
	}

	/* Read in text and data segments. */
	if (load_text) {
		r = read_seg(exec, exec_len, emap.text_.fileoffset, proc_e,
		             T, emap.text_.vaddr, emap.text_.filebytes);
		if (r != OK) {
			printf("RS: do_exec: read_seg(T) failed: %d\n", r);
			exec_restart(proc_e, r, pc);
			return r;
		}
	}
	else
		printf("RS: do_exec: not loading text segment\n");
	
	r = read_seg(exec, exec_len, emap.data_.fileoffset, proc_e,
	             D, emap.data_.vaddr, emap.data_.filebytes);
	if (r != OK) {
	      printf("RS: do_exec: read_seg(D) failed: %d\n", r);
	      exec_restart(proc_e, r, pc);
	      return r;
	}

	/* Patch up stack and copy it from RS to new core image. */
	vsp = stack_top;
	vsp -= frame_len;
	patch_ptr(frame, vsp);
	r = sys_datacopy(SELF, (vir_bytes) frame,
		proc_e, (vir_bytes) vsp, (phys_bytes)frame_len);
	if (r != OK) {
		printf("RS: stack_top is 0x%lx; tried to copy to 0x%lx in %d\n",
			stack_top, vsp, proc_e);
		printf("do_exec: copying out new stack failed: %d\n", r);
		exec_restart(proc_e, r, pc);
		return r;
	}

	return exec_restart(proc_e, OK, pc);
}

/*===========================================================================*
 *				exec_newmem				     *
 *===========================================================================*/
static int exec_newmem(
  int proc_e,
  vir_bytes text_addr,
  vir_bytes text_bytes,
  vir_bytes data_addr,
  vir_bytes data_bytes,
  vir_bytes tot_bytes,
  vir_bytes frame_len,
  int sep_id,
  int is_elf,
  dev_t st_dev,
  ino_t st_ino,
  time_t st_ctime,
  char *progname,
  int new_uid,
  int new_gid,
  vir_bytes *stack_topp,
  int *load_textp,
  int *allow_setuidp
)
{
	int r;
	struct exec_newmem e;
	message m;

	e.text_addr = text_addr;
	e.text_bytes= text_bytes;
	e.data_addr = data_addr;
	e.data_bytes= data_bytes;
	e.tot_bytes= tot_bytes;
	e.args_bytes= frame_len;
	e.sep_id= sep_id;
	e.is_elf= is_elf;
	e.st_dev= st_dev;
	e.st_ino= st_ino;
	e.st_ctime= st_ctime;
	e.new_uid= new_uid;
	e.new_gid= new_gid;
	strncpy(e.progname, progname, sizeof(e.progname)-1);
	e.progname[sizeof(e.progname)-1]= '\0';

	m.m_type= EXEC_NEWMEM;
	m.EXC_NM_PROC= proc_e;
	m.EXC_NM_PTR= (char *)&e;
	r= sendrec(PM_PROC_NR, &m);
	if (r != OK)
		return r;
#if DEBUG_CODE
	printf("exec_newmem: r = %d, m_type = %d\n", r, m.m_type);
#endif
	*stack_topp= m.m1_i1;
	*load_textp= !!(m.m1_i2 & EXC_NM_RF_LOAD_TEXT);
	*allow_setuidp= !!(m.m1_i2 & EXC_NM_RF_ALLOW_SETUID);
#if DEBUG_CODE
	printf("RS: exec_newmem: stack_top = 0x%x\n", *stack_topp);
	printf("RS: exec_newmem: load_text = %d\n", *load_textp);
#endif
	return m.m_type;
}


/*===========================================================================*
 *				exec_restart				     *
 *===========================================================================*/
static int exec_restart(int proc_e, int result, vir_bytes pc)
{
	int r;
	message m;

	m.m_type= EXEC_RESTART;
	m.EXC_RS_PROC= proc_e;
	m.EXC_RS_RESULT= result;
	m.EXC_RS_PC= (void*)pc;
	r= sendrec(PM_PROC_NR, &m);
	if (r != OK)
		return r;
	return m.m_type;
}

/*===========================================================================*
 *				patch_ptr				     *
 *===========================================================================*/
static void patch_ptr(
char stack[ARG_MAX],		/* pointer to stack image within PM */
vir_bytes base			/* virtual address of stack base inside user */
)
{
/* When doing an exec(name, argv, envp) call, the user builds up a stack
 * image with arg and env pointers relative to the start of the stack.  Now
 * these pointers must be relocated, since the stack is not positioned at
 * address 0 in the user's address space.
 */

  char **ap, flag;
  vir_bytes v;

  flag = 0;			/* counts number of 0-pointers seen */
  ap = (char **) stack;		/* points initially to 'nargs' */
  ap++;				/* now points to argv[0] */
  while (flag < 2) {
	if (ap >= (char **) &stack[ARG_MAX]) return;	/* too bad */
	if (*ap != NULL) {
		v = (vir_bytes) *ap;	/* v is relative pointer */
		v += base;		/* relocate it */
		*ap = (char *) v;	/* put it back */
	} else {
		flag++;
	}
	ap++;
  }
}

/*===========================================================================*
 *				read_seg				     *
 *===========================================================================*/
static int read_seg(
  const char *image,		/* executable image */
  size_t image_len,		/* size of executable image */
  off_t off,			/* offset in image (file) */
  int proc_e,			/* process number (endpoint) */
  int seg,			/* T, D, or S */
  vir_bytes seg_addr,		/* address to load segment */
  phys_bytes seg_bytes		/* how much is to be transferred? */
)
{
/*
 * The byte count on read is usually smaller than the segment count, because
 * a segment is padded out to a click multiple, and the data segment is only
 * partially initialized.

 * CHECKME: does it still apply?
 */
  int r;

  assert((seg == T)||(seg == D));

  if (off+seg_bytes > image_len) return ENOEXEC;
  r= sys_vircopy(SELF, D, ((vir_bytes)image)+off,
                 proc_e, seg, seg_addr, seg_bytes);
  return r;
}
