/* MINIX linker script, for executables with user-ajustable stacks */
SECTIONS
{
  .stack : {
	. = . + 32K ;
	_stack_top = .;
  } :stack
}
