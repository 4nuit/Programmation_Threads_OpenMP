FILES = $(wildcard *.c)
OBJS = $(FILES:%.c=obj/%.o)
DEPS = $(FILES:%.c=dep/%.d)

MAKEFLAGS += --no-print-directory

ARCH = $(shell uname -p)

ifeq ($(ARCH),unknown)
	ARCH = $(shell uname -m)
endif

ifeq ($(ARCH),unknown)
	ARCH = $(shell arch)
endif

CC=gcc
CFLAGS=-Wall -D$(ARCH)_ARCH -D_REENTRANT -g -pipe -lpthread

all: lib/libmthread.a tests

lib/libmthread.a:$(OBJS)
	@echo "Generate $@"
	@ar rcs $@ obj/*.o

$(DEPS): dep/%.d: %.c
	@echo "Generate $@"
	@$(CC) $(CFLAGS) -I. -M $(patsubst dep/%.d,%.c,$@) | \
	sed 's,$(patsubst dep/%.d,%.o,$@):,$(patsubst %.d,%.o,$@) $@:,g' > $@ 2> /dev/null

$(OBJS): obj/%.o: dep/%.d
	@echo "Generate $@"
	@$(CC) $(CFLAGS) -I. -c $(patsubst obj/%.o,%.c,$@) -o $@

tests: $(OBJS)
	@echo "Generate $@.out"
	@$(CC) $(CFLAGS) -I. $(OBJS) -o $@.out

clean:
	@echo "Cleaning"
	@rm -f dep/* lib/* obj/* *~ *.out; printf ""

ifneq ($(MAKECMDGOALS),clean)
	-include $(DEPS)
endif
