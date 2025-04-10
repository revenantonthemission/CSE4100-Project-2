#include "myshell.h"

int main() {
    // Variables
    char cmdline[MAX_LENGTH_3], *commands[MAX_LENGTH];
    int i;

    do {
        // Initialize
        memset(cmdline, '\0', MAX_LENGTH_3);
        memset(commands, 0, MAX_LENGTH);

        // Shell Prompt: print your prompt
        fputs("CSE4100-SP-P2> ", stdout);
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
    int status;
    int prev_pipe[2] = {-1, -1};
    int curr_pipe[2];
    pid_t pid;
    char *tokens[MAX_LENGTH];

    // 명령어가 없으면 종료
    if (commands[0] == NULL) {
        return;
    }

    // 각 명령어 처리
    while (commands[i] != NULL) {
        // 현재 명령어를 토큰으로 분리
        memset(tokens, 0, sizeof(tokens));
        myshell_parseInput(commands[i], tokens, " ");

        // 빌트인 명령어 처리
        if (!strcmp(tokens[0], "exit")) {
            exit(EXIT_SUCCESS);
        } else if (!strcmp(tokens[0], "cd")) {
            // Change directory
            if (tokens[1] == NULL) {
                // Change to home directory
                char *home = getenv("HOME");
                if (home == NULL) {
                    fprintf(stderr, "cd: HOME not set\n");
                    continue;
                }
                chdir(home);
            } else {
                // Change to specified directory
                if (chdir(tokens[1]) < 0) {
                    fprintf(stderr, "cd: %s: No such file or directory\n", tokens[1]);
                    continue;
                }
            }
            i++;
            continue;
        }

        // 다음 명령어가 있으면 새 파이프 생성
        if (commands[i+1] != NULL) {
            if (pipe(curr_pipe) < 0) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        // 자식 프로세스 생성
        pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
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

            // 명령어 실행
            myshell_handleRedirection(tokens);  // 리다이렉션 처리
            execvp(tokens[0], tokens);
            perror(tokens[0]);
            exit(EXIT_FAILURE);
        }
        // 부모 프로세스
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
        if (strcmp(tokens[i], "<") == 0 && tokens[i+1] != NULL) {
            fd = open(tokens[i+1], O_RDONLY);
            if (fd < 0) {
                perror(tokens[i+1]);
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            tokens[i] = NULL;  // 리다이렉션 기호와 파일명 제거
            i++;
        }
        // 출력 리다이렉션
        else if (strcmp(tokens[i], ">") == 0 && tokens[i+1] != NULL) {
            fd = open(tokens[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror(tokens[i+1]);
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            tokens[i] = NULL;  // 리다이렉션 기호와 파일명 제거
            i++;
        }
        i++;
    }
}