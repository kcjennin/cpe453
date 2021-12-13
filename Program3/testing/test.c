#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

#define FILESIZE 600000

int main()
{
    int i, read_size, offset, fd, fd2, err, err2;
    char buf[FILESIZE];
    char buf2[FILESIZE];
    time_t t;

    srand((unsigned) time(&t));

    fd = open("/tmp/kcjennin/mnt/t1.txt", O_RDONLY);
    fd2 = open("/tmp/kcjennin/mnt_ref/t1.txt", O_RDONLY);
    for (i = 0; i < 1000000; i++)
    {
        read_size = rand() % FILESIZE;
        offset = rand() % FILESIZE;

        if(lseek(fd, offset, SEEK_SET) < 0)
        {
            perror("lseek");
            exit(1);
        }
        err = read(fd, buf, read_size);
        assert(err <= FILESIZE - offset);

        if(lseek(fd2, offset, SEEK_SET) < 0)
        {
            perror("lseek2");
            exit(1);
        }
        err2 = read(fd2, buf2, read_size);
        assert(err2 <= FILESIZE - offset);

        assert(err == err2);
        assert(memcmp(buf, buf2, err) == 0);
    }
    close(fd);
    close(fd2);

    return 0;
}