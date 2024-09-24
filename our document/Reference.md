
https://web.stanford.edu/class/cs140/projects/pintos/pintos_6.html#SEC91
## 1. Loading
핀토스에서 가장 처음으로 실행되는것은 Loader `threads/loader.S`
PC BIOS가 loader를 메모리에 로드. 
로더가 디스크에서 커널 찾은 뒤 메모리에 로딩하고 해당 메모리 주소로 이동.
PC BIOS가 첫 하드 디스크의 첫번째 섹터인 *master boot record*(MBR)에서 loader를 로드
64바이트를 PC convention 예약에 사용, 핀토스는 128 추가 바이트를 커널 커맨드라인드 arg에 사용. 
loader를 위해 300 바이트 가량 남음. --> 어셈블리로 작성될  것이 강제됨.
loader는 커널을 찾기 위해 각 하드 디스크 파티션 테이블을 읽고 핀토스 커널에 사용되는 부팅 가능한 파티션을 찾아다님. 찾으면 128kB 메모리를 읽어 들임.(512kB이하로만 읽음, 표존 PC BIOS는 1MB 이상의 커널 로드 불가)
Loader는 커널의 ELF 헤더에 포함된 entry point를 가리키는 pointer를 추출. 포인터가 가리키는 위치로 이동.
Pintos kernel 커맨드는 boot loader에 저장됨. `pintos`는 커널이 실행될 때마다 boot loader 복사본을 수정하여 arg를 삽입. 부팅시 커널이 부트로더의 해당 arg를 읽음.

loader 마지막으로 커널 엔트리 포인트인 `threads/star.S`의 `start()`에 컨트롤 넘김.
해당 코드는 CPU를 16bit realmode -> 32bit protected mode로 스위치


startup 코드는 
첫번째, BIOS에게 PC의 memory size 물어봄.
- 해당 기능 수행 BIOS function는 64MB의 RAM까지만 감지 가능.(핀토스의 실질적 한계)
- `init_ram_pages` global variable로 저장
CPU 초기화
첫번째로, A20 라인 활성화. 
CPU 주소 라인 20d은 역사적인 이유로 0으로 고정되어 부팅됨.
- 그래서 처음 1MB 이상 메모리 접근 실패
- 핀토스는 이 이상을 원하므로 enable해야함.
다음으로 loader는 basic page table 생성
- 가상 메모리 기반(가상주소 0으로 시작)의 64MB를 동일한 물리 주소에 직접 매핑
- 기본값이 0xc0000000 (3GB)인 `LOADER_PHYS_BASE` 가상 주소에도 동일한 물리 주소 매핑
페이징 테이블 초기화 후, CPU control registers를 로드, protected mode, paging 켜기, segment register 세팅
아직 보호모드에서 인터럽트 처리 준비 완료 안되었으므로 인터럽트는 비활성화

커널은 `main()`함수로 시작됨. (pintos의 다른 대부분의 코드처럼 c로 작성됨)
`main` 시작시에도 아직 뭐 준비된 게 아닌 날 것의 상태.
- `main`은 주로 pintos의 다른 module 초기화 함수 호출로 이루어짐. 
	- 주로 이름은 `module_init()`, `module`은 실제 모듈 이름으로 치환해 생각해라.

`main` 첫 스텝
`bss_init()`호출
- kernel의`BSS`를 지움.
- `BSS`: 모두 0으로 초기화되어야하는 세그먼트의 전통적인 이름
- 대부분의 c 구현에서 함수 외부 변수 선언 -> 해당 변수 BSS로 들어감.
- 로더는 메모리로 가져오는 이미지에 BSS는 저장되어 있지 않음.(그냥 `memset`하면 되잖아)
`main` 두번째 스텝
`read_command_line`
- kernel command를 arguments로 쪼갬
- `parse_options`로 command 시작 옵션 읽음.
다음
`threads_init()`
- thread 시스템 초기화
- 초기화 초반에 수행하는 이유: valid thread structure가 acquiring a lock의 전제 조건이기에
	- 그리고 acquiring a lock은 핀토스의 다른 subsystems에 중요함.
- console 초기화 후 startup message 콘솔에 출력
다음
kernel의 메모리 시스템 초기화
`palloc_init()`
- kernel page allocator를 set up
	- kernel page allocator: 하나 이상의 페이지를 한번에 할당함.
`malloc_init()`
- 임의의 크기의 메모리 블럭을 할당하는 allocator setup
`paging_init()`
- kernel의 page table set up

Assn2 및 이후에서 `main`은 `tss_init(),gdt_init()`도 호출

다음
interrupt system 초기화
`intr_init()`
- CPU의 *interrupt descriptor table*(IDT) set up
	- interrupt handling을 위한 준비
`timer_init()`,`kbd_init()`
- timer interrupts, keyboard interrupts 준비
`input_init()`
- serial, keyboard input을 하나의 스트림으로 합치도록 설정
Assn2 이상에서는 유저 프로그램에 인한 interrupt 처리 준비도 함.

다음
스케줄러 시작
`thread_start()`
- idle thread 생성, interrupts 활성화
interrupt 활성화되면서 interrupt-driven serial port I/O도 가능해짐.
`serial_init_queue()`
- 해당 모드로 전환
`timer_calibrate()`
- 정확한 짧은 지연을 위한 timer 보정

파일 시스템 컴파일된 경우,(Assn2에서 할 거임)
`ide_init()`
- IDE disk 초기화
`filesys_init()`
- 파일 시스템 초기화

부트 완료! 메세지 출력


`run_actions`
- 파싱하고 kernel command line에 쓰인 action 수행
- ex. `run`, user program(assn2)

`-q` 옵션 command에 있을시 `shutdown_power_off()` 호출
- 머신 시뮬레이터 종료
만약 없다면
- `main()`이 `thread_exit()` 호출하여 실행 중인 다른 스레드가 계속 실행될 수 있도록 함.


Physical Memory Map
@headitem Memory Range

|                    |          |                                                                               |
| ------------------ | -------- | ----------------------------------------------------------------------------- |
| Owner              | Contents |                                                                               |
| 00000000--000003ff | CPU      | Real mode interrupt table.                                                    |
| 00000400--000005ff | BIOS     | Miscellaneous data area.                                                      |
| 00000600--00007bff | --       | ---                                                                           |
| 00007c00--00007dff | Pintos   | Loader.                                                                       |
| 0000e000--0000efff | Pintos   | Stack for loader; kernel stack and `struct thread` for initial kernel thread. |
| 0000f000--0000ffff | Pintos   | Page directory for startup code.                                              |
| 00010000--00020000 | Pintos   | Page tables for startup code.                                                 |
| 00020000--0009ffff | Pintos   | Kernel code, data, and uninitialized data segments.                           |
| 000a0000--000bffff | Video    | VGA display memory.                                                           |
| 000c0000--000effff | Hardware | Reserved for expansion card RAM and ROM.                                      |
| 000f0000--000fffff | BIOS     | ROM BIOS.                                                                     |
| 00100000--03ffffff | Pintos   | Dynamic memory allocation                                                     |

