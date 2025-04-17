#ifndef __MY_SHELL_3
#define __MY_SHELL_3

// Headers
#include <assert.h>
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
#include <pthread.h>
#include <semaphore.h>

// Constants & Macros
#define MAX_LENGTH 32
#define MAX_LENGTH_2 1024
#define MAX_LENGTH_3 32768
#define MAX_JOBS 128

#define RUNNING 0 // Currently running or waiting to run
#define STOPPED 1 // SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU

typedef struct {
    pid_t pid;
    char cmdline[MAX_LENGTH_2];
    char state; // RUNNING or STOPPED
} job_t;

typedef void handler_t(int);

/* begin global variables */
char prompt[] = "CSE4100-SP-P2> ";       /* command line prompt */
char message[MAX_LENGTH_2];         /* for composing sprintf messages */
char *foreground_cmd = NULL;
volatile sig_atomic_t job_count = 0;
volatile pid_t foreground_pid = 0;
pid_t shell_pgid;
job_t job_list[MAX_JOBS];
sigjmp_buf jump;

// Functions
void add_job(pid_t pid, const char *cmdline, char state);
void list_jobs();
job_t *get_job_by_index(int idx);
void myshell_readInput(char *);
void myshell_parseInput(char *, char **, const char *);
void myshell_execCommand(char **);
void myshell_handleRedirection(char **);
void myshell_handleBuiltin(char **);
void myshell_SIGINT(int);
void myshell_SIGCHLD(int);
void myshell_SIGTSTP(int);

handler_t *Signal(int signum, handler_t *handler);
#endif