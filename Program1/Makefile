AS = gcc
CC = gcc

CFLAGS = -g -I. -fPIC -Wall -Werror
LDFLAGS = -L. -llwp

OBJS = smartalloc.o lwp.o magic64.o
DEPS = fp.h lwp.h smartalloc.h

TARGETS = liblwp.a

all: $(TARGETS)


# ASM files
%.o: %.S $(DEPS)
	$(AS) -c -o $@ $<

# C files
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

liblwp.a: $(OBJS)
	ar r $@ $^
	ranlib $@

nums: liblwp.a
	gcc -o nums numbersmain.c AlwaysZero.c liblwp.a -I.

rsnakes: liblwp.a
	gcc -o rsnakes randomsnakes.c liblwp.a libsnakes.a -I. -lncurses -L. -lsnakes

.PHONY: clean

clean:
	rm -f *.o $(TARGETS) nums 2> /dev/null
