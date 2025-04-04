#include "myshell.h"

int main() {
    // Variables
    char cmdline[MAX_LINE], *args[MAX_LENGTH];

    do {
        // Initialize
        memset(cmdline, '\0', MAX_LINE);
        memset(args, 0, MAX_LENGTH * sizeof(char*));
        fflush(stdin);
        fflush(stdout);

        // Shell Prompt: print your prompt
        fputs("CSE4100-SP-P2> ", stdout);

        // Reading: Read the command from standard input
        myshell_readInput(cmdline);

        // Parsing: Parse the command
        myshell_parseInput(cmdline, args);
        
        // Execute the command
        if(!strcmp(args[0], "exit"))
            exit(EXIT_SUCCESS);
        else if(!strcmp(args[0], "cd"))
        {
            // Change directory
            if(args[1] == NULL) {
                // Change to home directory
                char *home = getenv("HOME");
                if (home == NULL) {
                    fprintf(stderr, "cd: HOME not set\n");
                    continue;
                }
                chdir(home);
            }
            else {
                // Change to specified directory
                if(chdir(args[1]) < 0) {
                    fprintf(stderr, "cd: %s: No such file or directory\n", args[1]);
                    continue;
                }
            }
            continue;
        }
        myshell_execCommand(args);

    } while(1);

    return 0;
}

/* This function reads input from the command line*/
void myshell_readInput(char *buf) {
    fgets(buf, MAX_LINE, stdin);
}

void myshell_parseInput(char *buf, char **args) {
    // Variables
    char buffer[MAX_LINE];
    char *token[MAX_LENGTH];

    // Copy the buffer
    strcpy(buffer, buf);

    // Tokenize the buffer
    buffer[strlen(buffer)-1] = ' ';
    token[0] = strtok(buffer, " ");
    int i = 0;
    while(token[i] != NULL) {
        i++;
        token[i] = strtok(NULL, " ");
    }

    // Copy the tokens to args
    for(int j = 0; j < i; j++) {
        args[j] = token[j];
        args[j][strlen(args[j])] = '\0';
    }
    args[i] = NULL;
}

void myshell_execCommand(char **args) {

    // Variables
    pid_t pid;
    int status;

    // Fork the process
    pid = fork();

    // Child Process
    if(pid == 0) {
        // Execute the command
        if (execvp(args[0], args) < 0) {
            fprintf(stderr, "%s: Command not found\n", args[0]);
            exit(EXIT_FAILURE);
        }
    }
    // Parent Process
    else {
        // Wait for the child process to terminate
        waitpid(pid, &status, 0);
        return;
    }

    // Check for errors
    if (pid < 0) {
        fprintf(stderr, "Fork failed\n");
        exit(EXIT_FAILURE);
    }
    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        if (exit_status != 0) {
            fprintf(stderr, "Command exited with status %d\n", exit_status);
        }
    }
    if (WIFSIGNALED(status)) {
        int signal_number = WTERMSIG(status);
        fprintf(stderr, "Command terminated by signal %d\n", signal_number);
    }
    if (WIFSTOPPED(status)) {
        int signal_number = WSTOPSIG(status);
        fprintf(stderr, "Command stopped by signal %d\n", signal_number);
    }
    if (WIFCONTINUED(status)) {
        fprintf(stderr, "Command continued\n");
    }
    exit(0);
}