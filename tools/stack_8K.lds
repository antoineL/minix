/* MINIX linker script, for executables with user-ajustable stacks */
SECTIONS
{
  .stack : {
	. = . + 8K ;
	_stack_top = .;
  } :stack
}
