IDIR=../include
LDIR=../lib
ODIR=obj

CC=gcc
CFLAGS=-I$(IDIR) -g -Wall -ansi
LIBS=-lPLN

_DEPS = lwp.h fp.h smartalloc.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = lwp.o smartalloc.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

OUT = a1.out

.PHONY: all clean test

all: $(OUT)

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(OUT): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ $(OUT)