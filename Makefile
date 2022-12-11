

all: writeonceFS 
# print.o

writeonceFS: writeonceFS.o print.o
	cc -o writeonceFS print.o writeonceFS.o

writeonceFS.o: writeonceFS.c
	gcc -g -c -o writeonceFS.o writeonceFS.c


print.o: print.c
	gcc -g -c -o print.o print.c

clean:
	rm -rf writeonceFS print *.o *.a