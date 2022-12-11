

all: print.o
	gcc -o writeonceFS writeonceFS.c print.o

print.o: print.c
	gcc -g -c -o print.o print.c