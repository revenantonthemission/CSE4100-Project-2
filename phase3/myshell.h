#ifndef __MY_SHELL_3
#define __MY_SHELL_3

// Headers
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <setjmp.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>

// Constants & Macros
#define MAX_LENGTH 32
#define MAX_LENGTH_2 1024
#define MAX_LENGTH_3 32768
#define OFF 0
#define RUNNING 1
#define STOPPED 2

/* Process structure for each command in the pipeline.  */
typedef struct process
{
    struct process *next; /* next process in pipeline */
    char **argv;          /* for exec */
    pid_t pid;            /* process ID */
    char state;           /* the state of the process, 0 for completion, 1 for running, 2 for stopped */
    int status;           /* reported status value */
} process;

/* A job is a pipeline of processes.  */
typedef struct job
{
    struct job *next;          /* next active job */
    char *command;             /* command line, used for messages */
    process *first_process;    /* list of processes in this job */
    pid_t pgid;                /* process group ID */
    char notified;             /* true if user told about stopped job */
    struct termios tmodes;     /* saved terminal modes */
    int stdin, stdout, stderr; /* i/o channels */
} job;

typedef void handler_t(int);

/* begin global variables */
job *first_job = NULL; /* The active jobs are linked into a list.  This is its head. */
pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;
char prompt[] = "CSE4100-SP-P3> "; /* command line prompt */
jmp_buf buffer;

// Functions
void myshell_init();
void myshell_readInput(char *);
void myshell_parseInput(char *, char **, const char *);
void myshell_execCommand(char **);
void myshell_handleRedirection(char **);
int myshell_handleBuiltin(char **);
job *myshell_initJob(job *, char *, process *, pid_t, char, struct termios, int, int, int);
process *myshell_initProcess(process *, char **, pid_t, char, int);
void launch_process(process *, pid_t, int, int, int, int);
int job_is_stopped(job *);
int job_is_completed(job *);
void launch_job(job *, int);
void put_job_in_foreground(job *j, int cont);
void put_job_in_background(job *j, int cont);
int mark_process_status(pid_t pid, int status);
void update_status(void);
void wait_for_job(job *j);
void format_job_info(job *j, const char *status);
void do_job_notification(void);
void mark_job_as_running(job *j);
void continue_job(job *j, int foreground);
job *find_job(pid_t);
void free_job(job *);
void myshell_SIGINT(int);
void myshell_SIGCHLD(int);
void myshell_SIGTSTP(int);
void myshell_SIGTERM(int);
handler_t *Signal(int signum, handler_t *handler);

#endif