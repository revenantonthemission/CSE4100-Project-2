#include "myshell.h"

int main() {
    // Variables
    char cmdline[MAX_LENGTH_3], *commands[MAX_LENGTH];
    int i;
    sigset_t mask, prev;

    // Initialize signal set
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);

    // Signal handling
    if (signal(SIGINT, myshell_SIGINT) == SIG_ERR) {
        perror("signal");
        _exit(EXIT_FAILURE);
    }
    if (signal(SIGCHLD, myshell_SIGCHLD) == SIG_ERR) {
        perror("signal");
        _exit(EXIT_FAILURE);
    }
    
    sigprocmask(SIG_SETMASK, &prev, NULL);

    do {
        // Initialize
        memset(cmdline, '\0', MAX_LENGTH_3);
        memset(commands, 0, MAX_LENGTH);

        // Shell Prompt: print your prompt
        write(STDOUT_FILENO, "CSE4100-SP-P3> ", 15);
        fflush(stdout);

        // Reading: Read the command from standard input
        myshell_readInput(cmdline);

        // Parsing: Parse the command
        cmdline[strlen(cmdline) - 1] = ' ';
        myshell_parseInput(cmdline, commands, "|");

        // Execute the command
        myshell_execCommand(commands);

        // Clear the buffers
        fflush(stdin);
        fflush(stdout);

    } while (1);

    return 0;
}

/* This function reads input from the command line */
void myshell_readInput(char *buf) {
    // fgets(buf, MAX_LENGTH_3, stdin);
    read(STDIN_FILENO, buf, MAX_LENGTH_3);
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
        background = 0;
        last_token = 0;
        // 마지막 토큰이 '&'이면 백그라운드 실행
        while (tokens[last_token] != NULL) {
            last_token++;
        }
        if (last_token > 0 && tokens[last_token - 1][strlen(tokens[last_token - 1]) - 1] == '&') {
            background = 1;
            tokens[last_token - 1] = NULL;  // '&' 제거
        }

        // 빌트인 명령어 처리
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
            } else if (background) {
                // 마지막 명령어가 백그라운드 실행이면 자식 프로세스 기다리지 않음
                write(STDOUT_FILENO, "Background process started > PID: ", 34);
                sprintf(pid_str, "%d\n", pid);
                write(STDOUT_FILENO, pid_str, strlen(pid_str));
                fflush(stdout);
            } else {
                // 마지막 명령어면 자식 프로세스 기다림
                waitpid(pid, &status, 0);
            }
        }

        i++;
    }

    // 모든 자식 프로세스가 종료될 때까지 기다림
    while (wait(NULL) > 0);
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
        fflush(stdout);
        // 자식 프로세스 종료
        pid = getpid();
        if (kill(pid, SIGKILL) < 0) {
            perror("kill");
            _exit(EXIT_FAILURE);
        }
        // 부모 프로세스 종료
        _exit(0);
    }
}

void myshell_SIGCHLD(int signal) {
    int status, olderr;
    sigset_t mask, prev;
    pid_t pid;
    char pid_str[10];

    // Initialize signal set
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);

    // SIGCHLD 처리
    if (signal == SIGCHLD) {
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            if (WIFEXITED(status)) {
                write(STDOUT_FILENO, "[", 1);
                sprintf(pid_str, "%d", pid);
                write(STDOUT_FILENO, pid_str, strlen(pid_str));
                write(STDOUT_FILENO, "]  child process terminated\n", 28);
                fflush(stdout);
            } else if (WIFSIGNALED(status)) {
                write(STDOUT_FILENO, "[", 1);
                sprintf(pid_str, "%d", pid);
                write(STDOUT_FILENO, pid_str, strlen(pid_str));
                write(STDOUT_FILENO, "]  child process killed by signal\n", 34);
                fflush(stdout);
            } else if (WIFSTOPPED(status)) {
                write(STDOUT_FILENO, "[", 1);
                sprintf(pid_str, "%d", pid);
                write(STDOUT_FILENO, pid_str, strlen(pid_str));
                write(STDOUT_FILENO, "]  child process stopped\n", 25);
                fflush(stdout);
            } else if (WIFCONTINUED(status)) {
                write(STDOUT_FILENO, "[", 1);
                sprintf(pid_str, "%d", pid);
                write(STDOUT_FILENO, pid_str, strlen(pid_str));
                write(STDOUT_FILENO, "]  child process continued\n", 27);
                fflush(stdout);
            }
        }
    }

    // Restore previous signal mask
    sigprocmask(SIG_SETMASK, &prev, NULL);
    errno = olderr;
    return;
}
void myshell_addJob() {
    // Add job to the job list
    // This function is not implemented in this code snippet
    // You can implement it based on your requirements
}
void myshell_deleteJob() {
    // Delete job from the job list
    // This function is not implemented in this code snippet
    // You can implement it based on your requirements
}