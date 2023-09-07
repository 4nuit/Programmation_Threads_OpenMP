CC=gcc
CFLAGS_OMP=-O3 -Wall -fopenmp -DUSE_OMP
EXE=tree-traverse.exe

all : $(EXE)

$(EXE) : tree-traverse.o
	$(CC) $(CFLAGS_OMP) -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS_OMP) -c -o $@ $<

clean : proper
	rm -f $(EXE)

proper :
	rm -f *.o *.dat
