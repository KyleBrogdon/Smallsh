#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>


#define  MAX_LEN      2049 // max length of argument + null terminate

void shell() {
    FILE *input = stdin;
    char inputBuff[MAX_LEN];
    // check for input redirecton
    fgets(inputBuff, MAX_LEN, input);
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
