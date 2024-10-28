# Implementation

## Process Termination Messages
TODO

## Argument Passing
TODO

## System call
TODO: Syscall Handler
### User Process Manipulation
TODO: `halt`, `exit`, `exec`, `wait`
### File Manipulation

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


#### read
```c
int read (int fd, void *buffer, unsigned size)
```
주어진 FD에 해당하는 파일에서 최대 `size` 바이트 만큼의 데이터를 읽어 `buffer`에 복사하는 시스템 콜. 실제로 파일에서 복사된 바이트 수를 반환한다. 
- FD Table에서 파일 포인터를 참조한 후 `file_read`를 호출한다.
- `fd == 0`일 경우 사용자 입력에서 정보를 읽어온다는 뜻이므로 `input_getc`를 통해 키보드 입력을 받는다.
- 악의적인 호출에 대비하여 `buffer`의 주소가 안전한지 검사한다.


#### write
```c
int write (int fd, const void *buffer, unsigned size)
```
`buffer` 위에 존재하는 데이터를 주어진 FD에 해당하는 파일에 최대 `size` 바이트 만큼 복사하는 시스템 콜. 실제로 파일에 복사된 바이트 수를 반환한다.
- 현재 프로젝트에선 각 파일의 크기가 고정되어있기 때문에, 복사할 데이터의 크기가 파일의 끝을 넘어설 경우 파일의 끝까지만 복사한 뒤 복사된 바이트 수를 반환한다.
- FD Table에서 파일 포인터를 참조한 후 `file_write`를 호출한다.
- `fd == 1`의 경우 `putbuf` 함수를 이용해 콘솔에다 `buffer`의 데이터를 출력한다.
- 악의적인 호출에 대비하여 `buffer`의 주소가 안전한지 검사한다.


#### seek
```c
void seek (int fd, unsigned position)
```
주어진 FD에 해당하는 파일이 현재까지 읽은 데이터의 오프셋을 `position`으로 변경하는 시스템 콜.
- FD Table에서 파일 포인터를 참조한 후 `file_seek`를 호출한다.
- 새 `position`이 기존 파일 길이를 넘어서는 경우에도 정상 동작으로 간주한다.


#### tell
```c
unsigned tell (int fd)
```
주어진 FD에 해당하는 파일이 현재까지 읽은 데이터의 오프셋을 반환하는 시스템 콜.
- FD Table에서 파일 포인터를 참조한 후 `file_tell`을 호출한다.


#### close
```c
void close (int fd)
```
주어진 FD에 해당하는 파일을 찾아 닫아주는 시스템 콜.
- FD Table에서 파일 포인터를 참조한 후 `file_close`를 호출한 뒤, 해당 테이블 엔트리의 `in_use` 플래그를 `false`로 바꾼다.

> 위의 File Manipulation 시스템 콜들은 동시에 여러 프로세스가 동시에 실행할 시 Race Condition이 발생할 우려가 있으므로 Lock 등의 Synchronization 수단을 통해 동시성을 확보해줘야 한다.


## Denying Writes to Executables
디스크 상에 존재하는 유저 프로그램 역시 하나의 파일이기 때문에 동시에 여러 프로세스가 접근하여 Read, Write, Execute하는 것이 가능하다. 하지만 어떤 프로세스가 유저 프로그램을 실행하는 중 다른 프로세스가 해당 프로그램 파일에 Write 하는 등의 수정이 일어날 경우 정상적인 동작이 불가능할 것이다. 때문에 어떤 프로세스가 파일을 실행하고 있을 경우 앞서 살펴본 `file_deny_write`를 호출하여 다른 프로세스가 파일을 변경하는 것을 막고, 실행을 마칠 시 `file_allow_write`를 호출하여 제한을 해제해주는 과정이 수반되어야 한다.

프로세스가 실행 가능한 ELF 파일을 읽어와 스택에 올리는 과정은 `userprog/process.c`의 `load` 함수에서 일어난다. 이때 대상 파일은 `filesys_open(file_name)`으로 명시가 되어있으므로, 파일을 성공적으로 불러온 후 해당 파일에 대해 `file_deny_write`를 호출함으로써 변경 제한을 설정하는 기능은 쉽게 구현이 가능하다.

하지만 프로세스가 종료될 때인 `process_exit`에서는 해당 프로세스가 현재 실행중이었던 파일이 무엇이었는지 알 수 없어 `file_allow_write`를 호출할 수 없다. 이를 위해 `thread` 구조체에 현재 실행중인 파일에 대한 정보를 담고 있는 멤버 변수 `file_exec`을 추가하여 `load` 중 현재 프로세스가 해당 파일을 실행 중임을 명시해주고, `process_exit`에서 어떤 파일을 실행중이었는지를 `file_exec`을 참조하여 알아낸 뒤 `file_allow_write`를 호출해줌으로써 변경 제한을 해제하는 기능을 구현할 예정이다.

TODO: `file_exec`을 `thread` 말고 PCB 정의되면 거기에다 넣을지?