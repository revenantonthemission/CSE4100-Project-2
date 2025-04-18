# My Shell (Phase 2)

## 주요 기능
Phase 1의 기능에 파이프(|)와 리다이렉션(>, <) 기능이 추가되었다.
### 1. 기본 명령어 실행
* Unix 기본 명령어 실행 (ls, ps, pwd 등)
* 내장 명령어 지원
  - `cd`: 디렉토리 변경
  - `exit`: 쉘 종료

### 2. 파이프 기능
* 여러 명령어를 파이프(|)로 연결하여 실행 가능
* 예시:
```bash
CSE4100-SP-P2> ls -l | grep .txt | wc -l
```

### 3. 리다이렉션 기능
* 입력 리다이렉션 (<) 지원
* 출력 리다이렉션 (>) 지원
* 예시:
```bash
CSE4100-SP-P2> ls > output.txt
CSE4100-SP-P2> sort < input.txt
```

## 구현된 함수들

### 1. myshell_readInput
```c
void myshell_readInput(char *buf);
```
* 사용자로부터 명령어를 입력받는 함수
* `fgets`를 사용하여 입력을 받음

### 2. myshell_parseInput
```c
void myshell_parseInput(char *buf, char **args, const char *delim);
```
* 입력된 문자열을 파싱하는 함수
* 주어진 구분자(파이프, 공백)를 기준으로 문자열을 토큰화
* 앞뒤 공백 제거 기능 포함

### 3. myshell_execCommand
```c
void myshell_execCommand(char **commands);
```
* 명령어를 실행하는 메인 함수
* 파이프 처리
* 자식 프로세스 생성 및 관리
* 내장 명령어 처리

### 4. myshell_handleRedirection
```c
void myshell_handleRedirection(char **tokens);
```
* 입출력 리다이렉션을 처리하는 함수
* 파일 디스크립터 관리

## 프로그램 제한사항
* 최대 명령어 길이: 32768 바이트 (`MAX_LENGTH_3`)
* 최대 인자 개수: 32개 (`MAX_LENGTH`)
* 최대 토큰 길이: 1024 바이트 (`MAX_LENGTH_2`)

## 에러 처리
* 파일 열기 실패
* 명령어 실행 실패
* 파이프 생성 실패
* fork 실패

## 주의사항
* 리다이렉션과 파이프를 함께 사용할 때는 순서에 주의
* 모든 파일 디스크립터는 적절히 닫아야 함
* 좀비 프로세스 방지를 위한 wait 처리 필요
* 메모리 누수 방지를 위한 적절한 자원 해제 필요