CC=gcc
CFLAGS=-O3 -Wall -fopenmp
EXE=pi.exe

all : $(EXE)

$(EXE) : pi_sequentiel.o
	$(CC) $(CFLAGS) -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean :
	rm -f *.o $(EXE)

proper :
	rm -f *.o
