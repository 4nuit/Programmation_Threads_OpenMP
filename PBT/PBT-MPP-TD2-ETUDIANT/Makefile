CC = /usr/bin/gcc
GNUPTHDIR = ./pth-2.0.7/install
GNUPTHFLAGS = -I$(GNUPTHDIR)/include -L$(GNUPTHDIR)/lib -lpth
PTHREADFLAGS = -lpthread

all: q4 q5 q7 q8

q4:
	$(CC) -o $@.out $@.c $(GNUPTHFLAGS)

q5:
	$(CC) -o $@.out $@.c $(PTHREADFLAGS)

q7:
	$(CC) -o $@.out $@.c $(GNUPTHFLAGS)

q8:
	$(CC) -o $@.out $@.c $(PTHREADFLAGs)

clean:
	-rm *.out
