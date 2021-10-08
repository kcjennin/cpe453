AS = gcc
CC = gcc

CFLAGS = -g -I. -fPIC -Wall -Werror
LDFLAGS = -L. -llwp

OBJS = smartalloc.o lwp.o magic64.o
DEPS = fp.h lwp.h smartalloc.h

TARGETS = liblwp.a liblwp.so

all: $(TARGETS)


# ASM files
%.o: %.S $(DEPS)
	$(AS) -c -o $@ $<

# C files
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

liblwp.so: $(OBJS)
	$(CC) $(CFLAGS) -shared -o $@ $^

liblwp.a: $(OBJS)
	ar r $@ $^

nums: liblwp.a liblwp.so AlwaysZero.o numbersmain.o
	$(CC) $(CFLAGS) -o nums numbersmain.o AlwaysZero.o -L. -llwp

.PHONY: clean

clean:
	rm -f *.o $(TARGETS) 2> /dev/null
