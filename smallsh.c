#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>


#define  MAX_LEN      2048 // max length of user commands
#define  MAX_ARG      512 // max # of arguments
char inputBuff[MAX_LEN];
FILE *input;
char *parsedInput;
char *commandArgs[MAX_ARG];
int numCmds = 0;
void expandVar(char *command);
int newChild();
int openPid[MAX_LEN] = {0};
int numProcesses = 1;
int backgroundCommands = 0;  // tracks non-built in processes that have been executed since parent shell started
int terminationStatus = 0; // last termination code


int newChild(){
    pid_t spawnPid = -5;
    int childStatus;
    //fork new process
    spawnPid = fork();
    switch(spawnPid){
        case -1:
            perror("fork error");
            exit(1);
        case 0:
            openPid[numProcesses] = getpid();
            numProcesses ++;
            if (execvp(commandArgs[0], commandArgs) == -1){
                perror("execvp command error");
                exit(1);
            }
            else {
                exit(0);
            }
        default:
            spawnPid = waitpid(spawnPid, &childStatus, 0);
            if (WIFEXITED(childStatus)){
                terminationStatus = WEXITSTATUS(childStatus);
            }
            else{
                terminationStatus = WTERMSIG(childStatus);
            }
            exit(0);
    }

}

void expandVar(char *command){
    // convert PID to a string for manipulation, code citation https://stackoverflow.com/questions/5242524/converting-int-to-string-in-c
    // loop through a string and replace instances of needle, code citation https://stackoverflow.com/questions/32413667/replace-all-occurrences-of-a-substring-in-a-string-in-c
    char *tmp = command;
    char buffer[MAX_LEN] = {0};
    int pid = getpid();
    int length = snprintf(NULL, 0, "%d", pid);  // find length of pid
    char *str = malloc(length + 1);
    snprintf(str, length+1, "%d", pid);  // convert pid to string with correct length
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


//implement status

void shell() {
    while(1){
        input = stdin;
        // check for input redirecton
        openPid[0] = getpid();
        printf(": ");
        fflush(stdout);
        fgets(inputBuff, MAX_LEN-1, input); // leave room for null terminate
        if (ferror(input)){
            perror("fgets error");
            exit (1);
        }
        // remove new line from input with strcspn, code citation: https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
        inputBuff[strcspn(inputBuff, "\n")] = 0;
        if (inputBuff[0] == '#' || inputBuff[0] == '\n'){
            memset(inputBuff, 0, sizeof(inputBuff));
            continue;
        }
        // check if variable expansion is needed for $$
        if (strstr(inputBuff, "$$")){
            expandVar(inputBuff);
        }
        parsedInput = strtok(inputBuff, " ");
        int i = 0;
        // split space separated user input into an array of string literals holding each argument
        while (parsedInput != NULL){
            commandArgs[i] = parsedInput;
            parsedInput = strtok(NULL, " ");
            i++;
            numCmds ++;
        }
        if (strcmp(commandArgs[0], "cd") == 0) {
            if (numCmds > 2) {
                perror("invalid number of arguments");
                exit(1);
            } else if (numCmds == 1) {
                char *home = getenv("HOME");
                if (home == NULL) {
                    perror("getenv error");
                    exit(1);
                }
                int dir = chdir(home);

                if (dir != 0) {
                    perror("chdir error");
                    exit(1);
                }
            } else {
                int dir = chdir(commandArgs[1]);
                if (dir != 0) {
                    perror("chdir error");
                    exit(1);
                }
            }
        }
        // starting from last child process, kill all including parent and exit smallsh
        else if (strcmp(commandArgs[0], "exit") == 0) {
            for (; numProcesses > 0; numProcesses--) {
                kill(openPid[numProcesses - 1], SIGKILL);
            }
        }
        else if (strcmp(commandArgs[0], "status") == 0){
            if (backgroundCommands == 0){
                exit(0);
            }
            else{
                int length = snprintf(NULL, 0, "%d", terminationStatus);  // find length of status
                char *str = malloc(length + 1);
                snprintf(str, length+1, "%d", terminationStatus);  // convert status to string with correct length
                printf("%s", str);
            }
        }
        else{
            break;
        }
        // check for <
        // check for >
        // check if last letter is &
        // check for blank input
        // check for #
    }
}


int main() {
    shell();  //start smallsh and parse arguments
    newChild();  // forks a new child process to execute commands
    printf("Hello, World!\n");
    return 0;
}
