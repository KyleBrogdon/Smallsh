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



void expandVar(char *command){
    // convert PID to a string for manipulation, code citation https://stackoverflow.com/questions/5242524/converting-int-to-string-in-c
    // loop through a string and replace instances of needle, code citation https://stackoverflow.com/questions/32413667/replace-all-occurrences-of-a-substring-in-a-string-in-c
    char *tmp = command;
    char buffer[MAX_LEN] = {0};
    int pid = getpid();
    int length = snprintf(NULL, 0, "%d", pid);  // find length of pid
    char *str = malloc(length + 1);
    snprintf(str, length+1, "%d", pid);  // convert pid to string
    char *needle = "$$";
    size_t needle_len = strlen(needle);
    size_t replace_len = strlen(str);
    char *insert_point = &buffer[0];
    while(1){
        char *pointer = strstr(tmp, needle);
        if (pointer == NULL){
            strcpy(insert_point, tmp);
            break;
        }
        memcpy(insert_point, tmp, pointer - tmp);
        insert_point += pointer - tmp;
        memcpy(insert_point, str, replace_len);
        insert_point += replace_len;
        tmp = pointer + needle_len;
    }
    buffer[strlen(buffer)] = '\0';
    strncpy(command, buffer, strlen(buffer));
    free(str);
}

void shell() {
    while(1){
        input = stdin;
        // check for input redirecton
        //
        printf(": ");
        fflush(stdout);
        fgets(inputBuff, MAX_LEN-1, input); // leave room for null terminate
        if (ferror(input)){
            perror("fgets error");
            exit (1);
        }
        // remove new line from input with strcspn, code citation: https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
        inputBuff[strcspn(inputBuff, "\n")] = 0;
        // check if variable expansion is needed for $$
        if (strstr(inputBuff, "$$")){
            expandVar(inputBuff);
        }
        parsedInput = strtok(inputBuff, " ");
        int i = 0;
        // split space separated user input into an array holding arguments
        while (parsedInput != NULL){
            commandArgs[i] = parsedInput;
            parsedInput = strtok(NULL, " ");
            i++;
        }
        // check for <
        // check for >
        // check if last word is &
        // check for &&
        // check for blank input
        // check for #
        // implement cd
        // implement exit
        // implement status
        break;
    }
}


int main() {
    shell();
    printf("Hello, World!\n");
    return 0;
}
