CC = exec cc

PROG =	requestor grantor 1fifo 2fifo

all: $(PROG)
	chmod +x down run

requestor: requestor.c inc.h .gitignore
	$(CC) -o $@ $< -lsys

grantor: grantor.c inc.h .gitignore
	$(CC) -o $@ $< -lsys

1fifo 2fifo: .gitignore
	mkfifo $@

run: all
	sh run

.gitignore: Makefile
	echo $(PROG) | tr ' ' '\n' > $@


kill:
	sh down

clean:
	rm -f grantor requestor 1fifo 2fifo

