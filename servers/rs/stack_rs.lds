/* MINIX linker script, for executables with user-ajustable stacks */
SECTIONS
{
  .stack : {
	. = . + 1200K ;
	_stack_top = .;
  } :stack
}
