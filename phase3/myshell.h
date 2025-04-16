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
#include <termios.h>

// Constants & Macros
#define MAX_LENGTH 32
#define MAX_LENGTH_2 1024
#define MAX_LENGTH_3 32768
#define MAX_JOBS 128

/* Process structure for each command in the pipeline.  */
typedef struct process
{
  struct process *next;       /* next process in pipeline */
  char **argv;                /* for exec */
  pid_t pid;                  /* process ID */
  char completed;             /* true if process has completed */
  char stopped;               /* true if process has stopped */
  int status;                 /* reported status value */
} process;

/* A job is a pipeline of processes.  */
typedef struct job
{
  struct job *next;           /* next active job */
  char *command;              /* command line, used for messages */
  process *first_process;     /* list of processes in this job */
  pid_t pgid;                 /* process group ID */
  char notified;              /* true if user told about stopped job */
  struct termios tmodes;      /* saved terminal modes */
  int stdin, stdout, stderr;  /* standard i/o channels */
} job;

/* The active jobs are linked into a list.  This is its head.   */
job *first_job = NULL;

typedef struct {
    pid_t pid;
    char cmdline[MAX_LENGTH_2];
    int running;
    int stopped; // Added to track if the job is stopped
} job_t;

typedef void handler_t(int);

/* begin global variables */
char prompt[] = "CSE4100-SP-P3> ";       /* command line prompt */
int verbose = 0;            /* if true, print extra output */
char message[MAX_LENGTH_2];         /* for composing sprintf messages */
int job_count = 0;
pid_t shell_pgid;
job_t job_list[MAX_JOBS];
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;

// Functions
void add_job(pid_t pid, const char *cmdline);
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
void myshell_SIGTERM(int);

handler_t *Signal(int signum, handler_t *handler);
#endif