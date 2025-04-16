#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h> 
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <sys/file.h>

pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;

/* A process is a single process.  */
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

job *find_job(pid_t);
int job_is_stopped(job *);
int job_is_completed(job *);
void init_shell (void);
void launch_process (process *, pid_t, int, int, int, int);
void launch_job(job *, int);
void put_job_in_foreground(job *, int);
void put_job_in_background(job *, int);
void mark_process_status(pid_t, int);
void update_status(void);
void wait_for_job(job *);
void format_job_info(job *, const char *);
void do_job_notification(void);
void mark_job_as_running(job *);
void continue_job(job *j, int foreground);