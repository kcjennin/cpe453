CC = gcc

CFLAGS = -g -I. -fPIC -Wall #-Werror
LDFLAGS = -L. -lmalloc

OBJS = malloc.o main.o
DEPS = malloc.h

TARGETS = libmalloc.a libmalloc.so

all: malloc


# ASM files
%.o: %.S $(DEPS)
	$(AS) -c -o $@ $<

# C files
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

libmalloc.a: malloc.o
	ar r $@ $^
	ranlib $@

libmalloc.so: malloc.o
	gcc -shared $(CFLAGS) -o $@ $^

a.out: main.o
	gcc $^ $(LDFLAGS)

.PHONY: clean malloc library_set

malloc: $(TARGETS)

gdb:
	gdb -iex "set env LD_PRELOAD libmalloc.so" a.out

clean:
	rm -f *.o $(TARGETS) a.out 2> /dev/null
