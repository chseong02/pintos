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
TODO: `create`, `remove`, `open`, `filesize`, `read`, `write`, `seek`, `tell`, `close`

## Denying Writes to Executables
디스크 상에 존재하는 유저 프로그램 역시 하나의 파일이기 때문에 동시에 여러 프로세스가 접근하여 Read, Write, Execute하는 것이 가능하다. 하지만 어떤 프로세스가 유저 프로그램을 실행하는 중 다른 프로세스가 해당 프로그램 파일에 Write 하는 등의 수정이 일어날 경우 정상적인 동작이 불가능할 것이다. 때문에 어떤 프로세스가 파일을 실행하고 있을 경우 앞서 살펴본 `file_deny_write`를 호출하여 다른 프로세스가 파일을 변경하는 것을 막고, 실행을 마칠 시 `file_allow_write`를 호출하여 제한을 해제해주는 과정이 수반되어야 한다.

프로세스가 실행 가능한 ELF 파일을 읽어와 스택에 올리는 과정은 `userprog/process.c`의 `load` 함수에서 일어난다. 이때 대상 파일은 `filesys_open(file_name)`으로 명시가 되어있으므로, 파일을 성공적으로 불러온 후 해당 파일에 대해 `file_deny_write`를 호출함으로써 변경 제한을 설정하는 기능은 쉽게 구현이 가능하다.

하지만 프로세스가 종료될 때인 `process_exit`에서는 해당 프로세스가 현재 실행중이었던 파일이 무엇이었는지 알 수 없어 `file_allow_write`를 호출할 수 없다. 이를 위해 `thread` 구조체에 현재 실행중인 파일에 대한 정보를 담고 있는 멤버 변수 `file_exec`을 추가하여 `load` 중 현재 프로세스가 해당 파일을 실행 중임을 명시해주고, `process_exit`에서 어떤 파일을 실행중이었는지를 `file_exec`을 참조하여 알아낸 뒤 `file_allow_write`를 호출해줌으로써 변경 제한을 해제하는 기능을 구현할 예정이다.

TODO: `thread` 말고 다른 곳에다?