#include "myshell.h"

int main() {
    // 변수 선언
    char cmdline[MAX_LENGTH_3], *commands[MAX_LENGTH];
    int i;

    Signal(SIGINT, myshell_SIGINT);   /* ctrl-c */
    Signal(SIGCHLD, myshell_SIGCHLD); 
    Signal(SIGTSTP, myshell_SIGTSTP); /* ctrl-z */

    do {
        // 초기화
        memset(cmdline, '\0', MAX_LENGTH_3);
        memset(commands, 0, MAX_LENGTH);

        // 버퍼 비우기
        fflush(stdout);

        write(STDOUT_FILENO, prompt, strlen(prompt));

        // 만약 백그라운드 프로세스의 종료로 인해 프롬프트가 입력받는 위치에 없다면, 입력을 기다리지 않고 새로운 프롬프트를 출력함
        myshell_readInput(cmdline);

        // 명령어 파싱
        myshell_parseInput(cmdline, commands, "|");
        
        // 명령어 실행
        myshell_execCommand(commands);

    } while (1);

    return 0;
}

/* 명령어 입력을 읽는 함수 */
void myshell_readInput(char *buf) {
    fgets(buf, MAX_LENGTH_3, stdin);
}

void myshell_parseInput(char *buf, char **args, const char *delim) {
    // Variables
    int i = 0;
    char *token;

    // Remove leading and trailing whitespace
    while (isspace((unsigned char)*buf)) {
        buf++;
    }
    while (buf[strlen(buf) - 1] == '\n' || buf[strlen(buf) - 1] == ' ') {
        buf[strlen(buf) - 1] = '\0';
    }

    // Tokenize the input string
    token = strtok(buf, delim);
    while (token != NULL) {
        // Remove leading and trailing whitespace
        while (isspace((unsigned char)*token)) {
            token++;
        }
        while (token[strlen(token) - 1] == '\n' || token[strlen(token) - 1] == ' ') {
            token[strlen(token) - 1] = '\0';
        }

        // Store the token in the args array
        args[i++] = token;

        // Get the next token
        token = strtok(NULL, delim);
    }
    args[i] = NULL;  // Null-terminate the args array
}

void myshell_execCommand(char **commands) {
    int i = 0;
    int status, background, last_token;
    int prev_pipe[2] = {-1, -1};
    int curr_pipe[2];
    pid_t pid;
    char *tokens[MAX_LENGTH], pid_str[10];

    // 명령어가 없으면 종료
    if (commands[0] == NULL) {
        return;
    }

    // 각 명령어 처리
    while (commands[i] != NULL) {

        // 현재 명령어를 토큰으로 분리
        memset(tokens, 0, sizeof(tokens));
        myshell_parseInput(commands[i], tokens, " ");

        last_token = 0;
        background = 0;

        // 마지막 토큰이 '&'이면 백그라운드 실행
        // '&'가 명령어와 붙어 있는 경우 처리 ex. "ls -l&"
        while (tokens[last_token] != NULL) {
            last_token++;
        }
        if (last_token > 0 && !strcmp(tokens[last_token - 1], "&")) {
            background = 1;
            tokens[last_token - 1] = NULL;  // '&' 제거
        } else if(last_token > 0) {
            if(strlen(tokens[last_token-1]) && tokens[last_token-1][strlen(tokens[last_token-1])-1] == '&') {
                background = 1;
                tokens[last_token - 1][strlen(tokens[last_token-1]) - 1] = '\0';  // '&' 제거
            }
        }

        // built-in 명령어는 사전 처리
        if (!strcmp(tokens[0], "exit")) {
            _exit(EXIT_SUCCESS);
        } else if (!strcmp(tokens[0], "cd")) {
            // Change directory
            if (tokens[1] == NULL) {
                // Change to home directory
                char *home = getenv("HOME");
                if (home == NULL) {
                    perror("cd");
                    continue;
                }
                chdir(home);
            } else {
                // Change to specified directory
                if (chdir(tokens[1]) < 0) {
                    perror("cd");
                    continue;
                }
            }
            i++;
            continue;
        } else if (!strcmp(tokens[0], "jobs")) {
            // 현재 실행 중인 프로세스 목록 출력
            list_jobs();
            i++;
            continue;
        } else if (!strcmp(tokens[0], "fg")) {
            // 포그라운드 프로세스 재개
            int job_num = (tokens[1] != NULL) ? atoi(tokens[1] + 1) : job_count;
            job_t *job = get_job_by_index(job_num);
            if (job != NULL) {
                kill(job->pid, SIGCONT);
                waitpid(job->pid, &status, 0);
                job->running = 0; // Mark job as not running after completion
            }
            i++;
            continue;
        } else if (!strcmp(tokens[0], "bg")) {
            // 백그라운드 프로세스 재개
            int job_num = (tokens[1] != NULL) ? atoi(tokens[1] + 1) : job_count;
            job_t *job = get_job_by_index(job_num);
            if (job != NULL) {
                kill(job->pid, SIGCONT);
                job->running = 1; // Mark job as running
            }
            i++;
            continue;
        } else if (!strcmp(tokens[0], "kill")) {
            if (tokens[1] != NULL && tokens[1][0] == '%') {
                int job_num = atoi(tokens[1] + 1);
                job_t *job = get_job_by_index(job_num);
                if (job != NULL) {
                    kill(job->pid, SIGTERM);
                    job->running = 0;
                    printf("Job [%d] (%d) terminated\n", job_num, job->pid);
                }
            }
            i++;
            continue;
        }

        // 다음 명령어가 있으면 새 파이프 생성
        if (commands[i+1] != NULL) {
            if (pipe(curr_pipe) < 0) {
                perror("pipe");
                _exit(EXIT_FAILURE);
            }
        }

        // 자식 프로세스 생성
        pid = fork();
        if (pid < 0) {
            perror("fork");
            _exit(EXIT_FAILURE);
        }
        // 자식 프로세스
        if (pid == 0) {

           // 이전 파이프에서 입력 리다이렉션
            if (prev_pipe[0] != -1) {
                dup2(prev_pipe[0], STDIN_FILENO);
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }

            // 다음 명령어가 있으면 출력 리다이렉션
            if (commands[i+1] != NULL) {
                close(curr_pipe[0]);
                dup2(curr_pipe[1], STDOUT_FILENO);
                close(curr_pipe[1]);
            }

            // 백그라운드 프로세스인 경우, SIGINT와 SIGTSTP 시그널을 무시함
            if (background) {
                Signal(SIGINT, SIG_IGN);
                Signal(SIGTSTP, SIG_IGN);
            }

            // 명령어 실행
            myshell_handleRedirection(tokens);  // 리다이렉션 처리
            setpgid(0, 0);  // 자식 프로세스의 그룹 ID 설정
            execvp(tokens[0], tokens);
            perror(tokens[0]);
            _exit(EXIT_FAILURE);
        }
        else {
            // 이전 파이프 닫기
            if (prev_pipe[0] != -1) {
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }
            // 현재 파이프를 이전 파이프로 설정
            if (commands[i+1] != NULL) {
                prev_pipe[0] = curr_pipe[0];
                prev_pipe[1] = curr_pipe[1];
            }
            else {
                // 마지막 명령어면 자식 프로세스 기다림
                if (background) {
                    // 백그라운드 프로세스인 경우, PID 출력
                    sprintf(pid_str, "%d", pid);
                    write(STDOUT_FILENO, pid_str, strlen(pid_str));
                    write(STDOUT_FILENO, "\n", 1);
                    add_job(pid, commands[i]); // Add job to job list
                } else {
                    waitpid(pid, &status, 0);
                }
            }
        }
        i++;
    }
}

// 리다이렉션 처리 함수
void myshell_handleRedirection(char **tokens) {
    int i = 0;
    int fd;

    // 모든 토큰 검사
    while (tokens[i] != NULL) {
        // 입력 리다이렉션
        if (strcmp(tokens[i], "<") == 0 && tokens[i + 1] != NULL) {
            fd = open(tokens[i + 1], O_RDONLY);
            if (fd < 0) {
                perror(tokens[i + 1]);
                _exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            tokens[i] = NULL;  // 리다이렉션 기호와 파일명 제거
        }
        // 출력 리다이렉션
        else if (strcmp(tokens[i], ">") == 0 && tokens[i + 1] != NULL) {
            // 0644 = 0400(User read) + 0200(User write) + 0040(Group read) + 0004(Others read)
            fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror(tokens[i + 1]);
                _exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            tokens[i] = NULL;  // 리다이렉션 기호와 파일명 제거
        }
        i++;
    }
}

void add_job(pid_t pid, const char *cmdline) {
    if (job_count < MAX_JOBS) {
        job_list[job_count].pid = pid;
        strncpy(job_list[job_count].cmdline, cmdline, MAX_LENGTH_2);
        job_list[job_count].running = 1;
        job_count++;
    }
}

void list_jobs() {
    for (int i = 0; i < job_count; ++i) {
        if (job_list[i].running)
            printf("[%d] %d\t%s\n", i+1, job_list[i].pid, job_list[i].cmdline);
    }
}

job_t *get_job_by_index(int idx) {
    if (idx >= 1 && idx <= job_count)
        return &job_list[idx-1];
    return NULL;
}


void myshell_SIGINT(int signal) {
    write(STDOUT_FILENO, "SIGINT received\n", 17);
    // 프롬프트로 돌아가기
    // SIGINT 시그널을 무시하고 프롬프트로 돌아감
    _exit(EXIT_SUCCESS);

}
void myshell_SIGCHLD(int signal) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Job control: mark the job as not running
        for (int i = 0; i < job_count; i++) {
            if (job_list[i].pid == pid) {
                job_list[i].running = 0;
                break;
            }
        }
    }
}
void myshell_SIGTSTP(int signal) {
    if (job_count > 0) {
        for (int i = job_count - 1; i >= 0; --i) {
            if (job_list[i].running) {
                kill(job_list[i].pid, SIGTSTP);
                printf("\n[Job %d] (%d) stopped and moved to background\n", i + 1, job_list[i].pid);
                break;
            }
        }
    }
}
void myshell_SIGTERM(int signal) {
    //write(STDOUT_FILENO, "SIGTERM received\n", 17);
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