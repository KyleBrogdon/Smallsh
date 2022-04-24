#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>


#define  MAX_LEN      2048 // max length of user commands
#define  MAX_ARG      512 // max # of arguments
char inputBuff[MAX_LEN];
FILE *input;
char *parsedInput;
char *commandArgs[MAX_ARG];


void shell() {
    input = stdin;
    // check for input redirecton
    fgets(inputBuff, MAX_LEN-1, input); // leave room for null terminate
    if (ferror(input)){
        perror("fgets error");
        exit (1);
    }
    // remove new line from input with strcspn, code citation: https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
    inputBuff[strcspn(inputBuff, "\n")] = 0;
    parsedInput = strtok(inputBuff, " ");
    int i = 0;
    // split space separated user input into an array holding arguments
    while (parsedInput != NULL){
        commandArgs[i] = parsedInput;
        parsedInput = strtok(NULL, " ");
        i++;
    }
}


int main() {
    shell();
    printf("Hello, World!\n");
    return 0;
}
