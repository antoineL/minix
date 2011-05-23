/* MINIX linker script, for executables with user-ajustable stacks */
SECTIONS
{
  .stack : {
	. = . + 128K ;
	_stack_top = .;
  } :stack
}
