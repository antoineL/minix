OUTPUT_ARCH("i386")
ENTRY(MINIX)
PHDRS {
  text PT_LOAD FILEHDR PHDRS ;
  data PT_LOAD ;
}
SECTIONS {
	BASE = 0;	/* Base virtual address */
	LOAD_ADDR = 0x0100000;	/* Where it is laoded (hint) */

	. = SIZEOF_HEADERS + BASE;
	_text = .;
	PROVIDE( begtext = . );
	.text.head . : AT(ADDR(.text.head) - BASE + LOAD_ADDR) {
		*(.text.head)
	} :text
	.text . : AT(ADDR(.text) - BASE + LOAD_ADDR) {
		*(.text)
		*(.text.*)
		. = ALIGN(16); /* cache line */
	} :text
	_etext = .;
	PROVIDE( etext = . );

/*	. = ALIGN(4096);	FIXME! this does not work on VMWare... */
	_data = .;
	PROVIDE(begdata = . );
	.data . : AT(ADDR(.data) - BASE + LOAD_ADDR) {
		_rodata = .;
		/* kernel data starts with this magic number */
		/* SHORT(0x526f); already put there in mpx.S */
		*(.rodata)
		*(.rodata.*)
		_erodata = .;
		*(.data)
		*(.data.*)
	} :data
	_edata = .;

	_bss = .;
	PROVIDE(begbss = . );
	.bss . : AT(ADDR(.bss) - BASE + LOAD_ADDR) {
		*(.bss)
		*(.bss.*)
		*(COMMON)
	}
	_ebss = .;
	PROVIDE(endbss = . );
	/* no stack for kernel */
	_end = .;
	PROVIDE(end = . );

	/DISCARD/ :
	{
		*(.eh_frame)
		*(.comment)
		*(.comment.*)
		*(.note)
		*(.note.*)
	}
}
