all: test-scancodes

clean:
	rm -f *.o test-scancodes

test-scancodes: test-scancodes.o .gitignore

test-scancodes.o: test-scancodes.c

.gitignore: Makefile
	echo test-scancodes | tr ' ' '\n' > $@

