# My Shell (Phase 1)

## 주요 기능

### 1. 명령어 실행

* 기본적인 Unix 명령어 실행 (ls, ps, pwd 등)
* Built-in 명령어 지원
  - `cd`: 디렉토리 변경
  - `exit`: 쉘 종료

### 2. 내장 명령어 상세
* `cd`: 
  - 인자 없이 실행 시 HOME 디렉토리로 이동
  - 경로 지정 시 해당 경로로 이동
* `exit`: 
  - 쉘 프로그램 종료

## 구현된 함수들

### 1. myshell_readInput
```c
void myshell_readInput(char *buf);
```
* 사용자로부터 명령어를 입력받는 함수
* `fgets`를 사용하여 입력을 받음

### 2. myshell_parseInput
```c
void myshell_parseInput(char *buf, char **args);
```
* 입력된 문자열을 파싱하는 함수
* 공백을 기준으로 문자열을 토큰화
* NULL로 끝나는 문자열 배열 생성

### 3. myshell_execCommand
```c
void myshell_execCommand(char **args);
```
* 명령어를 실행하는 메인 함수
* `fork()`를 통한 자식 프로세스 생성
* `execvp()`를 사용한 명령어 실행
* 부모 프로세스의 자식 프로세스 대기

## 프로그램 제한사항
* 최대 명령어 길이: 8192 바이트 (`MAX_LINE`)
* 최대 인자 개수: 128개 (`MAX_LENGTH`)

## 사용 예시
```bash
CSE4100-SP-P2> ls
CSE4100-SP-P2> cd /home
CSE4100-SP-P2> pwd
CSE4100-SP-P2> exit
```

## 에러 처리
* 존재하지 않는 명령어 실행 시 에러 메시지 출력
* 디렉토리 변경 실패 시 에러 메시지 출력
* HOME 환경변수가 설정되지 않은 경우 에러 메시지 출력


## 주의사항
* `fork()`와 `execvp()` 실패 시 적절한 에러 처리 필요
* 자식 프로세스 종료 시 좀비 프로세스 방지를 위한 `waitpid()` 호출 필요
* 메모리 누수 방지를 위한 적절한 자원 해제 필요