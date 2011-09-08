CC = exec cc

all: dstest subs .gitignore
	chmod +x down run

dstest: dstest.c inc.h
	$(CC) -o $@ $< -lsys

subs: subs.c inc.h
	$(CC) -o $@ $< -lsys

run: all
	sh run

kill:
	sh down

.gitignore: Makefile
	echo dstest subs | tr ' ' '\n' > $@

clean:
	rm -f dstest subs

