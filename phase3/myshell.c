#include "myshell.h"

int main() {
    // 변수 선언
    char cmdline[MAX_LENGTH_3], *commands[MAX_LENGTH];
    int i;

    // 시그널 처리
    if (signal(SIGINT, myshell_SIGINT) == SIG_ERR) {
        perror("signal");
        _exit(EXIT_FAILURE);
    }
    if (signal(SIGCHLD, myshell_SIGCHLD) == SIG_ERR) {
        perror("signal");
        _exit(EXIT_FAILURE);
    }
    if (signal(SIGTSTP, myshell_SIGTSTP) == SIG_ERR) {
        perror("signal");
        _exit(EXIT_FAILURE);
    }

    do {
        // 초기화
        memset(cmdline, '\0', MAX_LENGTH_3);
        memset(commands, 0, MAX_LENGTH);

        // 버퍼 비우기
        fflush(stdin);
        fflush(stdout);
        

        // jobs 배열 초기화
        myshell_initJobs();

        // 명령어 읽기 전에 프롬프트 출력 (중복 출력 방지)
        if (prompt_ready) {
            write(STDOUT_FILENO, prompt, strlen(prompt));
            prompt_ready = 0;
        }
        
        // 명령어 읽기
        myshell_readInput(cmdline);

        // 명령어 파싱
        cmdline[strlen(cmdline) - 1] = ' ';
        myshell_parseInput(cmdline, commands, "|");

        // 명령어 실행
        myshell_execCommand(commands);

    } while (1);

    return 0;
}

/* 명령어 입력을 읽는 함수 */
void myshell_readInput(char *buf) {
    read(STDIN_FILENO, buf, MAX_LENGTH_3);
}

/* 명령어를 파싱하는 함수 */
void myshell_parseInput(char *buf, char **args, const char *delim) {
    // 변수 선언
    int i = 0;
    char *token;

    // 앞뒤 공백 제거
    while (isspace((unsigned char)*buf)) {
        buf++;
    }
    while (buf[strlen(buf) - 1] == '\n' || buf[strlen(buf) - 1] == ' ') {
        buf[strlen(buf) - 1] = '\0';
    }

    // 입력 문자열을 토큰화
    token = strtok(buf, delim);
    while (token != NULL) {
        // 앞뒤 공백 제거
        while (isspace((unsigned char)*token)) {
            token++;
        }
        while (token[strlen(token) - 1] == '\n' || token[strlen(token) - 1] == ' ') {
            token[strlen(token) - 1] = '\0';
        }

        // 토큰을 args 배열에 저장
        args[i++] = token;

        // 다음 토큰 가져오기
        token = strtok(NULL, delim);
    }
    args[i] = NULL;  // args 배열을 널로 종료
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
        prompt_ready = 1;
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

        //built-in 명령어는 사전 처리
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
            prompt_ready = 1;
            continue;
        } else if (!strcmp(tokens[0], "jobs")) {
            // 작업 목록 출력
            myshell_listJobs();
            prompt_ready = 1;
            i++;
            continue;
        } else if (!strcmp(tokens[0], "fg")) {
            // 포그라운드 프로세스 재개
            if (tokens[1] != NULL) {
                pid_t fg_pid = atoi(tokens[1]);
                myshell_fgJob(fg_pid);
            }
            prompt_ready = 1;
            i++;
            continue;
        } else if (!strcmp(tokens[0], "bg")) {
            // 백그라운드 프로세스 재개
            if (tokens[1] != NULL) {
                pid_t bg_pid = atoi(tokens[1]);
                myshell_bgJob(bg_pid);
            }
            prompt_ready = 1;
            i++;
            continue;
        } else if (!strcmp(tokens[0], "kill")) {
            // 프로세스 종료
            if (tokens[1] != NULL) {
                pid_t kill_pid = atoi(tokens[1]);
                myshell_killJob(kill_pid);
            }
            prompt_ready = 1;
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

            // 백그라운드 프로세스인 경우, SIGINT와 SIGTSTP 시그널 무시
            if (background) {
                signal(SIGINT, SIG_IGN);
                signal(SIGTSTP, SIG_IGN);
            }

            // 명령어 실행
            myshell_handleRedirection(tokens);  // 리다이렉션 처리
            execvp(tokens[0], tokens);
            perror(tokens[0]);
            _exit(EXIT_FAILURE);
        }
        // 부모 프로세스
        else {
            // jobs 배열에 작업 추가
            if (background) {
                myshell_addJob(getpid(), BG, commands[i]);
            } else {
                myshell_addJob(getpid(), FG, commands[i]);
            }
            // 이전 파이프 닫기
            if (prev_pipe[0] != -1) {
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }
            // 현재 파이프를 이전 파이프로 설정
            if (commands[i+1] != NULL) {
                prev_pipe[0] = curr_pipe[0];
                prev_pipe[1] = curr_pipe[1];
            } else { 
                // 마지막 명령어면 자식 프로세스 기다림
                if (!background) {
                    myshell_waitForJob(pid);
                } else {
                    // 백그라운드 프로세스인 경우, PID 출력
                    write(STDOUT_FILENO, "\n", 1);
                    write(STDOUT_FILENO, "Background process started > PID: ", 35);
                    sprintf(pid_str, "%d\n", pid);
                    write(STDOUT_FILENO, pid_str, strlen(pid_str));
                }
                // jobs 배열에서 작업 삭제
                prompt_ready = 1;
                return;
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

void myshell_SIGINT(int signal) {
    int olderr = errno;
    pid_t pid;

    // SIGINT 처리
    if (signal == SIGINT) {
        write(STDOUT_FILENO, "\nSIGINT received. Exiting...\n", 30);
        // 모든 자식 프로세스 종료
        for (int i = 0; i < MAXJOBS; i++) {
            if (jobs[i].state != UNDEF) {
                pid = jobs[i].pid;
                kill(pid, SIGKILL);
                jobs[i].state = UNDEF;  // 작업 상태 초기화
            }
        }
    }
}

// SIGCHLD 처리 함수. 자식 프로세스가 종료되어나 중지되었을 때 호출됨
void myshell_SIGCHLD(int signal) {
    int olderr = errno;
    pid_t pid;
    int status;

    // 자식 프로세스가 종료되었을 때
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        // 해당 프로세스가 백그라운드 작업인 경우
        if (WIFEXITED(status) && myshell_getJobState(pid) == BG) {
            write(STDOUT_FILENO, "\nBackground process terminated > PID: ", 37);
            char pid_str[10];
            sprintf(pid_str, "%d\n", pid);
            write(STDOUT_FILENO, pid_str, strlen(pid_str));
            myshell_deleteJob(pid);  // 작업 삭제
        }
        // 포그라운드 프로세스가 중지된 경우
        else if (WIFSTOPPED(status)) {
            write(STDOUT_FILENO, "\nForeground process stopped > PID: ", 36);
            char pid_str[10];
            sprintf(pid_str, "%d\n", pid);
            write(STDOUT_FILENO, pid_str, strlen(pid_str));
            myshell_setJobState(pid, ST);  // 작업 상태 변경
        }
        // 포그라운드 프로세스가 비정상적으로 종료된 경우
        else if (WIFSIGNALED(status)) {
            write(STDOUT_FILENO, "\nForeground process killed > PID: ", 37);
            char pid_str[10];
            sprintf(pid_str, "%d\n", pid);
            write(STDOUT_FILENO, pid_str, strlen(pid_str));
            // 종료 이유 출력
            write(STDOUT_FILENO, "Killed by\n", 10);
            sprintf(pid_str, "%d\n", WTERMSIG(status));
            write(STDOUT_FILENO, pid_str, strlen(pid_str));
            myshell_deleteJob(pid);  // 작업 삭제
        }
    }
    prompt_ready = 1;  // 프롬프트 출력 준비 완료
    errno = olderr;
    return;
}
void myshell_SIGTSTP(int signal) {
    int olderr = errno;
    pid_t pid;
    char pid_str[10];

    // SIGTSTP 처리
    if (signal == SIGTSTP) {
        write(STDOUT_FILENO, "\nSIGTSTP received. Stopping...\n", 31);
        // 모든 자식 프로세스 중지
        for (int i = 0; i < MAXJOBS; i++) {
            if (jobs[i].state != UNDEF) {
                pid = jobs[i].pid;
                kill(pid, SIGSTOP);
                jobs[i].state = ST;  // 작업 상태 변경
            }
        }
        write(STDOUT_FILENO, "Foreground process stopped > PID: ", 34);
        sprintf(pid_str, "%d", pid);
        write(STDOUT_FILENO, pid_str, strlen(pid_str));
        write(STDOUT_FILENO, "\n", 1);
        prompt_ready = 1;
    }
    errno = olderr;
}
void myshell_initJobs(void) {
    for (int i = 0; i < MAXJOBS; i++) {
        jobs[i].pid = 0;
        jobs[i].state = UNDEF;
        jobs[i].cmdline[0] = '\0';
    }
}
void myshell_addJob(pid_t pid, int state, const char* cmd) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].state == UNDEF) {
            jobs[i].pid = pid;
            jobs[i].state = state;
            strncpy(jobs[i].cmdline, cmd, MAX_LENGTH_2);
            jobs[i].cmdline[MAX_LENGTH_2 - 1] = '\0';  // null-terminate
            return;
        }
    }
}
void myshell_deleteJob(pid_t pid) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].pid = 0;
            jobs[i].state = UNDEF;
            jobs[i].cmdline[0] = '\0';
            return;
        }
    }
}
void myshell_listJobs(void) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].state != UNDEF) {
            write(STDOUT_FILENO, jobs[i].cmdline, strlen(jobs[i].cmdline));
            write(STDOUT_FILENO, "\n", 1);
        }
    }
}
void myshell_waitForJob(pid_t pid) {
    int status;
    waitpid(pid, &status, 0);
}
void myshell_killJob(pid_t pid) {
    myshell_deleteJob(pid);
    write(STDOUT_FILENO, "\nProcess killed > PID: ", 23);
    char pid_str[10];
    sprintf(pid_str, "%d\n", pid);
    write(STDOUT_FILENO, pid_str, strlen(pid_str));
    prompt_ready = 1;
    kill(pid, SIGKILL);
}
void myshell_fgJob(pid_t pid) {
    myshell_setJobState(pid, FG);
    write(STDOUT_FILENO, "\nForeground process resumed > PID: ", 36);
    char pid_str[10];
    sprintf(pid_str, "%d\n", pid);
    write(STDOUT_FILENO, pid_str, strlen(pid_str));
    prompt_ready = 1;
    kill(pid, SIGCONT);
}
void myshell_bgJob(pid_t pid) {
    myshell_setJobState(pid, BG);
    write(STDOUT_FILENO, "\nBackground process resumed > PID: ", 35);
    char pid_str[10];
    sprintf(pid_str, "%d\n", pid);
    write(STDOUT_FILENO, pid_str, strlen(pid_str));
    prompt_ready = 1;
    kill(pid, SIGCONT);
}
void myshell_setJobState(pid_t pid, int state) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].state = state;
            return;
        }
    }
}
int myshell_getJobState(pid_t pid) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            return jobs[i].state;
        }
    }
    return UNDEF;
}