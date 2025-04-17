#include "myshell.h"

int main()
{
    // 변수 선언
    char cmdline[MAX_LENGTH_3], *commands[MAX_LENGTH];
    int i;

    // 쉘의 pid를 쉘의 프로세스 그룹 ID로 설정함.
    shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    // 셸 프로세스 그룹을 foreground로 설정
    tcsetpgrp(STDIN_FILENO, shell_pgid);

    Signal(SIGINT, myshell_SIGINT);   // ctrl-c를 누를 때 myshell_SIGINT가 호출됨.
    Signal(SIGCHLD, myshell_SIGCHLD); // 자식 프로세스가 종료될 때 myshell_SIGCHLD가 호출됨.
    Signal(SIGTSTP, myshell_SIGTSTP); // ctrl-z를 누를 때 myshell_SIGTSTP가 호출됨.
    Signal(SIGTTOU, SIG_IGN);
    Signal(SIGTTIN, SIG_IGN);

    do
    {

        setjmp(jump);

        // 버퍼 비우기
        fflush(stdout);

        // 초기화
        memset(cmdline, '\0', MAX_LENGTH_3);
        memset(commands, 0, MAX_LENGTH);

        // 프롬프트 출력
        write(STDOUT_FILENO, prompt, strlen(prompt));

        // 만약 백그라운드 프로세스의 종료로 인해 프롬프트가 입력받는 위치에 없다면, 입력을 기다리지 않고 새로운 프롬프트를 출력함
        myshell_readInput(cmdline);

        // 파이프를 기준으로 전체 커맨드를 나누어 commands에 담음.
        myshell_parseInput(cmdline, commands, "|");

        // 명령어 실행
        myshell_execCommand(commands);
    } while (1);

    return 0;
}

/* 명령어 입력을 읽는 함수 */
void myshell_readInput(char *buf)
{
    // 커맨드 입력
    fgets(buf, MAX_LENGTH_3, stdin);
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
    int i = 0;
    int status, background, last_token, job_num;
    int prev_pipe[2] = {-1, -1};
    int curr_pipe[2];
    pid_t pid;
    job_t *job;
    char *tokens[MAX_LENGTH], pid_str[10];

    // 명령어가 없으면 종료
    if (commands[0] == NULL)
    {
        return;
    }

    // 각 명령어 처리
    while (commands[i] != NULL)
    {

        // 현재 명령어를 토큰으로 분리
        memset(tokens, 0, sizeof(tokens));
        myshell_parseInput(commands[i], tokens, " ");

        last_token = 0;
        background = 0;

        // 마지막 토큰이 '&'이면 백그라운드 실행
        // '&'가 명령어와 붙어 있는 경우 처리 ex. "ls -l&"
        while (tokens[last_token] != NULL)
        {
            last_token++;
        }
        if (last_token > 0 && !strcmp(tokens[last_token - 1], "&"))
        {
            background = 1;
            tokens[last_token - 1] = NULL; // '&' 제거
        }
        else if (last_token > 0)
        {
            if (strlen(tokens[last_token - 1]) && tokens[last_token - 1][strlen(tokens[last_token - 1]) - 1] == '&')
            {
                background = 1;
                tokens[last_token - 1][strlen(tokens[last_token - 1]) - 1] = '\0'; // '&' 제거
            }
        }

        // built-in 명령어 처리.
        if (!strcmp(tokens[0], "exit"))
        {
            // exit: 쉘 자체를 종료한다.
            _exit(EXIT_SUCCESS);
        }
        else if (!strcmp(tokens[0], "cd"))
        {
            // cd: Change directory. 현재 쉘이 위치한 경로를 바꾼다. tokens[1]에 가고자 하는 디렉토리가 적혀있음.
            if (tokens[1] == NULL)
            {
                // 디렉토리가 명시되어 있지 않은 경우 홈 디렉토리로 이동함,
                // 이때 getenv 함수를 통해 홈 디렉토리를 얻은 뒤 그곳으로 이동한다.
                char *home = getenv("HOME");
                if (home == NULL)
                {
                    // 다음 명령을 시도해야 하기 때문에 continue로 while문을 탈출한다.
                    perror("cd");
                    continue;
                }
                chdir(home);
            }
            else
            {
                // chdir 함수를 통해 tokens[1]에 적힌 디렉토리로 이동을 시도함.
                // 만약 실패한다면 반환값은 -1이다.
                if (chdir(tokens[1]) < 0)
                {
                    // 다음 명령을 시도해야 하기 때문에 continue로 while문을 탈출한다.
                    perror("cd");
                    continue;
                }
            }
            i++;
            continue;
        }
        else if (!strcmp(tokens[0], "jobs"))
        {
            // 현재 실행 중인 프로세스 목록을 출력한다.
            list_jobs();

            // 다음 명령을 시도해야 하기 때문에 continue로 while문을 탈출한다.
            i++;
            continue;
        }
        else if (!strcmp(tokens[0], "fg"))
        {
            // 백그라운드에 있던 작업을 포그라운드로 가져온다.
            // 중지됐거나 백그라운드로 실행 중인 작업이 백그라운드에 있다.

            // tokens[1]을 통해 현재 백그라운드에 있는 작업을 지s정한다.
            if (tokens[1] != NULL && tokens[1][0] == '%')
            {
                // job_num으로 지정한 작업을 가져온다.
                job_num = atoi(&tokens[1][1]);
                job = get_job_by_index(job_num);
                // 이렇게 가져온 job이 실제로 있다면, job의 상태를 RUNNING으로 바꾸고 해당 작업을 다시 활성화하기 위해 SIGCONT 시그널을 보낸다.
                // 그리고 터미널 통제권을 가져온다.
                if (job != NULL)
                {
                    // 1. foreground 정보 갱신
                    
                    foreground_pid = job->pid;
                    foreground_cmd = job->cmdline;

                    // 2. 터미널 제어권 넘기기
                    tcsetpgrp(STDIN_FILENO, job->pid);

                    // 3. SIGCONT로 깨우기
                    kill(-(job->pid), SIGCONT);

                    // 4. 종료/중지될 때까지 대기
                    waitpid(job->pid, &status, 0);
                    // 5. 다시 쉘이 터미널 제어권 획득
                    tcsetpgrp(STDIN_FILENO, shell_pgid);

                    // 6. foreground 정보 초기화
                    foreground_pid = 0;
                    foreground_cmd = NULL;
                }
            }
            else
            {
                // 예외 처리
            }

            // 다음 명령을 시도해야 하기 때문에 continue로 while문을 탈출한다.
            i++;
            continue;
        }
        else if (!strcmp(tokens[0], "bg"))
        {
            // tokens[1]을 통해 현재 백그라운드에 있는 작업을 지s정한다.
            if (tokens[1] != NULL && tokens[1][0] == '%')
            {

                // job_num으로 지정한 작업을 가져온다.
                job_num = atoi(&tokens[1][1]);
                job = get_job_by_index(job_num);
                // 이렇게 가져온 job이 실제로 있다면, job의 상태를 RUNNING으로 바꾸고 해당 작업을 다시 활성화하기 위해 SIGCONT 시그널을 보낸다.
                // 그리고 터미널 통제권을 가져온다.
                if (job != NULL)
                {
                    job->state = RUNNING;
                    kill(-(job->pid), SIGCONT);
                }
            }
            else
            {
                // 예외 처리
            }

            // 다음 명령을 시도해야 하기 때문에 continue로 while문을 탈출한다.
            i++;
            continue;
        }
        else if (!strcmp(tokens[0], "kill"))
        {
            if (tokens[1] != NULL && tokens[1][0] == '%')
            {
                int job_num = atoi(tokens[1] + 1);
                job_t *job = get_job_by_index(job_num);
                if (job != NULL)
                {
                    // 지정된 시그널을 보냄.
                }
            }
            i++;
            continue;
        }

        // 다음 명령어가 있으면 새 파이프 생성
        if (commands[i + 1] != NULL)
        {
            if (pipe(curr_pipe) < 0)
            {
                perror("pipe");
                _exit(EXIT_FAILURE);
            }
        }

        // 자식 프로세스 생성
        pid = fork();
        if (pid < 0)
        {
            perror("fork");
            _exit(EXIT_FAILURE);
        }
        // 자식 프로세스는 이 부분을 실행한다.
        else if (pid == 0)
        {
            // 자식 프로세스의 그룹 ID 설정
            setpgid(0, 0);

            // 이전 파이프에서 입력 리다이렉션
            if (prev_pipe[0] != -1)
            {
                dup2(prev_pipe[0], STDIN_FILENO);
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }

            // 다음 명령어가 있으면 출력 리다이렉션
            if (commands[i + 1] != NULL)
            {
                close(curr_pipe[0]);
                dup2(curr_pipe[1], STDOUT_FILENO);
                close(curr_pipe[1]);
            }

            // 백그라운드 프로세스인 경우, SIGINT와 SIGTSTP 시그널을 무시함
            /*if (background)
            {
                Signal(SIGINT, SIG_IGN);
                Signal(SIGTSTP, SIG_IGN);
            }*/

            // 명령어 실행
            myshell_handleRedirection(tokens); // 리다이렉션 처리
            execvp(tokens[0], tokens);
            perror(tokens[0]);
            _exit(EXIT_FAILURE);
        }
        // 부모 프로세스는 이 부분을 실행한다.
        else
        {
            // 이전 파이프 닫기
            if (prev_pipe[0] != -1)
            {
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }
            // 현재 파이프를 이전 파이프로 설정
            if (commands[i + 1] != NULL)
            {
                prev_pipe[0] = curr_pipe[0];
                prev_pipe[1] = curr_pipe[1];
            }
            else
            {
                // 마지막 명령어면 자식 프로세스 기다림
                if (background)
                {
                    // 백그라운드 프로세스인 경우, PID 출력
                    add_job(pid, commands[i], RUNNING); // Add job to job list
                    sprintf(pid_str, "[%d] %d", job_count, pid);
                    write(STDOUT_FILENO, pid_str, strlen(pid_str));
                    write(STDOUT_FILENO, "\n", 1);
                }
                else
                {
                    foreground_pid = pid;
                    foreground_cmd = commands[i];
                    tcsetpgrp(STDIN_FILENO, pid);

                    do
                    {
                        waitpid(pid, &status, 0);
                    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

                    tcsetpgrp(STDIN_FILENO, shell_pgid); // 셸 프로세스 그룹을 foreground로 설정
                    foreground_cmd = NULL;
                    foreground_pid = 0;
                }
            }
        }
        i++;
    }
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

void add_job(pid_t pid, const char *cmdline, char state)
{
    if (job_count < MAX_JOBS)
    {
        job_list[job_count].pid = pid;
        strncpy(job_list[job_count].cmdline, cmdline, MAX_LENGTH_2);
        job_list[job_count].state = state;
        switch (state)
        {
        case RUNNING:
            kill(pid, SIGCONT);
            break;
        case STOPPED:
            kill(pid, SIGSTOP);
            break;
        }
        job_count++;
    }
}

void list_jobs()
{
    for (int i = 0; i < job_count; ++i)
    {
        if (job_list[i].state != RUNNING)
            printf("[%d] %d\t%s\n", i + 1, job_list[i].pid, job_list[i].cmdline);
    }
}

job_t *get_job_by_index(int idx)
{
    if (idx >= 1 && idx <= job_count)
        return &job_list[idx - 1];
    return NULL;
}

void myshell_SIGINT(int signal)
{
    write(STDOUT_FILENO, "SIGINT received\n", 17);
    longjmp(jump, signal);
}

void myshell_SIGCHLD(int signal)
{
    pid_t pid;
    int status;
    // 백그라운드 프로세스 종료 시그널 처리
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        int found_idx = -1;
        // 종료된 프로세스의 PID를 찾음
        for (int i = 0; i < job_count; ++i)
        {
            if (job_list[i].pid == pid)
            {
                found_idx = i;
                write(STDOUT_FILENO, "\n", 1);
                fprintf(stdout, "Job [%d] (%d) terminated\n", i + 1, pid);
                write(STDOUT_FILENO, prompt, strlen(prompt));
                break;
            }
        }

        // 종료된 작업을 찾았을 때만 배열 시프트
        if (found_idx >= 0)
        {
            // 발견된 인덱스부터 배열 시프트
            for (int j = found_idx; j < job_count - 1; j++)
            {
                job_list[j] = job_list[j + 1];
            }
            job_count--;
        }
    }
}

void myshell_SIGTSTP(int signal)
{
    // 현재 실행 중인 프로세스를 정지시킨다.
    // kill(foreground_pid, SIGSTOP);

    // jobs에 현재 실행 중인 프로세스를 추가한다.
    add_job(foreground_pid, foreground_cmd, STOPPED);

    fprintf(stdout, "myshell: suspended %s\n", foreground_cmd);

    longjmp(jump, signal);
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