#ifndef __MY_SHELL_1
#define __MY_SHELL_1

// Headers
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <fcntl.h>

// Constants & Macros
#define MAX_LINE 8192
#define MAX_LENGTH 128

// Functions
void myshell_readInput(char *);
void myshell_parseInput(char *, char **);
void myshell_execCommand(char **);
#endif