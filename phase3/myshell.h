#ifndef __MY_SHELL_3
#define __MY_SHELL_3

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

// code/ecf/shell.c에서 일단 그대로 가져와 봄.

#define MAXARGS   128
#define MAXJOBS    16

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : "fg pid"
 *     ST -> BG  : "bg pid"
 *     BG -> FG  : "fg pid"
 * At most 1 job can be in the FG state.
 */

/* begin global variables */
char prompt[] = "CSE4100-SP-P2> ";       /* command line prompt */
int verbose = 0;            /* if true, print extra output */
char message[MAX_LENGTH_2];         /* for composing sprintf messages */
volatile sig_atomic_t prompt_ready = 1;

struct job_t {
    pid_t pid;              /* job PID */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAX_LENGTH_2];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* job list */



// Functions
void myshell_readInput(char *);
void myshell_parseInput(char *, char **, const char *);
void myshell_execCommand(char **);
void myshell_handleRedirection(char **);
void myshell_SIGINT(int);
void myshell_SIGCHLD(int);
void myshell_SIGTSTP(int);
void myshell_initJobs(void);
void myshell_addJob(pid_t, int, const char*);
void myshell_deleteJob(pid_t);
void myshell_listJobs(void);
void myshell_waitForJob(pid_t);
void myshell_killJob(pid_t);
void myshell_fgJob(pid_t);
void myshell_bgJob(pid_t);
void myshell_setJobState(pid_t, int);
int myshell_getJobState(pid_t);

#endif