#ifndef __MY_SHELL_2
#define __MY_SHELL_2

// Headers
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <assert.h>

// Constants & Macros
#define MAX_LENGTH 32
#define MAX_LENGTH_2 1024
#define MAX_LENGTH_3 32768

// Functions
void myshell_readInput(char *);
void myshell_parseInput(char *, char **, const char *);
void myshell_execCommand(char **);
void myshell_handleRedirection(char **);
#endif