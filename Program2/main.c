#include<string.h>
#include<stdlib.h>
#include<stdio.h>

int main(int argc, char *argv[]) {
    char *s, *s2, *s3;

    s = (char *)malloc(sizeof(char) * (2 << 15));
    s2 = (char *)malloc(sizeof(char) * (2 << 15));

    free(s);
    free(s2);

    s3 = (char *)malloc(sizeof(char) * (2 << 17));

    s3[(2 << 17) - 1] = 0;

    free(s3);

    return 0;
}