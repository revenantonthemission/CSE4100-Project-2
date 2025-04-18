# My Shell (Phase 3)

## 주요 기능
Phase 2의 기능에 작업 관리(Job Control) 기능이 추가되었다.

### 1. 기본 명령어 실행
* Unix 기본 명령어 실행 (ls, ps, pwd 등)
* 내장 명령어 지원
  - `cd`: 디렉토리 변경
  - `exit`: 쉘 종료
  - `jobs`: 현재 작업 목록 출력
  - `fg`: 백그라운드 작업을 포그라운드로 전환
  - `bg`: 중지된 작업을 백그라운드로 실행
  - `kill`: 작업 종료

### 2. 작업 제어 기능
* 백그라운드 실행 (`&`)
* 작업 일시 중지 (`Ctrl+Z`)
* 작업 종료 (`Ctrl+C`)
* 중지된 작업 재개 (`fg`, `bg`)
* 작업 상태 확인 (`jobs`)

### 3. 파이프와 리다이렉션
* 파이프(|) 지원
* 입출력 리다이렉션(>, <) 지원

## 구현된 함수들

### 1. 작업 제어 관련 함수
```c
void init_job();              // 작업 목록 초기화
void list_jobs();            // 작업 목록 출력
void add_job();              // 새 작업 추가
void delete_job();           // 작업 제거
job_t *get_job_by_index();   // 작업 번호로 작업 검색
```

### 2. 시그널 핸들러
```c
void myshell_SIGINT();    // Ctrl+C 처리
void myshell_SIGTSTP();   // Ctrl+Z 처리
void myshell_SIGCHLD();   // 자식 프로세스 종료 처리
void myshell_SIGCONT();   // 작업 재개 처리
```

### 3. 명령어 처리 함수
```c
void myshell_readInput();          // 입력 처리
void myshell_parseInput();         // 명령어 파싱
void myshell_execCommand();        // 명령어 실행
void myshell_handleRedirection();  // 리다이렉션 처리
```

## 프로그램 제한사항
* 최대 명령어 길이: 32768 바이트 (`MAX_LENGTH_3`)
* 최대 인자 개수: 32개 (`MAX_LENGTH`)
* 최대 작업 개수: 128개 (`MAX_JOBS`)

## 사용 예시
```bash
# 기본 명령어
CSE4100-SP-P2> ls -l | grep .txt

# 백그라운드 실행
CSE4100-SP-P2> sleep 100&

# 작업 목록 확인
CSE4100-SP-P2> jobs

# 작업 제어
CSE4100-SP-P2> fg %1
CSE4100-SP-P2> bg %1

# 작업 종료
CSE4100-SP-P2> kill %1
```

## 주요 시그널 처리
* `SIGINT (Ctrl+C)`: 현재 실행 중인 포그라운드 작업 종료
* `SIGTSTP (Ctrl+Z)`: 포그라운드 작업 일시 중지
* `SIGCHLD`: 자식 프로세스 종료 처리
* `SIGCONT`: 중지된 작업 재개
* `SIGTTOU`, `SIGTTIN`: 백그라운드 프로세스의 터미널 접근 제어

## 주의사항
* 프로세스 그룹 관리와 터미널 제어 설정 필요
* 시그널 핸들러의 재진입 가능성 고려
* 좀비 프로세스 방지를 위한 SIGCHLD 처리
* 파일 디스크립터 누수 방지
* 메모리 누수 방지