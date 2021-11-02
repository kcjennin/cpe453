#include<string.h>
#include<stdlib.h>
#include<stdio.h>

int main(int argc, char *argv[]) {
    char *s, *s2;

    s = (char *)malloc(sizeof(char) * (2 << 15));
    s2 = (char *)malloc(sizeof(char) * (2 << 15));

    free(s);
    free(s2);

    return 0;
}