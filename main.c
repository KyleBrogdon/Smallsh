#define _POSIX_SOURCE
#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stddef.h>


#define  MAX_LEN      2048          // max length of user commands
#define  MAX_ARG      512           // max # of arguments

void handleSIGTSTP(int signo);      // handles the SIGTSTP to turn on/off background commands
void catchSIGTSTP();                // catches a call of SIGTSTP and passes it to handleSIGTSTP
void ignoreSIGINT();                // sets the response to SIGINT to ignore
void defaultSIGINT();               // sets the response to SIGINT to terminate
void parseUserInput();              // parses user input and delimits by spaces into an array
void smallshCD();                   // handles change directory command inside smallsh
void smallshStatus();               // handles exit command inside smallsh
void shell();                       // handles user input and managing command flow and execution
void cleanUpBackground();           // reaps zombie processes that have terminated
void expandVar(char *command);      // function that expands && into the PID
void newChild();                    // function that spawns a child process to execute commands
char *commandArgs[MAX_ARG];         // holds strings containing commands entered by user
int numCmds = 0;                    // tracks number of separate commands entered each time user inputs
int openPid[MAX_LEN] = {0};         // tracks all open processes from this shell, background and foreground
int numProcesses = 1;               // starts at 1 with main process
int runningBackground[MAX_LEN];     // array that holds non-completed background proccses
int backgroundFlag = 0;             // flag that indicates whether current forked process is background
int terminationStatus = 0;          // holds termination code of last foreground process run by shell
int inputFlag = 0;                  // flag used to indicate user is redirecting input
int outputFlag = 0;                 // flag used to indicate user is redirecting output
int childCalled = 0;                // flag used to detect if a non-built in command has been run yet
char *outputFileName = "\0";        // string that holds the pathname to the redirected output
char *inputFileName = "\0";         // string that holds the pathname to the redirected input
char inputBuff[MAX_LEN];            // buffer that holds user input commands prior to parsing
int finishedBackground[MAX_LEN];    // array that holds completed background processes which need to be printed
int finishedStatus[MAX_LEN];        // array that holds exit status of finishedBackground processes in matching index
int finishedCount;                  // counter for processes which need to be printed
int ignoreBackground = 0;           // flag to indicate if SIGSTP was called



/**
 * Function: main
 *------------------------------
 * Sets the initial response to SIGTSTP and SIGINT, sets the parent PID to openPID index 0, and then calls shell which
 * handles executing user input commands. Once shell is finished running, one last check for zombie processes is
 * performed before main exits.
 *
 * Returns: None
 */
int main() {
    catchSIGTSTP();  // catches SIGSTP and redirects to handle
    ignoreSIGINT();  // parent process will ignore SIGINT
    openPid[0] = getpid();
    shell();  //start smallsh and parse arguments
    cleanUpBackground(); // cleanup any remaining background arguments
    return 0;
}
/**
 * Function: shell
 *------------------------------
 * Loops while true, first checking if any zombie processes need to be reaped via cleanUpBackground(), then getting user
 * input from stdin, validating user input was not a command or blank entry, parses user input and separates into
 * separate space delimited strings via parseUserInput(), storing the result in the array commandArgs. The commandArgs
 * are then evaluated for the built-in commands cd, exit, and status, before calling newChild to handle execution of
 * non-built in commands.
 *
 * Returns: None
 */
void shell() {
    while(1){
        memset(inputBuff, 0, sizeof(inputBuff));
        cleanUpBackground();
        printf(": ");
        fflush(stdout);
        if(fgets(inputBuff, MAX_LEN, stdin) == NULL){
            if (ferror(stdin)) {
                perror("fgets error");
                fflush(stderr);
                exit(1);
            }
            if (feof(stdin)){
            }
            else {
                continue;
            }
        }
        if (inputBuff[0] == '#' || inputBuff[0] == '\n'){  // if it's a comment or new line, ignore and prompt
            continue;
        }
        // remove new line from input with strcspn, code citation: https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
        inputBuff[strcspn(inputBuff, "\n")] = 0;
        // check if variable expansion is needed for $$
        // code citation strstr: https://www.tutorialspoint.com/c_standard_library/c_function_strstr.htm
        if (strstr(inputBuff, "$$")){
            expandVar(inputBuff);
        }
        parseUserInput();  // parse user input and separate into an array of commands
        // if it's cd, call smallshCD
        if (strcmp(commandArgs[0], "cd") == 0) {
            smallshCD();
            continue;
        }
        // starting from last child process, kill all including parent and exit smallsh
        if (strcmp(commandArgs[0], "exit") == 0) {
            for (; numProcesses > 0; numProcesses--) {
                kill(openPid[numProcesses - 1], SIGKILL);
            }
        }
        // if it's status, call smallshStatus
        if (strcmp(commandArgs[0], "status") == 0){
            smallshStatus();
            continue;
        }
        if (numCmds > 1) {
            if (strcmp(commandArgs[numCmds - 1], "&") == 0) {
                if (ignoreBackground == 0) {  // if SIGSTP has not been called, process background command
                    backgroundFlag = 1;
                }
                commandArgs[numCmds -1] = NULL; // remove & after setting background flag
                numCmds --;
            }
        }
        newChild();  // forks a new child process to execute commands
    }
}
/**
 * Function: newChild
 *------------------------------
 * Spawns off a new child process to execute commands by copying commandArgs to argsToRun, then passing argsToRun to
 * execvp and resets commandArgs.  Handles redirection for input and output via DUP2 in the child process. Stores
 * currently running foreground/background processes in appropriate arrays, updates open and running processes, and
 * resets values of outputFileName, inputFileName, and backgroundFlag when finished.
 *
 * Returns: None
 */
void newChild(){
    if (backgroundFlag == 0) {  // if this command is being run in the foreground
        childCalled = 1;
    }
    char *argsToRun[numCmds+1];         // duplicate array to hold commandArgs and null terminate
    int sourceFD;
    int targetFD;
    int result;
    int result2;
    for(int i = 0; i < numCmds+1; i++){  // create a copy of the arguments we need to run
        argsToRun[i] = commandArgs[i];
    }
    numCmds = 0;  // reset number of commands counter
    memset(commandArgs, 0, sizeof(commandArgs)); //clear buffer
    pid_t spawnPid = -5;
    int childStatus;
    //fork new process
    spawnPid = fork();
    if(spawnPid > 0) {
        numProcesses++;
        openPid[numProcesses - 1] = spawnPid;
        if (backgroundFlag == 1) {
            fprintf(stdout, "Background pid is %d\n", spawnPid);
            fflush(stdout);
            int currentBackgroundCount = 0;
            // get number of currently running background procceses to append to right index
            while (runningBackground[currentBackgroundCount] != 0) {
                currentBackgroundCount++;
            }
            runningBackground[currentBackgroundCount] = spawnPid;
        }
    }
    // code citation, exploration monitoring child processes example code https://canvas.oregonstate.edu/courses/1870063/pages/exploration-process-api-monitoring-child-processes?module_item_id=22026548
    switch(spawnPid) {
        case -1: {
            perror("fork error");
            exit(1);
        }
        // code citation for all uses of dup2/redirection, exploration I/O https://canvas.oregonstate.edu/courses/1870063/pages/exploration-processes-and-i-slash-o?module_item_id=22026557
        case 0: {
            if (backgroundFlag == 0){ // if child is a foreground process, must terminate on SIGINT and ignore SIGTSTPgit
                defaultSIGINT();
            }
            char *localOutputName = outputFileName;
            char *localInputName = inputFileName;
            if (strcmp(localInputName, "\0") != 0) { //need to open input file for input redirection
                // open source file
                sourceFD = open(localInputName, O_RDONLY);
                if (sourceFD == -1) {
                    fprintf(stderr, "Source open error");
                    fflush(stderr);
                    exit(1);
                }
                // redirect stdin to source
                result = dup2(sourceFD, 0);
                if (result == -1) {
                    exit(1);
                }
            }
            if (strcmp(localOutputName, "\0") != 0) { //need to open output file for output redirection
                // open target file
                targetFD = open(localOutputName, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (targetFD == -1) {
                    fprintf(stderr, "Target open error");
                    fflush(stderr);
                    exit(1);
                }
                //redirect stdout to target
                result2 = dup2(targetFD, 1);
                if (result2 == -1) {
                    exit(1);
                }
            }
            // if process is running in the background and either redirection wasn't specified, redirect to /dev/null
            if ((backgroundFlag == 1) & ((strcmp(localInputName, "\0") == 0) || (strcmp(localOutputName, "\0")) == 0)){
                if (strcmp(localInputName, "\0") == 0){
                    localInputName = "/dev/null";
                    // open source file
                    sourceFD = open(localInputName, O_RDONLY);
                    if (sourceFD == -1) {
                        fprintf(stderr, "Source open error");
                        fflush(stderr);
                        exit(1);
                    }
                    // redirect stdin to source
                    result = dup2(sourceFD, 0);
                    if (result == -1) {
                        exit(1);
                    }
                }
                if (strcmp(localOutputName, "\0") == 0){
                    localOutputName = "/dev/null";
                    // open target file
                    targetFD = open(localOutputName, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if (targetFD == -1) {
                        fprintf(stderr, "Target open error");
                        fflush(stderr);
                        exit(1);
                    }
                    //redirect stdout to target
                    result2 = dup2(targetFD, 1);
                    if (result2 == -1) {
                        exit(1);
                    }
                }
            }
            // code citation execvp usage, exploration executing new program https://canvas.oregonstate.edu/courses/1870063/pages/exploration-process-api-executing-a-new-program?module_item_id=22026549
            if (execvp(argsToRun[0], argsToRun) == -1){
                exit(1);
            } else {
                exit(0);
            }
        }
        // code citation, dealing with child termination status https://canvas.oregonstate.edu/courses/1870063/pages/exploration-process-api-creating-and-terminating-processes?module_item_id=22026547
        default: {
            if (backgroundFlag == 0) {  // if it was run in the foreground, wait for it
                spawnPid = waitpid(spawnPid, &childStatus, 0);
                openPid[numProcesses - 1] = '\0';
                numProcesses--;
                if (WIFEXITED(childStatus)) {
                    terminationStatus = WEXITSTATUS(childStatus);  // if exited normally, get termination status
                    if (terminationStatus != 0) {
                        if (strcmp(argsToRun[0], "test") != 0) {
                            fprintf(stderr, "Error: Exited with code %d \n", terminationStatus);
                            fflush(stderr);
                        }
                    }
                } else if (WIFSIGNALED(childStatus)) {
                    terminationStatus = WTERMSIG(childStatus);  // if abnormally, get termination status
                    fprintf(stdout, "Terminated by signal %d \n", terminationStatus);
                    }
            }
            outputFileName = "\0";  // reset outputFileName
            inputFileName = "\0";  // reset inputFileName
            backgroundFlag = 0;  // reset background flag
            return;
        }
    }

}
/**
 * Function: cleanUpBackground
 *------------------------------
 * Counts number of running background processes, checks to see if any child process is a zombie process and needs
 * to be reaped, updates termination status and arrays of opening/background processes, prints PID and termination
 * status of any reaped processes, and clears values of finishedBackground and finishedStatus.
 *
 * Returns: None
 */
void cleanUpBackground(){
    int currentBackgroundCount = 0;
    int j = 0;
    // get number of currently running background processes
    while (runningBackground[j] != 0){
        currentBackgroundCount++;
        j++;
    }
    if (currentBackgroundCount == 0){
        return;
    }
    // loop through and check if background processes have completed with WNOHANG
    for (int i = 0; i < currentBackgroundCount; i++){
        int childStatus;
        int backgroundPid = runningBackground[i];
        backgroundPid = waitpid(backgroundPid, &childStatus, WNOHANG);
        if (backgroundPid == 0){ // not ready to reap, continue
            continue;
        }
        else{ // process is ready to be reaped, find it in the array
            for (int k = 0; k < numProcesses; k++) {
                if (openPid[k] == backgroundPid) {
                    openPid[k] = '\0';
                    break;
                }
            }
            // store termination status for printing
            if (WIFEXITED(childStatus)) {
                terminationStatus = WEXITSTATUS(childStatus);
            } else {
                terminationStatus = WTERMSIG(childStatus);
            }
            finishedBackground[i] = backgroundPid;
            finishedStatus[i] = terminationStatus;
            for (int count = i; count < numProcesses; count++){
                runningBackground[count] = runningBackground[count+1];  // left shift array to remove finished
            }
            // clean up with array shift of runningprocesses
            finishedCount++;
            numProcesses--;
        }
    }
    // print finished PID and finished termination / exit status
    for(int i = 0; finishedCount > 0; finishedCount --){
        fprintf(stdout, "Background Process %d is finished - exit/termination status %d\n",
                finishedBackground[i], finishedStatus[i]);
        i++;
        fflush(stdout);
    }
    memset(finishedBackground, 0, sizeof(finishedBackground)); // reset finished background array
    memset(finishedStatus, 0, sizeof(finishedStatus));  // reset finished status array
}
/**
 * Function: expandVar
 *------------------------------
 * Takes a char array containing a string, loops through the char array, and replaces any instance of $$
 * with the PID of the calling process. The value in command is replaced with the new string.
 *
 * Returns: None
 */
void expandVar(char *command){
    // convert PID to a string for manipulation, code citation https://stackoverflow.com/questions/5242524/converting-int-to-string-in-c
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
    /** loop through and find next needle, then expand in place using memcpy and insertpointers to incrementally shift
     * the place in memory you are copying over. If pointer is ever null, then theres nothing else to expand and break
     */
    // loop through a string and replace instances of needle, code citation https://stackoverflow.com/questions/32413667/replace-all-occurrences-of-a-substring-in-a-string-in-c
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
    strncpy(command, buffer, strlen(buffer));  // copy back over the string at command
    free(str);
}


/**
 * Function: parseUserInput
 *------------------------------
 * Using strtok, parses through the inputBuff and stores space delimited strings in the commandArgs array and updates
 * the numCmds variable.
 *
 * Returns: None
 */
// code citations strtok: https://man7.org/linux/man-pages/man3/strtok_r.3.html
// code citations strtok: https://stackoverflow.com/questions/29977413/how-do-i-use-strtok-to-take-in-words-separated-by-white-space-into-a-char-array
void parseUserInput(){
    char *parsedInput;
    parsedInput = strtok(inputBuff, " ");
    int i = 0;
    // split space separated user input into an array of string literals holding each argument
    while (parsedInput != NULL){
        // if input redirection was entered, flag it for next strtok for inputFileName
        if (strcmp(parsedInput, "<") == 0){
            inputFlag ++;
            parsedInput = strtok(NULL, " ");
            continue;
        }
        // if output redirection was entered, flag it for next strtok for outputFileName
        if (strcmp(parsedInput, ">") == 0){
            outputFlag ++;
            parsedInput = strtok(NULL, " ");
            continue;
        }
        // current strtok value is redirected input
        if (inputFlag != 0){
            inputFileName = parsedInput;
            inputFlag = 0;
            parsedInput = strtok(NULL, " ");
            if (outputFlag == 0) {  // if only input redirection and no output redirection
                continue;
            }
        }
        // current strtok value is redirected output
        if (outputFlag != 0){
            outputFileName = parsedInput;
            outputFlag = 0;
            parsedInput = strtok(NULL, " ");
            continue;
        }
        commandArgs[i] = parsedInput;          // add string to array
        parsedInput = strtok(NULL, " ");         // grab next value, increment
        i++;
        numCmds ++;
    }
}

/**
 * Function: smallshCD
 *------------------------------
 * Changes directory to the pathname provided in commandArgs. If >2 commands, error is printed to user and command is
 * restored to shell for additional input. If 2 commands provided, cd to the second command using chdir. If only cd is
 * entered, cd to the directory provided by the HOME environement variable.
 *
 * Returns: None
 */
void smallshCD(){
        if (numCmds> 2) {
            perror("invalid number of arguments");
            numCmds = 0;
            memset(commandArgs, 0, sizeof(commandArgs));
            return;
        }
        else if (numCmds == 2){
            // if cd & was entered, ignore & and go to home
            if (strcmp(commandArgs[1], "&") == 0){
                commandArgs[1] = NULL;
                goto cdHome;
            }
            int dir = chdir(commandArgs[1]);
            if (dir != 0) {
                perror("chdir error");
                exit(1);
            }
            numCmds = 0;
            memset(commandArgs, 0, sizeof(commandArgs));
            return;
        }
        else {
            cdHome:
            {  // only cd was input by user or extranous &
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
                numCmds = 0;
                memset(commandArgs, 0, sizeof(commandArgs)); //clear buffer
                return;
            }
        }
}

/**
 * Function: smallshStatus
 *------------------------------
 * Clears the commandArgs array, then evaluates if a child foreground process has not been called. If has not been
 * called then reset numCmds to 0 and return. Else, print the termination status of the last non-built in command that
 * was run.
 *
 * Returns: None
 */
void smallshStatus(){
    memset(commandArgs, 0, sizeof(commandArgs)); //clear buffer
    if (childCalled == 0){
        numCmds = 0;
        return;
    }
    else{
        int length = snprintf(NULL, 0, "%d", terminationStatus);  // find length of status
        char *str = malloc(length + 1);
        snprintf(str, length+1, "%d", terminationStatus);  // convert status to string with correct length
        printf("exit value %s \n", str);
        fflush(stdout);
        free (str);
        numCmds = 0;
        return;
    }
}

// sets response to SIGINT to terminate
void defaultSIGINT(){
    struct sigaction SIGINTdefault = {0};
    SIGINTdefault.sa_handler = SIG_DFL;
    sigfillset(&SIGINTdefault.sa_mask);
    SIGINTdefault.sa_flags = 0;
    sigaction(SIGINT, &SIGINTdefault, NULL);
}

// sets response to SIGINT to ignore
void ignoreSIGINT(){
    struct sigaction SIGINTignore = {0};
    SIGINTignore.sa_handler = SIG_IGN;
    sigfillset(&SIGINTignore.sa_mask);
    SIGINTignore.sa_flags = 0;
    sigaction(SIGINT, &SIGINTignore, NULL);
}

// handles SIGTSTP by toggling background process commands and printing appropriate message to user
void handleSIGTSTP(int signo){
    pid_t callingPid = getpid();
    if (callingPid == openPid[0] && ignoreBackground == 0){
        ignoreBackground = 1;
        char * message = "Ignoring background process commands \n";
        write(STDOUT_FILENO, message, strlen(message));
    }
    else if (callingPid == openPid[0] && ignoreBackground == 1) {
        ignoreBackground = 0;
        char *message = "Background commands restored \n";
        write(STDOUT_FILENO, message, strlen(message));
    }
}

// catches SIGTSTP and passes it off to handler
void catchSIGTSTP(){
    struct sigaction SIGTSTPdefault;
    SIGTSTPdefault.sa_handler = handleSIGTSTP;
    sigfillset(&SIGTSTPdefault.sa_mask);
    SIGTSTPdefault.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTPdefault, NULL);
}
