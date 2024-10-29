`userprog`에서 주로 일할거임
하지만 pintos의 대부분과 상호작용해야 할 거임

## Background
과제1에서 테스트 코드들은 커널의 일부여서 시스템에 대한 권한 가짐. 그런데 운영체제 위 사용자 프로그램은 작동하지 않을 거임.
이번엔 이것을 다룸.

1 프로세스 이상 실행 가능
멀티스레드 프로세스는 불가능
User program은 머신 전체를 가진다는 환상 아래 작성됨
- 즉 여러 프로그램이 실행 중일 때도 이 환상이 유지되도록 메모리, 스케줄링, 등 잘 관리해야 함.

전 과제는 커널 일부에서 테스트를 진행하였지만 이제는 테스트를 user program을 실행함으로서 진행할 거임.

주어진 인터페이스만 맞춘다면 이전보다 자유롭게 구현 가능.

### Source Files
`userprog` 폴더 다 봐야함.

`process.c, .h`
- ELF(Executable and Linkable Format) 바이너리 로딩하고 process 시작함.
`pagedir.c, .h`
- **이번 플젝에서는 변경할 필요 x**
- 80x86 hardware page table의 간단한 관리자
- 함수는 불러 사용해야함. [4.1.2.3](https://web.stanford.edu/class/cs140/projects/pintos/pintos_4.html#SEC59) 참고
`syscall.c, .h`
- 언제든 유저 프로세스가 kernel functionality 접근하고 싶으면 system call을 invoke(호출)함
- skeleton system call handler임.
- 지금은 단지 메세지 출력, user process terminate함.
- 이번 과제에 system call에 필요한 모든 거 여기에 추가하면 됨.
`exception.c, .h`
- user process가 privileged(권한이 필요한) 또는 prohibited(금지된) operation을 실행하려고 하면, 커널에 exception이나 fault로 trap됨.
- 이건 exception 핸들링함.
- 현재 구현상은 모든 exception이 단순히 메세지 출력, 프로세스 종료함.
- 과제에서는 `page_fault()`를 수정해야할 거임
`gdt.c, .h` 
- 80x86은 segmented architecture임.
- GDT(Global Descriptor Table)은 사용 중인 세그먼트를 설명하는 테이블
- 해당 파일은 GDT를 셋업함.
- **어떤 플젝에서는 변경 필요 x**
- 읽고 싶으면 읽어봐
`tss.c, .h`
- TSS(Task-State Segment)는 80x86 architectural task switching에 사용됨.
- 핀토스에서는 User Process가 interrupt handler에 들어갈 때 stack switching할 때만 사용함.
- **이번 플젝에서 변경 필요 x**
- 읽고 싶으면 읽어봐

### Using the File System
파일 시스템 코드와 인터페이스해야 함.
- user program은 file system으로부터 로드되고 많은 system call이 file system을 다룸
- 근데 이번 플젝은 file system이 핵심이 아니므로 `filesys` 에 온전, 간단한 파일시스템 제공.
- `filesys.h`, `file.h` 을 봐야 할 것.
	- 변경하지 말 것 추천
- 해당 구현은 다음 한계가 있음.
	- No internal synchronization
	- File size is fixed at creation time
	- File data is allocated as a single extent
	- No subdirectories
	- File names are limited to 14 charatcters
	- 작업 중 시스템 충돌 발생시 자동복구 불가하게 디스크 손상 가능
- **중요 특성**
	- `filesys_remove()`에 대해 Unix like한 시맨틱
	- 파일 제거할 때 열려있는 경우, 해당 블록은 할당 해제 되지 않고 마지막 스레드가 파일 닫을 때까지 열려있는 모든 스레드에서 액세스 가능.
- `pintos-mkdisk` 프로그램은 simulated disk with a file system partition을 생성하는 기능 제공
	- Ex) in `userprog/build` `pintos-mkdisk filesys.dsk --filesys-size=2`
		- 2MB 핀토스 파일 시스템 파티션 포함한 `filesys.dsk` 이름의 시뮬레이션 디스크 생성
		- `pintos -f q`
			- `-f`: file system format
			- `-q`: pintos exit as soon as the format is done
	- 이외에도 `-p`(put), `-g`(get) 등 있음.
	- `filesys/fsutil.c`에서 더 자세히 볼수도
	- 뭐 여러 명령어 있다. 

### How User Programs Work
핀토스는 메모리에 맞고 구현한 system call만 사용하는한 일반 c 프로그램을 실행 가능
- 이 프로젝트에서 필요한 시스템 호출 중 memory allocation은 없음.--> `malloc()`은 불가능
- Floating point operation도 불가능. 
	- 커널이 thread switching 때 프로세서의 floating-point unit을 저장, 복구하지 않기에.
- `src/examples`에 몇몇 user program 존재

핀토스는 `userprog/process.c`에 제공된 loader를 사용하여 ELF 로드할 수 있음.
- ELF: Linux, Solaris 및 기타 여러 운영 체제에서 객체 파일, 공유 라이브러리 및 실행 파일에 사용하는 파일 형식
- simulated file system에 test program을 복사해 넣기 전까지 별 작업할 수 없음.
- clean reference file system disk `filesys.dsk`를 복사해두길 추천
### Virtual Memory Layout
핀토스의 virtual memory 두 개의 영역으로 나뉨.
- User virtual memory, kernel virtual memory
User virtual memory
- VA 0 ~ `PHYS_BASE` in `threads/vaddr.h`, default 3GB
- per-process
- kernel이 프로세스 스위치했을 때
	- User virtual address space도 switch by process의 page director base register 변경
		- `pagedir_activate()` in `userprog/pagedir.c` 참고
		- `struct thread`도 process page table에 대한 poitner 들고 있음.
Kernel virtual memory
- VA `PHYS_BASE` ~ 4GB
- global, 항상 같은 방식으로 매핑됨.
	- kernel virtual memory는 physical memory와 one-to-one
	- `PHYS_BASE`에서 시작
		- VA `PHYS_BASE` = PA 0
		- VA `PHYS_BASE+0x1234` = PA `0x1234`
User program은 그들의 user virtual memory만 접근 가능.
kernel virtual memory access 시도시 page fault, 
- by `page_fault()` in `userprog/exception.c`
- 그리고 프로세스 terminated
Kernel thread는 kernel virtual memory 접근 가능
- user process running시 running process의 virtual memory도 접근 가능
- 심지어 커널에서도 unmapped user virtual address를 접근하려고 하면 page fault

#### Typical Memory Layout
개념적으로는 각 process는 각자의 virtual memory 맘대로 layout 가능
실제로는 user virtual memory는 다음처럼 배치됨.
```c
   PHYS_BASE +----------------------------------+
             |            user stack            |
             |                 |                |
             |                 |                |
             |                 V                |
             |          grows downward          |
             |                                  |
             |                                  |
             |                                  |
             |                                  |
             |           grows upward           |
             |                 ^                |
             |                 |                |
             |                 |                |
             +----------------------------------+
             | uninitialized data segment (BSS) |
             +----------------------------------+
             |     initialized data segment     |
             +----------------------------------+
             |           code segment           |
  0x08048000 +----------------------------------+
             |                                  |
             |                                  |
             |                                  |
             |                                  |
             |                                  |
           0 +----------------------------------+
```
이번 프로젝트에서는 user stack은 fixed size
- 플젝3에서는 grow 가능
기존에는 system call을 통해 초기화되지 않은 data segment 크기 조정 가능.
- 이를 구현할 필요는 없음.

핀토스에서 code segment
- User VA `0x08084000`에서 시작, address space 맨 아래에서 약 128MB
	- 큰 의미 x
- Linker는 매모리에서의 user program의 layout 설정
	- directed by "linker script"<-- 다양한 프로그램 세그먼트의 이름, 위치를 알려줌.
### Accessing User Memory
system call의 일부로 kernel은 종종 user program이 제공하는 pointer를 통해 memory를 접근해야 함.
- user가 null pointer나 unmapped virtual memory에 대한 pointer, kernel virtual address space에 대한 poitner를 제공할 수 있다.
- --> kernel은 저런 pointer를 reject, 해당 process를 terminate하고 resource free

**이런 작업을 수행하는 두 가지 합리적인 방법**
- user-provided pointer의 validity를 verify하고 역참조(dereference)
	- 이 방법 선택시 `userprog/pagedir.c`, `threads/vaddr.h` 함수 참고
	- 가장 간단한 방법
- pointer가 `PHYS_BASE` 밑을 가리키는지만 확인 후 dereference
	- `page_fault()`, `userprog/exceiption.c`
	- 해당 방법 빠름

두 방법에서 leak resource(리소스 누수)에 주의
- 잘못된 사용자 포인터 발생시 release lock 하거나 page of memory free해야 함.
- user pointer 역참조 전에 verify 했다면 쉬움
- 하지만 잘못된 포인터로 인해 page fault 발생시 memory access에서 오류 코드 반환할 방법 없음.--> 어려움
	- 이에 유용한 코드
```c
/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}
 
/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}
```
이러한 각 함수는 사용자 주소가 이미 PHYS_BASE 아래에 있는 것으로 확인되었다고 가정합니다. 또한 커널에서 페이지 오류가 발생하면 eax를 0xffffffff로 설정하고 이전 값을 eip에 복사하도록 page_fault()를 수정했다고 가정

## Suggested Order of Implementation
다음 작업들을 병행해 구현하는 것을 추천
- Argument passing
	- 이거 구현 전까지 모든 user program은 즉시 page fault 남.
	- 당장은 `*esp = PHYS_BASE;`를 `*esp = PHYS_BASE - 12;`로 간단히 변경 가능. in `setup_stack()`
		- argument 안쓰는 테스트에서 작동
	- 제대로 구현 전까지는 command-line arguments 넘기지 말고 실행해야함.
- User memory access
	- 모든 system call은 user memory를 읽어야함.
	- 몇몇은 쓰기도 해야함.
- System call infrastructure
	- user stack에서 system call number를 읽고 handler로 dispatch 하도록 코드 작성
- The `exit` system call
	- 모든 프로그램 정상 종료되면 `exit` 호출, `main`도 간접적으로 `exit` 호출(`_start()` in `lib/user/entry.c`)
- The `write` system call for writing to fd 1, the system console.
	- 모든 테스트 프로그램은 콘솔에 씀.(user process version of `printf`)
		- `write` 가능할 때까지 오작동
- Change `process_wait()` to an infinite loop(one that waits forever).
	- 현재는 즉시 반환. 프로세스 실행 전에 핀토스가 꺼짐.

위 완료시 user process 최소한으로 작동. 최소 콘솔에 글 쓰고 종료 가능. 
이후 테스트 통과하게 개선해라
## Requirements
### Process Termination Messages
언제든 user process가 terminate되면 process 이름과 exit code를 출력해라.
- `exit`을 호출했든, 어떤 이유든
```c
printf ("%s: exit(%d)\n", ...);
```
`process_execute()`로 pass된 full name, command-line argument는 생략
user process가 아닌 kernel thread 종료 또는 `halt` system call 호출시에는 출력 x
이 외 출력은 테스트를 실패시킬 수 있음.
### Argument Passing
현재 `process_execute()`는 새로운 process에게 argument passing을 지원하지 않음.<-- 이를 구현해라.
단순히 파일 이름 인자 받는게 아니라 공백으로 나눠 argument 받아라.
- 첫번째 word: program name, 두번째 word: the first argument. etc...
- Ex: `grep foo bar` => grep, argument foo, bar
multiple space == single space.
command line arguments에 합리적 제한 가능
- Ex. 한 페이지 길이
- 하지만 핀토스 유틸리티가 사용가능한 128바이트에 의존 x
원하는 방식으로 구문 분석해라
- `lib/string.h`에 프로토 타입 존재, `string.c`에는 주석 있는 `strtok_r()` 참고
- stack 설정 방법은 [3.5.1 Program Startup Details](https://web.stanford.edu/class/cs140/projects/pintos/pintos_3.html#SEC51) 참조

### System Calls
`userprog/syscall.c`에 system call handler 구현해라.
- skeleton 구현은 system call을 terminating process 함으로써 대응.
- system call number, system call arguments 보고 적절한 작업해야...

다음 system call들을 구현해라
- 아래는 `lib/user/syscall.h`에 포함된 프로토타입들
- system call number는 `lib/syscall-nr.h`에 정의되 있음.

문서에는 구현해야 할 엄청 많은 system call 나열되어 있음.

명시되지 않은 syscall은 나중에 플젝3, 4에서 구현해라.
system call 구현 전에는 user virtual address space에 read,write하는 것부터 만들어라.
- system call number도 user virtual address space의 user stack에 있음...
- 잘못된 포인터 제공시 user process 종료

여러 user processs가 한 번에 호출할 수 있도록 system call을 synchronize해야 함.
여러 스레드에서 한 번에 `filesys` 폴더의 file system 코드 접근은 안전 x
- file system code는 critical section으로 취급해야 함.
- `process_execute()`도 파일에 엑세스함.
- `filesys`는 수정하지 말 것.

`lib/user/syscall.c`에는 각 system call에 대한 user level function이 있다.
- 이것들은 user process에게 system call invoke하는 방법 제공

이런 작업들이 완료되면 pintos는 절대 user program에 의해 충돌, 패닉, 오작동되지 않아야 함.
- 테스트는 온갖 방법으로 이를 방해할 거임. 코너 케이스 잘 생가해라.
- user program이 os를 종료할 수 있는 유일한 방법: `halt` system call
system call이 invalid argument 패스시 가능한 옵션들
- error value return
- return undefined value
- terminate process
- [3.5.2 System Call Details](https://web.stanford.edu/class/cs140/projects/pintos/pintos_3.html#SEC52) 참고
### Denying Writes to Executables
executable로 사용 중인 파일에 대한 쓰기를 거부해라.
프로세스가 디스크에서 변경 중인 코드를 실행하려고 하면 unpredictable result가 발생할 수 있음.
- 플젝3에서도 중요하지만 지금도 중요함.
 
 open file에 write를 막기 위해 `file_deny_write()`를 사용
- `file_allow_write()`를 호출하여 다시 사용 가능
- 파일 닫으면 다시 write 됨.
- 즉 프로세스 실행 파일에 대한 쓰기를 거부하려면 프로세스 실행 중에는 파일 열어두어야 함.