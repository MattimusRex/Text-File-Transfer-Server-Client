ftserver: ftserver.c
	gcc -o ftserver ftserver.c

all: ftserver

clean:
	rm -f ftserver
