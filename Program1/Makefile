AS = gcc
CC = gcc

CFLAGS = -g -I. -fPIC -Wall -Werror
LDFLAGS = -L. -llwp

OBJS = smartalloc.o lwp.o magic64.o
DEPS = fp.h lwp.h smartalloc.h

TARGET = liblwp.a

all: $(TARGET)


# ASM files
%.o: %.S $(DEPS)
	$(AS) -c -o $@ $<

# C files
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	ar r $@ $^
	ranlib $@

nums: liblwp.a
	gcc -o nums numbersmain.c AlwaysZero.c liblwp.a -I. -g

rsnakes: liblwp.a
	gcc -o rsnakes randomsnakes.c liblwp.a libsnakes.a -I. -lncurses -g

hsnakes: liblwp.a
	gcc -o hsnakes hungrysnakes.c liblwp.a libsnakes.a -I. -lncurses -g

testing: liblwp.a
	gcc -o testing testing.c liblwp.a -I. -g

.PHONY: clean

clean:
	rm -f *.o $(TARGET) nums rsnakes hsnakes testing 2> /dev/null
