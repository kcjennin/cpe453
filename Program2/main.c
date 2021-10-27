#include <string.h>
#include "malloc.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
    char *s;

    s = strdup("Tryme");
    puts(s);
    free(s);
    return 0;
}