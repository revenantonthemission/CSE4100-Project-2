#include "myshell.h"

int main()
{

    char cmdline[MAX_LENGTH_3], *commands[MAX_LENGTH];

    // 쉘 초기화
    myshell_init();

    Signal(SIGINT, myshell_SIGINT); /* ctrl-c */
    Signal(SIGCHLD, myshell_SIGCHLD);
    Signal(SIGTSTP, myshell_SIGTSTP); /* ctrl-z */

    do
    {
        setjmp(buffer);

        memset(cmdline, '\0', MAX_LENGTH_3);
        memset(commands, 0, MAX_LENGTH);
        fflush(stdout);
        
        if(sigchld_received) {
            sigchld_received = 0;
        }
        write(STDOUT_FILENO, prompt, strlen(prompt));

        // 명령어 입력
        myshell_readInput(cmdline);
        // 만약 빈 입력이면 루프 다시 진행
        if (strlen(cmdline) == 0)
            continue;

        // 명령어 파싱
        myshell_parseInput(cmdline, commands, "|");

        // 명령어 실행
        myshell_execCommand(commands);

    } while (1);

    return 0;
}

void myshell_init(void)
{

    // 터미널 장치와 연결된 파일 디스크립터를 확인한다.
    shell_terminal = STDIN_FILENO;

    // isatty 함수는 파일 디스크립터가 터미널(tty) 장치와 연결되어 있는지 확인하는 데 사용된다.
    // 이 함수는 파일 디스크립터를 매개변수로 받아, 터미널과 연결되어 있으면 0이 아닌 값을 반환하고, 그렇지 않으면 0을 반환한다.
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive)
    {
        /* Loop until we are in the foreground. */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
        {
            kill(-shell_pgid, SIGTTIN);
        }

        /* Ignore interactive and job-control signals.  */
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGCHLD, SIG_IGN);

        /* Put ourselves in our own process group.  */
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0)
        {
            perror("Couldn't put the shell in its own process group");
            _exit(EXIT_FAILURE);
        }
        /* Grab control of the terminal.  */
        tcsetpgrp(shell_terminal, shell_pgid);

        /* Save default terminal attributes for shell.  */
        tcgetattr(shell_terminal, &shell_tmodes);
    }
}

/* 명령어 입력을 읽는 함수 */
void myshell_readInput(char *buf)
{
    sigset_t mask, prev;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);   /* Block SIGCHLD while reading */

    if (fgets(buf, MAX_LENGTH_3, stdin) == NULL)
    {
        sigprocmask(SIG_SETMASK, &prev, NULL);  /* Unblock before exit */
        _exit(EXIT_SUCCESS);
    }
    sigprocmask(SIG_SETMASK, &prev, NULL);      /* Unblock after read */
    // 마지막 개행 문자 제거
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';
}

void myshell_parseInput(char *buf, char **args, const char *delim)
{
    // Variables
    int i = 0;
    char *token;

    // Remove leading and trailing whitespace
    while (isspace((unsigned char)*buf))
    {
        buf++;
    }
    while (buf[strlen(buf) - 1] == '\n' || buf[strlen(buf) - 1] == ' ')
    {
        buf[strlen(buf) - 1] = '\0';
    }

    // Tokenize the input string
    token = strtok(buf, delim);
    while (token != NULL)
    {
        // Remove leading and trailing whitespace
        while (isspace((unsigned char)*token))
        {
            token++;
        }
        while (token[strlen(token) - 1] == '\n' || token[strlen(token) - 1] == ' ')
        {
            token[strlen(token) - 1] = '\0';
        }

        // Store the token in the args array
        args[i++] = token;

        // Get the next token
        token = strtok(NULL, delim);
    }
    args[i] = NULL; // Null-terminate the args array
}

void myshell_execCommand(char **commands)
{
    // Variables
    int i = 0, status, last_token, background, infile, outfile;
    pid_t pid;
    job *new_job = NULL;
    process *proc_list = NULL;
    process *last_proc = NULL;
    char full_cmd[MAX_LENGTH_3] = "";
    char *args[MAX_LENGTH_2];

    do_job_notification();

    // 우선 각 명령어를 확인한다.
    while (commands[i] != NULL && shell_is_interactive)
    {

        // 명령어를 파싱하기 위해 args 배열을 초기화한다.
        for (int j = 0; j < MAX_LENGTH_2; j++)
        {
            args[j] = NULL;
        }

        // 각 명령어를 파싱한다.
        myshell_parseInput(commands[i], args, " ");

        last_token = 0;
        background = 0;

        // 마지막 토큰이 '&'이면 백그라운드 실행
        // '&'가 명령어와 붙어 있는 경우 처리 ex. "ls -l&"
        while (args[last_token] != NULL)
        {
            last_token++;
        }
        if (last_token > 0 && !strcmp(args[last_token - 1], "&"))
        {
            background = 1;
            args[last_token - 1] = NULL; // '&' 제거
        }
        else if (last_token > 0)
        {
            if (strlen(args[last_token - 1]) && args[last_token - 1][strlen(args[last_token - 1]) - 1] == '&')
            {
                background = 1;
                args[last_token - 1][strlen(args[last_token - 1]) - 1] = '\0'; // '&' 제거
            }
        }

        // 새 process를 동적 할당하여 초기화
        process *proc = (process *)malloc(sizeof(process));
        if (!proc)
        {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        // args 배열의 길이 계산하여 argv용 메모리 할당
        int arg_count = 0;
        while (args[arg_count] != NULL)
        {
            arg_count++;
        }
        proc->argv = malloc(sizeof(char *) * (arg_count + 1));
        for (int k = 0; k < arg_count; k++)
        {
            proc->argv[k] = strdup(args[k]); // 각 토큰을 복사
        }
        proc->argv[arg_count] = NULL;
        proc->pid = 0;         // 아직 fork 전이므로 0 초기화
        proc->state = RUNNING; // 실행 상태 기본값 (0: 완료, 1: 실행중, 2: 중지)
        proc->status = 0;
        proc->next = NULL;

        // proc = myshell_initProcess(NULL, args, 0, RUNNING, 0);
        //  process들을 연결된 리스트로 구성
        if (proc_list == NULL)
        {
            proc_list = proc;
        }
        else
        {
            last_proc->next = proc;
        }
        last_proc = proc;

        // job의 command 문자열 구성 (전체 명령어 문자열)
        strcat(full_cmd, commands[i]);
        if (commands[i + 1] != NULL)
        {
            strcat(full_cmd, " | ");
        }
        i++;
    }
    /* ---------- 빌트인 먼저 확인 ---------- */
    if (proc_list && myshell_handleBuiltin(proc_list->argv))
    {
        /* 임시로 만든 process 리스트 해제 */
        process *tmp;
        for (process *p = proc_list; p; p = tmp)
        {
            tmp = p->next;
            for (int i = 0; p->argv[i]; ++i)
                free(p->argv[i]);
            free(p->argv);
            free(p);
        }
        return; /* 잡을 만들지 않고 끝 */
    }

    /* ---------- 그 다음에 잡 생성 ---------- */
    new_job = myshell_initJob(
        NULL, strdup(full_cmd), proc_list,
        0, 0, shell_tmodes,
        STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO);
    new_job->next = first_job;
    first_job = new_job;

    launch_job(new_job, !background);
}

// 리다이렉션 처리 함수
void myshell_handleRedirection(char **tokens)
{
    int i = 0;
    int fd;

    // 모든 토큰 검사
    while (tokens[i] != NULL)
    {
        // 입력 리다이렉션
        if (strcmp(tokens[i], "<") == 0 && tokens[i + 1] != NULL)
        {
            fd = open(tokens[i + 1], O_RDONLY);
            if (fd < 0)
            {
                perror(tokens[i + 1]);
                _exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            tokens[i] = NULL; // 리다이렉션 기호와 파일명 제거
        }
        // 출력 리다이렉션
        else if (strcmp(tokens[i], ">") == 0 && tokens[i + 1] != NULL)
        {
            // 0644 = 0400(User read) + 0200(User write) + 0040(Group read) + 0004(Others read)
            fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0)
            {
                perror(tokens[i + 1]);
                _exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            tokens[i] = NULL; // 리다이렉션 기호와 파일명 제거
        }
        i++;
    }
}

int myshell_handleBuiltin(char **command)
{
    // Built-in 명령어 처리
    if (!strcmp(command[0], "exit"))
    {
        // exit 명령어 처리
        _exit(EXIT_SUCCESS);
    }
    else if (!strcmp(command[0], "cd"))
    {
        // Change directory
        if (command[1] == NULL)
        {
            // Change to home directory
            char *home = getenv("HOME");
            if (home == NULL)
            {
                perror("cd");
                return 1;
            }
            chdir(home);
        }
        else
        {
            // Change to specified directory
            if (chdir(command[1]) < 0)
            {
                perror("cd");
                return 1;
            }
        }
    }
    else if (!strcmp(command[0], "jobs"))
    {
        // 현재 job 리스트를 순회하며 아직 완료되지 않은 job들을 출력
        for (job *j = first_job; j != NULL; j = j->next)
        {
            if (!job_is_completed(j))
            {
                fprintf(stdout, "[%ld] ", (long)j->pgid);
                if (job_is_stopped(j))
                    fprintf(stdout, "Stopped ");
                else
                    fprintf(stdout, "Running ");
                fprintf(stdout, "%s\n", j->command);
            }
        }
        return 1;
    }
    else if (!strcmp(command[0], "fg"))
    {
        // fg 명령 사용법: fg [%jobid]
        job *j = NULL;
        if (command[1] == NULL)
        {
            // 인자가 없으면 가장 최근의 job(여기서는 first_job)을 사용
            j = first_job;
        }
        else
        {
            // 인자가 있을 경우 앞에 '%'가 있다면 제거하고 해당 job id를 사용
            char *jid_str = command[1];
            if (jid_str[0] == '%')
                jid_str++;
            pid_t jid = (pid_t)atoi(jid_str);
            // job 리스트에서 일치하는 job 찾기
            for (job *temp = first_job; temp != NULL; temp = temp->next)
            {
                if (temp->pgid == jid)
                {
                    j = temp;
                    break;
                }
            }
        }
        if (j == NULL)
        {
            fprintf(stderr, "fg: No such job\n");
            return 1;
        }
        // 해당 job을 포그라운드로 전환(필요시 SIGCONT 포함)
        put_job_in_foreground(j, 1);
        return 1;
    }
    else if (!strcmp(command[0], "bg"))
    {
        // bg 명령 사용법: bg [%jobid]
        job *j = NULL;
        if (command[1] == NULL)
        {
            // 인자가 없으면 가장 최근의 job을 사용
            j = first_job;
        }
        else
        {
            char *jid_str = command[1];
            if (jid_str[0] == '%')
                jid_str++;
            pid_t jid = (pid_t)atoi(jid_str);
            for (job *temp = first_job; temp != NULL; temp = temp->next)
            {
                if (temp->pgid == jid)
                {
                    j = temp;
                    break;
                }
            }
        }
        if (j == NULL)
        {
            fprintf(stderr, "bg: No such job\n");
            return 1;
        }
        // 백그라운드 실행: 중지되어 있던 job을 계속 실행
        put_job_in_background(j, 1);
        fprintf(stdout, "[%ld] %s &\n", (long)j->pgid, j->command);
        return 1;
    }
    else if (!strcmp(command[0], "kill"))
    {
        // kill 명령 사용법: kill [-signal] [%jobid or pid]
        if (command[1] == NULL)
        {
            fprintf(stderr, "kill: usage: kill [-signal] %%jobid or pid\n");
            return 1;
        }
        int sig = SIGTERM; // 기본 시그널은 SIGTERM
        int arg_index = 1;
        // 첫 번째 인자가 '-'로 시작하면 시그널 번호를 읽어옵니다.
        if (command[1][0] == '-')
        {
            sig = atoi(command[1] + 1);
            arg_index = 2;
        }
        if (command[arg_index] == NULL)
        {
            fprintf(stderr, "kill: usage: kill [-signal] %%jobid or pid\n");
            return 1;
        }
        if (command[arg_index][0] == '%')
        {
            // job id가 지정된 경우
            char *jid_str = command[arg_index] + 1; // '%' 제거
            pid_t jid = (pid_t)atoi(jid_str);
            job *j = NULL;
            for (job *temp = first_job; temp != NULL; temp = temp->next)
            {
                if (temp->pgid == jid)
                {
                    j = temp;
                    break;
                }
            }
            if (j == NULL)
            {
                fprintf(stderr, "kill: No such job\n");
                return 1;
            }
            // job의 프로세스 그룹 전체에 시그널 전송
            if (kill(-j->pgid, sig) < 0)
                perror("kill");
        }
        else
        {
            // PID가 지정된 경우
            pid_t pid = (pid_t)atoi(command[arg_index]);
            if (kill(pid, sig) < 0)
                perror("kill");
        }
        return 1;
    }
    else
    {
        return 0;
    }
    return 1;
}

job *myshell_initJob(job *next, char *command, process *first_process, pid_t pgid, char notified, struct termios tmodes, int stdin, int stdout, int stderr)
{
    job *job_instance = (job *)malloc(sizeof(job));

    job_instance->next = next;
    job_instance->command = command;
    job_instance->first_process = first_process;
    job_instance->pgid = pgid;
    job_instance->notified = notified;
    job_instance->tmodes = tmodes;
    job_instance->stdin = stdin;
    job_instance->stdout = stdout;
    job_instance->stderr = stderr;

    return job_instance;
}

process *myshell_initProcess(process *next, char **argv, pid_t pid, char state, int status)
{
    process *proc = (process *)malloc(sizeof(process));
    if (!proc)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    proc->next = next;
    proc->argv = argv;
    proc->pid = pid;
    proc->state = state;
    proc->status = status;

    return proc;
}

/* Keep track of attributes of the shell.  */
void launch_process(process *p, pid_t pgid,
                    int infile, int outfile, int errfile,
                    int foreground)
{
    pid_t pid;

    if (shell_is_interactive)
    {
        /* Put the process into the process group and give the process group
        the terminal, if appropriate.
        This has to be done both by the shell and in the individual
        child processes because of potential race conditions.  */
        pid = getpid();
        if (pgid == 0)
            pgid = pid;
        setpgid(pid, pgid);
        if (foreground)
            tcsetpgrp(shell_terminal, pgid);

        /* Set the handling for job control signals back to the default.  */
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
    }

    /* Set the standard input/output channels of the new process.  */
    if (infile != STDIN_FILENO)
    {
        dup2(infile, STDIN_FILENO);
        close(infile);
    }
    if (outfile != STDOUT_FILENO)
    {
        dup2(outfile, STDOUT_FILENO);
        close(outfile);
    }
    if (errfile != STDERR_FILENO)
    {
        dup2(errfile, STDERR_FILENO);
        close(errfile);
    }

    /* Exec the new process.  Make sure we exit.  */
    execvp(p->argv[0], p->argv);
    perror("execvp");
    _exit(EXIT_FAILURE);
}

void launch_job(job *j, int foreground)
{
    process *p;
    pid_t pid;
    int mypipe[2], infile, outfile;

    infile = j->stdin;
    // 백그라운드 작업일 경우 stdin을 /dev/null로 설정
    if (!foreground && (infile == STDIN_FILENO))
    {
        infile = open("/dev/null", O_RDONLY);
        if (infile < 0)
        {
            perror("open /dev/null");
            _exit(EXIT_FAILURE);
        }
    }

    for (p = j->first_process; p; p = p->next)
    {
        /* Set up pipes, if necessary.  */
        if (p->next)
        {
            if (pipe(mypipe) < 0)
            {
                perror("pipe");
                _exit(EXIT_FAILURE);
            }
            outfile = mypipe[1];
        }
        else
            outfile = j->stdout;

        /* Fork the child processes.  */
        pid = fork();
        if (pid == 0)
        {
            /* This is the child process.  */
            launch_process(p, j->pgid, infile, outfile, j->stderr, foreground);
        }
        else if (pid < 0)
        {
            /* The fork failed.  */
            perror("fork");
            _exit(EXIT_FAILURE);
        }
        else
        {
            /* This is the parent process.  */
            p->pid = pid;
            if (shell_is_interactive)
            {
                if (!j->pgid)
                    j->pgid = pid;
                setpgid(pid, j->pgid);
            }
        }

        /* Clean up after pipes.  */
        if (infile != j->stdin)
            close(infile);
        if (outfile != j->stdout)
            close(outfile);
        infile = mypipe[0];
    }

    format_job_info(j, "launched");

    if (!shell_is_interactive)
        wait_for_job(j);
    else if (foreground)
    {
        put_job_in_foreground(j, 0);
    }
    else
    {
        put_job_in_background(j, 0);
    }
}

/* Put job j in the foreground.  If cont is nonzero,
   restore the saved terminal modes and send the process group a
   SIGCONT signal to wake it up before we block.  */

void put_job_in_foreground(job *j, int cont)
{
    /* Put the job into the foreground.  */
    tcsetpgrp(shell_terminal, j->pgid);

    /* Send the job a continue signal, if necessary.  */
    if (cont)
    {
        tcsetattr(shell_terminal, TCSADRAIN, &j->tmodes);
        if (kill(-j->pgid, SIGCONT) < 0)
            perror("kill (SIGCONT)");
    }

    /* Wait for it to report.  */
    wait_for_job(j);

    /* Put the shell back in the foreground.  */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Restore the shell's terminal modes.  */
    tcgetattr(shell_terminal, &j->tmodes);
    tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
}

/* Put a job in the background.  If the cont argument is true, send
the process group a SIGCONT signal to wake it up.  */

void put_job_in_background(job *j, int cont)
{
    /* Send the job a continue signal, if necessary.  */
    if (cont) {
        if (kill(-j->pgid, SIGCONT) < 0)
            perror("kill (SIGCONT)");
        longjmp(buffer, ++jmp_call);
    }
        
}

/* Store the status of the process pid that was returned by waitpid.
   Return 0 if all went well, nonzero otherwise.  */

int mark_process_status(pid_t pid, int status)
{
    job *j;
    process *p;

    if (pid > 0)
    {
        /* Update the record for the process.  */
        for (j = first_job; j; j = j->next) {
            for (p = j->first_process; p; p = p->next){
                if (p->pid == pid)
                {
                    p->status = status;
                    if (WIFSTOPPED(status))
                        p->state = 2;
                    else
                    {
                        p->state = 0;
                        if (WIFSIGNALED(status))
                            fprintf(stderr, "%d: Terminated by signal %d.\n",
                                    (int)pid, WTERMSIG(p->status));
                    }
                    return 0;
                }
            }
        }
        fprintf(stderr, "No child process %d.\n", pid);
        return -1;
    }
    else if (pid == 0 || errno == ECHILD)
        /* No processes ready to report.  */
        return -1;
    else
    {
        /* Other weird errors.  */
        perror("waitpid");
        return -1;
    }
}

/* Check for processes that have status information available,
   without blocking.  */

void update_status(void)
{
    int status;
    pid_t pid;

    do
        pid = waitpid(WAIT_ANY, &status, WUNTRACED | WNOHANG);
    while (!mark_process_status(pid, status));
}

/* Check for processes that have status information available,
   blocking until all processes in the given job have reported.  */

void wait_for_job(job *j)
{
    int status;
    pid_t pid;

    do
        pid = waitpid(WAIT_ANY, &status, WUNTRACED);
    while (!mark_process_status(pid, status) && !job_is_stopped(j) && !job_is_completed(j));
}

/* Format information about job status for the user to look at.  */

void format_job_info(job *j, const char *status)
{
    fprintf(stderr, "%ld (%s): %s\n", (long)j->pgid, status, j->command);
}

/* Notify the user about stopped or terminated jobs.
   Delete terminated jobs from the active job list.  */

void do_job_notification(void)
{
    job *j, *jlast, *jnext;
    process *p;

    /* Update status information for child processes.  */
    update_status();

    jlast = NULL;
    for (j = first_job; j; j = jnext)
    {
        jnext = j->next;

        /* If all processes have completed, tell the user the job has
           completed and delete it from the list of active jobs.  */
        if (job_is_completed(j))
        {
            format_job_info(j, "completed");
            if (jlast)
                jlast->next = jnext;
            else
                first_job = jnext;
            free_job(j);
        }

        /* Notify the user about stopped jobs,
           marking them so that we won't do this more than once.  */
        else if (job_is_stopped(j) && !j->notified)
        {
            format_job_info(j, "stopped");
            j->notified = 1;
            jlast = j;
        }

        /* Don't say anything about jobs that are still running.  */
        else
            jlast = j;
    }
}

/* Mark a stopped job J as being running again.  */

void mark_job_as_running(job *j)
{
    process *p;

    for (p = j->first_process; p; p = p->next)
        p->state = RUNNING;
    j->notified = 0;
}

/* Continue the job J.  */

void continue_job(job *j, int foreground)
{
    mark_job_as_running(j);
    if (foreground)
        put_job_in_foreground(j, 1);
    else
        put_job_in_background(j, 1);
}

/* Find the active job with the indicated pgid.  */
job *find_job(pid_t pgid)
{
    job *j;

    for (j = first_job; j; j = j->next)
        if (j->pgid == pgid)
            return j;
    return NULL;
}

/* Return true if all processes in the job have stopped or completed.  */
int job_is_stopped(job *j)
{
    process *p;

    for (p = j->first_process; p; p = p->next)
        if (p->state != STOPPED)
            return 0;
    return 1;
}

/* Return true if all processes in the job have completed.  */
int job_is_completed(job *j)
{
    process *p;

    for (p = j->first_process; p; p = p->next)
        if (p->state != OFF)
            return 0;
    return 1;
}
void free_job(job *j)
{
    j->next = NULL;
    free(j->command);
    j->first_process = NULL;
    free(j);
}

// 현재 포그라운드 작업을 얻는 함수
job *get_foreground_job(void)
{
    for (job *j = first_job; j; j = j->next)
    {
        pid_t fg = tcgetpgrp(shell_terminal);
        if (j->pgid == fg && !job_is_completed(j) && !job_is_stopped(j))
            return j;
    }
    return NULL;
}

void myshell_SIGINT(int signal)
{
    // 포그라운드 작업이 있을 때만 해당 작업에 시그널 전달
    job *j = get_foreground_job();
    if (j != NULL)
    {
        kill(-j->pgid, SIGINT);
    }
    else
    {
        // 포그라운드 작업이 없으면 새 프롬프트 출력
        write(STDOUT_FILENO, "\n", 1);
        write(STDOUT_FILENO, prompt, strlen(prompt));
    }
    longjmp(buffer, ++jmp_call); // 현재 명령 입력 취소
}

void myshell_SIGCHLD(int sig)
{
    int saved_errno = errno;
    pid_t pid; int status;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
        mark_process_status(pid, status);
        do_job_notification();

    sigchld_received = 1;     /* 메인 루프에게 알려만 줌 */
    errno = saved_errno;
}

void myshell_SIGTSTP(int sig)
{
    job *j = get_foreground_job();
    if (j)
        kill(-j->pgid, SIGTSTP);
    else
    {
        write(STDOUT_FILENO, "\n", 1);
        write(STDOUT_FILENO, prompt, strlen(prompt));
    }
    longjmp(buffer, ++jmp_call);
}

handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* Restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        perror("Signal error");

    return (old_action.sa_handler);
}