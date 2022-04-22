#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>


void shell() {
    FILE *input = stdin;
    char inputBuff[2049];
    // check for input redirecton
    while (fgets(inputBuff, 2048, input) != NULL);
    if (ferror(input)){
        perror("fgets error");
        exit (1);
    }
}


int main() {
    shell();
    printf("Hello, World!\n");
    return 0;
}
