/* MINIX linker script, for executables with user-ajustable stacks */
SECTIONS
{
  .stack : {
	. = . + 16K ;
	_stack_top = .;
  } :stack
}
