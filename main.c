#define _POSIX_SOURCE
#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>


#define  MAX_LEN      2048 // max length of user commands
#define  MAX_ARG      512 // max # of arguments
FILE *input;
char *commandArgs[MAX_ARG];
int numCmds = 0;
void expandVar(char *command);
void newChild();
int openPid[MAX_LEN] = {0};
int numProcesses = 1;
int backgroundCommands = 0;  // tracks non-built in processes that have been executed since parent shell started
int terminationStatus = 0; // last termination code
int inputFlag = 0;
int outputFlag = 0;
char *outputFileName = "\0";
char *inputFileName = "\0";



void newChild(){
    char *argsToRun[numCmds+1];
    int sourceFD;
    int targetFD;
    int result;
    int result2;
    for(int i = 0; i < numCmds+1; i++){  // create a copy of the arguments we need to run
        argsToRun[i] = commandArgs[i];
    }
    numCmds = 0;  // reset number of commands counter
    memset(commandArgs, 0, sizeof(commandArgs)); //clear buffer in case this is a background process
    pid_t spawnPid = -5;
    int childStatus;
    //fork new process
    spawnPid = fork();
    if(spawnPid > 0){
        numProcesses ++;
        openPid[numProcesses-1] = spawnPid;
    }
    switch(spawnPid){
        case -1:
            perror("fork error");
            exit(1);
        case 0:
            outputFileName = "\0";
            char *localInputName = inputFileName;
            char *localOutputName = outputFileName;
            inputFileName = "\0";
            if (strcmp(localInputName, "\0") !=0){ //need to open input file for input redirection
                // open source file
                fflush(stdin);
                sourceFD = open(localInputName, O_RDONLY);
                if (sourceFD == -1){
                    perror("source open()");
                    fflush(stderr);
                    exit(1);
                }
                // redirect stdin to source
                result = dup2(sourceFD, 0);
                if (result == -1){
                    perror("source dup2()");
                    fflush(stderr);
                    exit(1);
                }
//                close(sourceFD);
            }
            if (strcmp(localOutputName, "\0") !=0){ //need to open output file for output redirection
                // open target file
                fflush(stdout);
                targetFD = open(localOutputName, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (targetFD == -1){
                    perror("target open()");
                    fflush(stderr);
                    exit(1);
                }
                //redirect stdout to target
                result2 = dup2(targetFD, 1);
                if (result2 == -1){
                    perror("target dup2()");
                    fflush(stderr);
                    exit(1);
                }
//                close(targetFD);
            }
            if (execvp(argsToRun[0], argsToRun) == -1){
                perror("execvp");
                fflush(stderr);
                exit(1);
            }
            else {
                fflush(stdout);
                exit(0);
            }
        default:
            spawnPid = waitpid(spawnPid, &childStatus, 0);
            outputFileName = "\0";
            inputFileName = "\0";
            openPid[numProcesses-1] = '\0';
            numProcesses --;
            if (WIFEXITED(childStatus)){
                terminationStatus = WEXITSTATUS(childStatus);
//                if (terminationStatus != 0){
//                    fprintf(stderr, "abnormal child termination");
//                    fflush(stdout);
//                    exit(1);
//                }
            }
            if (WIFSIGNALED(childStatus)){
                if (WTERMSIG(childStatus) == SIGSEGV){
                    perror("sigsegv");
                    fflush(stdout);
                }
            }
            else{
                terminationStatus = WTERMSIG(childStatus);
            }
            break;
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
        char *inputBuff = NULL;
        size_t len = 0;
        openPid[0] = getpid();
        printf(": ");
        fflush(stdout);
        if(getline(&inputBuff, &len, stdin) == -1){
            if (ferror(stdin)) {
                perror("fgets error");
                exit(1);
            }
            if (feof(stdin)){
            }
            else {
                continue;
            }
        }
        inputBuff[strcspn(inputBuff, "\n")] = 0;
        char *temp;
        // remove new line from input with strcspn, code citation: https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
        if (strcmp(inputBuff[0], "#") == 0 || strcmp(inputBuff, "\n") == 0){
            free(inputBuff);
            continue;
        }
        // check if variable expansion is needed for $$
        if (strstr(inputBuff, "$$")){
            expandVar(inputBuff);
        }
        char *parsedInput;
        parsedInput = strtok(inputBuff, " ");
        int i = 0;
        // split space separated user input into an array of string literals holding each argument
        while (parsedInput != NULL){
            if (strcmp(parsedInput, "<") == 0){
                inputFlag ++;
                parsedInput = strtok(NULL, " ");
                continue;
            }
            if (strcmp(parsedInput, ">") == 0){
                outputFlag ++;
                parsedInput = strtok(NULL, " ");
                continue;
            }
            if (inputFlag != 0){
                inputFileName = parsedInput;
                inputFlag = 0;
                parsedInput = strtok(NULL, " ");
                if (outputFlag == 0) {  // if only input redirection and no output redirection
                    continue;
                }
            }
            if (outputFlag != 0){
                outputFileName = parsedInput;
                outputFlag = 0;
                parsedInput = strtok(NULL, " ");
                continue;
            }
            commandArgs[i] = parsedInput;
            parsedInput = strtok(NULL, " ");
            i++;
            numCmds ++;
        }
        if (strcmp(commandArgs[0], "cd") == 0) {
            if (numCmds> 2) {
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
                continue;
            }
            else{
                int length = snprintf(NULL, 0, "%d", terminationStatus);  // find length of status
                char *str = malloc(length + 1);
                snprintf(str, length+1, "%d", terminationStatus);  // convert status to string with correct length
                printf("%s", str);
            }
        }
        else{
            newChild();  // forks a new child process to execute commands
        }
        // check for <
        // check for >
        // check if last letter is &
        free(inputBuff);
    }
}


int main() {
    shell();  //start smallsh and parse arguments
    printf("Hello, World!\n");
    return 0;
}

