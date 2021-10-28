#include<string.h>
#include<stdlib.h>
#include<stdio.h>

int main(int argc, char *argv[]) {
    char *s;

    s = strdup("Tryme");

    s = realloc(s, strlen("Tryme") + 156);
    realloc(s, 0);

    return 0;
}