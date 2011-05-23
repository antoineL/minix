/* MINIX linker script, for executables with user-ajustable stacks */
SECTIONS
{
  .stack : {
	. = . + 64K ;
	_stack_top = .;
  } :stack
}
