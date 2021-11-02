#include<string.h>
#include<stdlib.h>
#include<stdio.h>

int main(int argc, char *argv[]) {
    char *s[1000000];
    int i;

    for(i = 0; i < 1000000; i++)
    {
        s[i] = malloc(1024);
    }

    for(i = 0; i < 1000000; i++)
    {
        free(s[i]);
    }

    return 0;
}