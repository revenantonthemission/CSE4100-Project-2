#include "myshell.h"

int main() {
    // Variables
    char cmdline[MAX_LENGTH_3], *commands[MAX_LENGTH], *tokens[MAX_LENGTH];
    int i;

    do {
        // Initialize
        memset(cmdline, '\0', MAX_LENGTH_3);
        memset(commands, 0, MAX_LENGTH);
        memset(tokens, 0, MAX_LENGTH);

        // Clear the buffers
        fflush(stdin);
        fflush(stdout);

        // Shell Prompt: print your prompt
        fputs("CSE4100-SP-P2> ", stdout);

        // Reading: Read the command from standard input
        myshell_readInput(cmdline);

        // Parsing: Parse the command
        cmdline[strlen(cmdline)-1] = ' ';
        myshell_parseInput(cmdline, commands, "|");

        // Check for empty input
        if (commands[0] == NULL) {
            continue;
        }

        // Execute the command
        for(i = 0; i<MAX_LENGTH && commands[i]!=NULL; i++) {
            // Initialize
            memset(tokens, 0, MAX_LENGTH);
            myshell_parseInput(commands[i], tokens, " ");
            myshell_execCommand(tokens);
        }

    } while (1);

    return 0;
}

/* This function reads input from the command line */
void myshell_readInput(char *buf) {
    fgets(buf, MAX_LENGTH_3, stdin);
}

void myshell_parseInput(char *buf, char **args, const char *delim) {
    // Variables
    int i = 0;
    char* token = strtok(buf, delim);
    while (token != NULL) {
        // Store the token in the args array
        if (args != NULL) {
            args[i++] = token;
        }
        // Get the next token
        token = strtok(NULL, delim);
    }
}

void myshell_execCommand(char **args) {
    // Variables
    pid_t pid;  // 부모 프로세스(쉘)의 PID 얻기
    int status;

    // Execute the command
    if (!strcmp(args[0], "exit")) {
        exit(EXIT_SUCCESS);
    } else if (!strcmp(args[0], "cd")) {
        // Change directory
        if (args[1] == NULL) {
            // Change to home directory
            char *home = getenv("HOME");
            if (home == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return;
            }
            chdir(home);
        } else {
            // Change to specified directory
            if (chdir(args[1]) < 0) {
                fprintf(stderr, "cd: no such file or directory: %s\n", args[1]);
                return;
            }
        }
        return;
    }

    // Fork the process
    pid = fork();

    // Child Process
    if (pid == 0) {
        // Execute the command
        if (execvp(args[0], args) < 0) {
            fprintf(stderr, "%s: command not found\n", args[0]);
            exit(EXIT_FAILURE);
        }
    }
    // Parent Process
    else {
        // Wait for the child process to terminate
        waitpid(pid, &status, 0);
        return;
    }

    exit(0);
}