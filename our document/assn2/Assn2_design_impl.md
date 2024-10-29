# Implementation

## Process Termination Messages
후술할 시스템 콜 관련 구현에서 `exit`이 호출되거나 다른 시스템 콜 처리 중 예외 상황이 발생했을 때 스레드를 종료시킬 수 있는 함수를 만들어 정상 종료와 예외 상황을 일괄적으로 처리할 수 있게 구현한다. 프로세스 종료 메세지은 해당 함수의 실행 도중 출력하면 되는데, 프로세스 이름은 `thread_current()->name`으로 불러올 수 있고 exit code는 함수 인자로 전달될 예정이다.

### Argument Passing
현재 user program을 실행하는 구현은 다음과 같다.
- command line에서 parsing해 얻은 `argv[1]`을 `file_name`으로 하여 `process_execute(file_name)`에서 `start_process(file_name_)`을 수행하는 스레드를 생성한다.
- `start_process`에서는 전해 받은 `file_name_`을 이름으로 하는 실행 파일을 load하고 해당 프로그램의 시작 위치로 가 프로그램을 실행한다.
- `process_execute`에 실행해야 하는 파일 명과 argument를 포함한 `file_name`을 넘긴다.
  - 다만 초기 구현에선 `process_execute`와 `start_process`에서 파일 명과 argument가 모두 포함된 파싱되지 않은 `file_name`을 가지고 처리하고 있고, 또한 load한 프로그램에게 argument를 passing해주는 로직도 포함되어 있지 않다.

그렇기에 `process_execute` 또는 `start_process`에서 `file_name`을 parsing하여 파일 이름과 argument로 분리하는 로직을 추가해야 한다. 또한 파일이름을 가지고 executable을 로드 완료한 이후에 parsing해 얻은 argument 및 관련 정보를 executable을 로드한 스레드의 스택에 컨벤션에 맞추어 집어 넣어 user program이 해당 argument를 사용할 수 있도록 해야 한다. 

자세한 구현 계획은 아래와 같다.
- `process_execute(file_name)`에서 `strtok_r`을 이용하여 `file_name`을 `' '`을 기준으로 나누어 맨 앞을 `thread_create`의 스레드 이름으로 사용한다.
	- argument와 파일 이름을 모두 포함한 스레드 이름을 가지는 것이 아닌 파일 이름(실행 프로그램 이름)을 스레드 이름으로 가지게 하기 위함이다.
- `start_process`에서 매개변수로 받은 `file_name_`을 `strtok_r`을 이용하여 `' '`을 기준으로 나누어 맨 앞을 `load`함수 호출 시 파일이름으로 사용한다.
- `start_process`에서 load한 직후, parsing한 `file_name_`의 뒷 부분을 차례로 순회하며 `if_.esp` 로부터 시작해 문자열을 복사해 집어 넣는다.(Ex. `bar -> f00 -> -1 -> /bin/ls\0`순)
	- 각 argument의 끝은 `\0` 존재. 즉 생각하는 길이보다 1 더 길다.
- 앞서 argument로 다양한 문자열을 집어넣었기에 스택에 패딩을 집어넣어 주소가 4-byte allign 될 수 있도록 한다.
- 앞서 집어넣은 argument 문자열의 시작 주소를 집어 넣은 순서대로 차례로 stack에 넣는다. 
- stack에 넣은 argument의 개수(`argc`)를 스택에 넣은 뒤 마지막 return address에 대응되는 NULL 값을 스택에 넣은 뒤 esp를 올바르게 설정한다. 

아래의 표는 `/bin/ls -l foo bar` 커맨드를 실행하였을 때 argument를 올바르게 패싱한 스레드 스택의 모습이다.

| Address    | Name           | Data       | Type          |
| ---------- | -------------- | ---------- | ------------- |
| 0xbffffffc | `argv[3][...]` | bar\0      | `char[4]`     |
| 0xbffffff8 | `argv[2][...]` | foo\0      | `char[4]`     |
| 0xbffffff5 | `argv[1][...]` | -l\0       | `char[3]`     |
| 0xbfffffed | `argv[0][...]` | /bin/ls\0  | `char[8]`     |
| 0xbfffffec | word-align     | 0          | `uint8_t`     |
| 0xbfffffe8 | `argv[4]`      | 0          | `char *`      |
| 0xbfffffe4 | `argv[3]`      | 0xbffffffc | `char *`      |
| 0xbfffffe0 | `argv[2]`      | 0xbffffff8 | `char *`      |
| 0xbfffffdc | `argv[1]`      | 0xbffffff5 | `char *`      |
| 0xbfffffd8 | `argv[0]`      | 0xbfffffed | `char *`      |
| 0xbfffffd4 | `argv`         | 0xbfffffd8 | `char **`     |
| 0xbfffffd0 | `argc`         | 4          | `int`         |
| 0xbfffffcc | return address | 0          | `void (*) ()` |
- 위 구현에 문자열을 보관, 유지하기 위해 page를 allocation해야 할 필요가 있을 수 있다.


### System Call
현재 구현에서 system call 즉 0x30 interrupt에 대한 handler function은 `syscall_handler`로 어떤 system call인지 구분하지 않고 메세지를 출력하고 `thread_exit`을 통해  스레드를 종료한다. 

이를 system call 에 따라 각기 다른 기능을 수행하도록 변경해 구현해야 한다. 

#### System Call Handler
- `syscall_handler`에서 인수로 받은 `intr_frame *f`의 `esp`를 조회하여 32비트 system call number를 얻는다.
	- 해당 esp가 올바른 주소인지 검증하기 위해 
	- 또한 system call에 대한 argument는 system call number 다음 주소에 32비트 간격으로 존재한다.
- 위에서 얻은 system call number를 바탕으로 여러 system call 함수 중 어떤 함수를 실행할지 결정하고 호출해 실행한다.
	- 이 때 함수 호출시 현재 esp 다음 주소를 매개변수로 넘겨준다. 이는 각 system call 함수에서 사용해야 할 argument를 포함할 것이다.
	- 주소로부터 정해진 갯수(각 함수의 argument 개수)의 32비트를 얻을 수 있는 util 함수를 추가한다.
- 함수 호출의 결과를 `intr_frame`의 `eax`에 대입하여 system call의 결과 값을 리턴할 수 있도록 한다.

#### Process Control Block(PCB)
process에 대한 정보를 담은 Process Control Block struct인 `struct pcb`를 `userprog/process.h`에 선언한다.
`struct pcb`
- process identifier인 `pid`
- exitcode `exit_code`
- `wait`, `exit`에서 사용하는 exit code에 대한 semaphore `exit_code_sema`
- 해당 프로세스가 가진 file descriptor의 table인 `fdt`
- 자식 프로세스 list인 `children_list`
- 위 list의 자식 프로세스 list elem인 `child_list_elem`
-  exec 완료를 관리하는 semaphore `load_sema`

userprog를 실행하는 스레드들은 자신을 포함하는 process를 가지게 되고 이를 표현하기 위해 `thread` 구조체에 멤버 변수에 `pcb`에 대한 포인터를 추가해준다.

`create_tid`와 유사하게 unique한 pid를 생성하는 `create_pid`함수를 추가해주어야 한다.
`pcb` 구조체 정보를 초기화하는 `pcb_init` 함수를 추가한다.
- `wait`에 의해 스레드보다 `pcb`가 더 오랫동안 살아있을 수 있기에 `palloc_get_page` 등을 이용해 `pcb`를 위한 공간을 별도로 할당 받아야 한다.
- `create_pid`를 통한 pid 설정, `fdt`, `children_list`, 등의 초기화를 진행한다.
- `load_sema`의 값은 0으로 하여 초기화한다.
	- `start_process`에서 `load` 직후 `load_sema`를 up해준다. 이는 load 완료를 나타내기 위함이다.
#### User Process Manipulation
`void halt(void)`
- pintos를 종료하는 함수이다.
- `shutdown_power_off()`를 호출하여 종료한다.

`pid_t exec(const char *cmd_line)`

주어진 cmd를 수행하는 자식 프로세스를 생성하는 시스템 콜 함수이다.
`load_sema`를 이용해 자식 프로세스의 `load` 완료시점까지 parent process가 syscall을 리턴하지 않게 한다.
- `pcb_init`을 호출하여 새로운 `pcb`를 만들어 초기화한 뒤 `children_list`에 해당 pcb를 추가한다.
- `cmd_line`를 인수로 하여 `process_execute`를 호출한다.
- `pcb`의 `load_sema`를 `down`한다.
	- child process가 로드 완료되면 `up`되며 통과할 수 있게 된다.
- `pcb`의 `pid`를 반환한다.

`void exit(int status)`
- `pcb`의 `exit_code`에 `status`를 넣는다. `exit_code_sema`를 `up`한다.
- `process_exit`을 이용해 process 자원을 모두 반납한다.

`int wait(pid_t pid)`

`pid` 프로세스가 exit할 때 까지 기다리는 시스템 콜 함수이다.
`exit_code_sema`를 통해 자식 프로세스가 exit할 때까지 wait를 리턴하지 않고 기다리다 exit 하면 해당 프로세스(이미 해제됨)의 `pcb`를 할당 해제하고 `exit_code`를 리턴한다.
- 자식 프로세스 중, 즉 `children_list`를 순회하며 `pcb.pid`가 `pid`인 `pcb`를 찾는다.
- 찾은`pcb`의 `exit_code_sema`를 `down`한다.
	- 해당 프로세스가 exit()하면 `up`되어 진행할 수 있다.
- `pcb.exit_code`를 `exit_code` 임시 변수에 저장한다.
- `pcb`의 페이지를 해제하고 `exit_code`를 반환한다.

#### File Descriptor (FD)
UNIX 기반 운영체제에서 각 프로세스는 현재 open한 파일들에 접근하기 위해 각 파일마다 특정한 정수 키(key)를 대응시키는데, 이를 File Descriptor(FD)라고 한다. 일반적으로 시스템상 사전 정의된 일부 FD들을 제외하면 나머지 FD들은 프로세스마다 독립적으로 관리되기 때문에, PCB 위의 고정된 길이의 배열 (File Descriptor Table) 형태로 선언되어 관리한다. File Descriptor Table의 각 엔트리는 다음과 같이 구상해볼 수 있다.
```c
struct fdt_entry
  {
    struct file *file;
    bool in_use;
  };
```
`*file`은 해당 인덱스의 엔트리가 가리키는 파일, `in_use`는 해당 fd가 현재 할당된 상태인지를 나타낸다.

이를 이용하여 File Descriptor Table은 다음과 같이 `fdt_entry`의 정적 배열로 나타낼 수 있다.
```c
struct fdt_entry fd_table[OPEN_MAX];
```
FD를 해당 테이블의 인덱스로 사용하여 접근함으로써 대응하는 파일에 접근할 수 있다.
- `OPEN_MAX`는 한 프로세스가 동시에 열 수 있는 최대 파일 개수로, 운영체제 내에서 전처리된 상수이다.


#### create
```c
bool create (const char *file, unsigned initial_size)
```
명시된 이름(`file`)과 크기(`initial_size`)를 가진 파일을 생성한 뒤 성공 여부를 반환하는 시스템 콜.
- `filesys_create`와 동일한 동작이므로 이를 호출한다.

#### remove
```c
bool remove (const char *file)
```
명시된 이름(`file`)을 가진 파일을 찾아 삭제한 뒤 성공 여부를 반환하는 시스템 콜. 
- `filesys_remove`와 동일한 동작이므로 이를 호출한다.

#### open
```c
int open (const char *file)
```
명시된 이름(`file`)을 가진 파일을 찾아 오픈한 뒤 해당 파일에 부여된 FD를 반환하는 시스템 콜.
- `filesys_open`을 호출하여 해당하는 파일을 찾는다.
- 현재 프로세스의 File Descriptor Table를 인덱스 0부터 순회한다.
- 사용 가능한 (`in_use == false`) 첫 엔트리를 발견할 시 엔트리에 파일 관련 정보를 할당한 후 해당 인덱스를 FD로써 반환한다.
- 한 프로세스 내에서 동일한 파일이 여러번 열리더라도 매번 새로운 FD를 할당하여 반환해야 한다.
- 실패 시 `-1`을 반환한다.


#### filesize
```c
int filesize (int fd)
```
주어진 FD에 해당하는 파일의 크기를 바이트 단위로 반환하는 시스템 콜.
- FD Table에서 파일 포인터를 참조한 후 `file_length` 함수의 반환값을 반환한다.
- 실패 시 에러 코드와 함께 프로세스를 종료한다.


#### read
```c
int read (int fd, void *buffer, unsigned size)
```
주어진 FD에 해당하는 파일에서 최대 `size` 바이트 만큼의 데이터를 읽어 `buffer`에 복사하는 시스템 콜. 실제로 파일에서 복사된 바이트 수를 반환한다. 
- FD Table에서 파일 포인터를 참조한 후 `file_read`를 호출한다.
- 실패 시 `-1`을 반환한다.
- `fd == 0`일 경우 사용자 입력에서 정보를 읽어온다는 뜻이므로 `input_getc`를 통해 키보드 입력을 받는다.
- 악의적인 호출에 대비하여 `buffer`의 주소가 안전한지 검사한다.


#### write
```c
int write (int fd, const void *buffer, unsigned size)
```
`buffer` 위에 존재하는 데이터를 주어진 FD에 해당하는 파일에 최대 `size` 바이트 만큼 복사하는 시스템 콜. 실제로 파일에 복사된 바이트 수를 반환한다.
- 현재 프로젝트에선 각 파일의 크기가 고정되어있기 때문에, 복사할 데이터의 크기가 파일의 끝을 넘어설 경우 파일의 끝까지만 복사한 뒤 복사된 바이트 수를 반환한다.
- FD Table에서 파일 포인터를 참조한 후 `file_write`를 호출한다.
- 실패 시 에러 코드와 함께 프로세스를 종료한다.
- `fd == 1`의 경우 `putbuf` 함수를 이용해 콘솔에다 `buffer`의 데이터를 출력한다.
- 악의적인 호출에 대비하여 `buffer`의 주소가 안전한지 검사한다.


#### seek
```c
void seek (int fd, unsigned position)
```
주어진 FD에 해당하는 파일이 현재까지 읽은 데이터의 오프셋을 `position`으로 변경하는 시스템 콜.
- FD Table에서 파일 포인터를 참조한 후 `file_seek`를 호출한다.
- 새 `position`이 기존 파일 길이를 넘어서는 경우에도 정상 동작으로 간주한다.
- 실패 시 에러 코드와 함께 프로세스를 종료한다.


#### tell
```c
unsigned tell (int fd)
```
주어진 FD에 해당하는 파일이 현재까지 읽은 데이터의 오프셋을 반환하는 시스템 콜.
- FD Table에서 파일 포인터를 참조한 후 `file_tell`을 호출한다.
- 실패 시 에러 코드와 함께 프로세스를 종료한다.


#### close
```c
void close (int fd)
```
주어진 FD에 해당하는 파일을 찾아 닫아주는 시스템 콜.
- FD Table에서 파일 포인터를 참조한 후 `file_close`를 호출한 뒤, 해당 테이블 엔트리의 `in_use` 플래그를 `false`로 바꾼다.
- 실패 시 에러 코드와 함께 프로세스를 종료한다.

> 위의 File Manipulation 시스템 콜들은 동시에 여러 프로세스가 동시에 실행할 시 Race Condition이 발생할 우려가 있으므로 Lock 등의 Synchronization 수단을 통해 동시성을 확보해줘야 한다.
>
> 시스템 콜 도중 예외 상황 시 동작은 우선 Pintos 문서에 명시되어있는 동작을 따르되, 별다른 명시가 없을 경우 프로세스를 종료하는 방식을 따른다.


## Denying Writes to Executables
디스크 상에 존재하는 유저 프로그램 역시 하나의 파일이기 때문에 동시에 여러 프로세스가 접근하여 Read, Write, Execute하는 것이 가능하다. 하지만 어떤 프로세스가 유저 프로그램을 실행하는 중 다른 프로세스가 해당 프로그램 파일에 Write 하는 등의 수정이 일어날 경우 정상적인 동작이 불가능할 것이다. 때문에 어떤 프로세스가 파일을 실행하고 있을 경우 앞서 살펴본 `file_deny_write`를 호출하여 다른 프로세스가 파일을 변경하는 것을 막고, 실행을 마칠 시 `file_allow_write`를 호출하여 제한을 해제해주는 과정이 수반되어야 한다.

프로세스가 실행 가능한 ELF 파일을 읽어와 스택에 올리는 과정은 `userprog/process.c`의 `load` 함수에서 일어난다. 이때 대상 파일은 `filesys_open(file_name)`으로 명시가 되어있으므로, 파일을 성공적으로 불러온 후 해당 파일에 대해 `file_deny_write`를 호출함으로써 변경 제한을 설정하는 기능은 쉽게 구현이 가능하다.

하지만 프로세스가 종료될 때인 `process_exit`에서는 해당 프로세스가 현재 실행중이었던 파일이 무엇이었는지 알 수 없어 `file_allow_write`를 호출할 수 없다. 이를 위해 `thread` 구조체에 현재 실행중인 파일에 대한 정보를 담고 있는 멤버 변수 `file_exec`을 추가하여 `load` 중 현재 프로세스가 해당 파일을 실행 중임을 명시해주고, `process_exit`에서 어떤 파일을 실행중이었는지를 `file_exec`을 참조하여 알아낸 뒤 `file_allow_write`를 호출해줌으로써 변경 제한을 해제하는 기능을 구현할 예정이다.

TODO: `file_exec`을 `thread` 말고 PCB 정의되면 거기에다 넣을지?