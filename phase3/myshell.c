#include "myshell.h"

int main() {
    // 변수 선언
    char cmdline[MAX_LENGTH_3], *commands[MAX_LENGTH];
    int i;
    sigset_t mask, prev;
    
    // 시그널 처리
    if (signal(SIGINT, myshell_SIGINT) == SIG_ERR) {
        perror("signal");
        _exit(EXIT_FAILURE);
    }
    if (signal(SIGCHLD, myshell_SIGCHLD) == SIG_ERR) {
        perror("signal");
        _exit(EXIT_FAILURE);
    }

    do {
        // 초기화
        memset(cmdline, '\0', MAX_LENGTH_3);
        memset(commands, 0, MAX_LENGTH);
        
        // 명령어 읽기
        myshell_readInput(cmdline);

        // 명령어 파싱
        cmdline[strlen(cmdline) - 1] = ' ';
        myshell_parseInput(cmdline, commands, "|");

        // 명령어 실행
        myshell_execCommand(commands);
    
        // 시그널 마스크 설정
        // 버퍼 비우기
        fflush(stdin);
        fflush(stdout);
        
        sleep(1);

    } while (1);

    return 0;
}

/* 명령어 입력을 읽는 함수 */
void myshell_readInput(char *buf) {
    write(STDOUT_FILENO, prompt, strlen(prompt));
    read(STDIN_FILENO, buf, MAX_LENGTH_3);
}

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
        return;
    }

    // 각 명령어 처리
    while (commands[i] != NULL) {
        // 현재 명령어를 토큰으로 분리
        memset(tokens, 0, sizeof(tokens));
        myshell_parseInput(commands[i], tokens, " ");
        background = 0;
        last_token = 0;

        // 마지막 토큰이 '&'이면 백그라운드 실행
        while (tokens[last_token] != NULL) {
            last_token++;
        }

        // '&'가 명령어와 붙어 있는 경우 처리 ex. "ls -l&"
        if (last_token > 0 && !strcmp(tokens[last_token - 1], "&")) {
            background = 1;
            tokens[last_token - 1] = NULL;  // '&' 제거
        } else if(last_token > 0) {
            if(strlen(tokens[last_token-1]) && tokens[last_token-1][strlen(tokens[last_token-1])-1] == '&') {
                background = 1;
                tokens[last_token - 1][strlen(tokens[last_token-1]) - 1] = '\0';  // '&' 제거
            }
        }

        // 빌트인 명령어 처리
        if (!strcmp(tokens[0], "exit")) {
            _exit(EXIT_SUCCESS);
        } else if (!strcmp(tokens[0], "cd")) {
            // 디렉토리 변경
            if (tokens[1] == NULL) {
                // 홈 디렉토리로 변경
                char *home = getenv("HOME");
                if (home == NULL) {
                    perror("cd");
                    //kill(getpid(), SIGCONT);
                    continue;
                }
                chdir(home);
            } else {
                // 지정된 디렉토리로 변경
                if (chdir(tokens[1]) < 0) {
                    perror("cd");
                    //kill(getpid(), SIGCONT);
                    continue;
                }
            }
            i++;
            //kill(getpid(), SIGCONT);
            continue;
        }

        // 다음 명령어가 있으면 새 파이프 생성
        if (commands[i + 1] != NULL) {
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
            if (commands[i + 1] != NULL) {
                close(curr_pipe[0]);
                dup2(curr_pipe[1], STDOUT_FILENO);
                close(curr_pipe[1]);
            }

            if (background) {
                int fd = open("/dev/null", O_RDWR);
                if (fd < 0) {
                    perror("open");
                    _exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);  // 표준 입력 리다이렉션
                close(fd);
            }

            // 명령어 실행
            myshell_handleRedirection(tokens);  // 리다이렉션 처리
            execvp(tokens[0], tokens);
            perror(tokens[0]);
            _exit(EXIT_FAILURE);
        }

        // 부모 프로세스
        else {
            // 이전 파이프 닫기
            if (prev_pipe[0] != -1) {
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }
            // 현재 파이프를 이전 파이프로 설정
            if (commands[i + 1] != NULL) {
                prev_pipe[0] = curr_pipe[0];
                prev_pipe[1] = curr_pipe[1];
            }
            // 백그라운드 프로세스인 경우 작업 목록에 추가
           if (background) {
                // 작업 목록에 추가 (myshell_addJob 함수 구현 필요)
                //myshell_addJob(pid, commands[i]);
                write(STDOUT_FILENO, "Background process started > PID: ", 34);
                sprintf(pid_str, "%d\n", pid);
                write(STDOUT_FILENO, pid_str, strlen(pid_str));
                fflush(stdout);
           }
           else if (commands[i + 1] == NULL) {
                // 마지막 명령어면 자식 프로세스 기다림
                waitpid(pid, &status, 0);
                write(STDOUT_FILENO, "Foreground process terminated > PID: ", 37);
                sprintf(pid_str, "%d\n", pid);
                write(STDOUT_FILENO, pid_str, strlen(pid_str));
                fflush(stdout);
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
            i++;
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
            i++;
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
        _exit(1);
    }
}

// SIGCHLD 처리 함수. 자식 프로세스가 종료되어나 중지되었을 때 호출됨
void myshell_SIGCHLD(int signal) {
    int status, olderr;
    sigset_t mask, prev;
    pid_t pid, ppid;
    char message[MAX_LENGTH_2];
    
    // SIGCHLD 처리
    if (signal == SIGCHLD) {
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            if (WIFEXITED(status)) {
                // 자식 프로세스가 정상 종료된 경우
                sprintf(message, "Child process %d terminated with status %d\n", pid, WEXITSTATUS(status));
                write(STDOUT_FILENO, message, strlen(message));
            } else if (WIFSIGNALED(status)) {
                // 자식 프로세스가 시그널에 의해 종료된 경우
                sprintf(message, "Child process %d killed by signal %d\n", pid, WTERMSIG(status));
                write(STDOUT_FILENO, message, strlen(message));
            } else if (WIFSTOPPED(status)) {
                // 자식 프로세스가 중지된 경우
                sprintf(message, "Child process %d stopped by signal %d\n", pid, WSTOPSIG(status));
                write(STDOUT_FILENO, message, strlen(message));
            }
        }
    }

    errno = olderr;
    return;
}
void myshell_addJob() {
    // 작업 목록에 작업 추가
    // 이 함수는 이 코드 스니펫에서 구현되지 않았습니다.
    // 필요에 따라 구현할 수 있습니다.
}
void myshell_deleteJob() {
    // 작업 목록에서 작업 삭제
    // 이 함수는 이 코드 스니펫에서 구현되지 않았습니다.
    // 필요에 따라 구현할 수 있습니다.
}