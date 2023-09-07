CC=gcc
CFLAGS=-O3 -Wall -fopenmp
EXE1=pi_sequentiel.exe
EXE2=pi_parallele.exe

all : $(EXE1) $(EXE2)

$(EXE1) : pi_sequentiel.c
	$(CC) $(CFLAGS) -o $@ $<

$(EXE2) : pi_parallele.c
	$(CC) $(CFLAGS) -o $@ $<

clean :
	rm -f $(EXE1) $(EXE2)

proper :
	rm -f $(EXE1) $(EXE2)
