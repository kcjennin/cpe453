OS=$(shell uname -s)
ifeq ("$(OS)", "Darwin")
	OS_DEF=-DMACOSX -D__FreeBSD__=10
	FUSE_LINK=-lfuse
else
	OS_DEF=-DLINUX
	FUSE_LINK=-lfuse -lpthread
endif

CFLAGS=-Wall -Werror -D_FILE_OFFSET_BITS=64 $(OS_DEF)
CXXFLAGS=$(CFLAGS)

all: cpe453fs

cpe453fs: cpe453fs_main.o implementation.o smartalloc.o
	$(CXX) $(CXXFLAGS) cpe453fs_main.o implementation.o -o $@ $(FUSE_LINK)

cpe453fs_main.o: cpe453fs_main.c cpe453fs.h
implementation.o: implementation.cpp cpe453fs.h implementation.h
smartalloc.o: smartalloc.c smartalloc.h

clean:
	rm -f *.o cpe453fs hello_cpe453fs

unmount:
	fusermount -u /tmp/${USER}/mnt

unmount_ref:
	fusermount -u /tmp/${USER}/mnt_ref
