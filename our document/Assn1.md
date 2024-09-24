## Document
### Background
`threads` 디렉토리에서 작업 시작한다. `devices` 작업 곁들여서
컴파일은 `threads`디렉토리에서 완료
A.1 ~ A.5까지 훑어봐라. A.3 꼭 읽어라. B.4도 읽어야 할 거다.

핀토스는 이미 스레드 creation, completion과 스레드 간 전환을 위한 스케줄러, synchronization 기본요소(semaphores, locks, condition variable, optimization barriers)가 구현되어 있음.
- 이 코드들은 `printf`로 순서 확인 가능함. 물론 디버거 중단점도 가능.

아래는 내 생각
context: register 등 스레드에 연관된 상태값들을 포함한 환경? 맥락

스레드가 만들어지면 스케줄링될 새 context가 만들어지는 것.
해당 context에서 실행할 function을 `thread_create()`의 인수로 제공. 
스레드가 스케줄링되고 실행되면 함수의 처음부터 시작해 해당 context에서 실행. 함수 return 되면 스레드 terminate
각 스레드는 핀토스 내부에서 실행되는 미니 프로그램 같은 느낌.

어느때든 한 스레드만 실행되고 나머지는 모두 비활성화됨.
스케줄러는 다음에 어떤 스레드를 실행해야할지 정함.
(만약 ready to run인 스레드가 없다면 특별한 "idle" 스레드(`idle()`에 구현됨)가 실행됨.)

context 스위치에 대한 메카닉은 `threads/switch.S`에 80x86 어셈블리 코드로 존재.
- 실행되고 있는 스레드의 state를 저장하고 전환되었을 때 restore함.

GDB 디버거를 이용해 `schedule()`에 중단점을 설정하여 스위치 컨텍스트시 일어나는 일을 추적 가능.
한 스레드가 `switch_threads()`를 호출하면 다른 스레드가 실행 시작
새 스레드가 가장 먼저 하는일: `switch_threads()`에서 반환하기

핀토스에서 각 스레드는 4kB보다 작은 고정된 사이즈의 execution stack이 할당됨. 기본적으로 커널은 스택 오버플로를 감지하지만 완벽하지 않아 큰 데이터 구조를 non-static local variable로 선언시 문제가 발생할 수 있음.
- 이 대안으로 Page allocator, block allocator 사용 가능.


**소스코드**
`threads` 디렉토리
변경할 일 없는 크게 중요하지 않은 코드들
`loader.S, loader.h`
- kernel loader. 512 바이트 어셈블리
- BIOS가 메모리로 로드하고 디스크에서 커널 찾아 메모리에 로드한 후 `start.S`의 `start()`로 이동
`start.S`
- 메모리 보호, cpu의 32bit operation을 위한 기본적 세팅
- loader와 다르게 커널의 일부 
`kernel.lds.S`
- kernel을 링크하기 위한 linker script
- kernel의 local address를 세팅하고 `start.S`를 커널 이미지 시작 부분에 위치하도록 정렬

많이 사용될 중요한 코드들
`init.c, init.h`
- Kernel 초기화. 커널의 메인 프로그램인 `main()`포함.
- 너만의 초기화 코드 여기에 추가해라
`thread.c, thread.h`
- 기본적 스레드 기능
- 대부분 여기서 작업할 것
- 모든 프로젝트에서 건드리게 됨.
`synch.c, synch.h`
- 기본적 synchronization primitives:
	- semaphores, locks, condition variables, and optimization barriers
- 모든 프로젝트에서 건드리게 될 것

`switch.S, switch.h`
- thread 스위치 위한 어셈블리 루틴
`palloc.c, palloc.h`
- page allocator

`devices` 디렉토리
`timer.c, timer.h`
- 시스템 타이머 for tick(100 time/s)
- 이번 프로젝트에서 건드리게 될 것
`vga.c, vga.h`
- VGA 디스플레이 드라이버
- `printf()`가 해당 코드 실행할 것
- 해당 코드 볼 일 없음.
`serial.c, serial.h`
- Serial port 드라이버
- `printf()`가 해당 코드 실행할 것
- 해당 코드 볼 일 없음.
`block.c, block.h`
- `block` 디바이스를 위한 abstrcation layer
	- 고정 크기 블록의 배열로 구성된 random-access, disk-like 디바이스
	- 핀토스 2가지 유형 block 디바이스 지원함. (IDE disk, partition)
- 플젝 2까지 쓸 일 없음.
`rtc.c, rtc.h`
- Real-time clock 드라이버
- 기본적으로는 `thread/init.c`에서 난수 생성에 사용될 초기 시드 정할 때만 사용 
`pit.c, pit.h`
- 8254 프로그래밍 가능 interrupt timer 구성 코드
- `devices/timer.c`, `devices/speaker.c`에서 사용
	- PIT의 output channel 중 하나를 사용하기 때문

`lib,lib/kernel` 디렉토리
`kernel/list.c, kernel/list.h`
- 더블 링크드 리스트 구현 
- 아마 프로젝트1 몇몇 구간에서 사용할 것

**Synchronization**
대부분의 synchronization 문제는 interrupts를 끔으로써 해결 가능함.
근데 이러면 동시성(concurrency) 없어짐. 즉 race condition의 가능성이 없어짐.
근데 이렇게 **절대 다루지마**
`threads/synch.c` 주석을 잘 살펴봐라

핀토스에서 interrupt 비활성화로 잘 해결되는 유일한 문제는 kernel 스레드와 interrupt 핸들러 간 공유되는 데이터를 조정하는 것.
- interrupt 핸들러는 sleep할 수 없기 때문에 acquire lock 할 수 없다.
- 즉 해당 데이터는 interrupt 해제하여 커널 스레드 내에서 보호해야 한다.(?)

이 프로젝트는 interrupt 핸들러가 약간의 스레드 상태만 엑세스해도 됨.
Alarm clock에서 timer interrupt는 잠자는 스레드를 깨우고
고급 스케줄러에서는 timer interrupt가 몇몇 global, per-thread variable에 접근할 수 있어야 한다.
커널 스레드에서 이런 변수를 접근할 때, timer interrupt가 간섭하지 못하도록 interrupt를 비활성화해야 한다.

interrupt 끌 때는 가능한 최소한의 코드에서 사용. 
- 타이머 틱이나 중요 이벤트를 잃을 가능성 존재
- interrupt 처리 지연 증가

`synch.c`에서 interrupt 비활성화함으로써 synchronization primitive 구현

busy waiting이 있어서는 안된다.
- `thread_yield()`를 호출하는 tight loop도 안 됨.

### Requirements
**Alarm Clock**
`devices/timer.c`의 `timer_sleep()`을 다시 구현해라.
- 기존 구현은 busy waits
	- `thread_yields()` 부르며 loop 돌며 체크
- busy wait 아닌 방식으로 재구현해라

`void timer_sleep(int64_t ticks)`
- 타이머 틱 x 이상 지날때까지 호출 스레드의 실행을 일시 중지
- 꼭 정확히 x틱일 필요는 없음.
- `TIMER_FREQ`: timer ticks per second
	- macro defined in `devices/timer.h`
	- Default: 100
- `timer_msleep(),timer_usleep(),timer_nsleep()`은 별도로 수정 필요 x
	- 자동적으로 `timer_sleep` 사용할 것

**Priority Scheduling**
우선순위 높은 스레드가 ready list에 추가될 시 즉시 프로세서를 새 스레드에 양보한다. 스레드가 lock, semaphore, condition variable을 기다릴 때도 우선순위 높은 것이 제일 빨리 깨어남. 우선순위는 높아지거나 낮아질 수 있다.

`PRI_MIN(0)` ~ `PRI_MAX(63)` 사이
- 작은 수는 낮은 우선순위
- `thread_create()`시 arg로 넘김. 다른 우선순위할 이유 없다면 `PRI_DEFAULT(31)`

ISSUE: "priority inversion" (우선순위 반전)
- H,L,M 스레드 있을 때, lower priority thread가 high priority thread의 락을 가지고 있을 때, lower priority thread는 실행되지 않고 결국 high는 실행되지 않음.
- H가 L에게 priority를 "donate"함으로써 해결 필요.
	- recall시 되돌려받기
- donation 필요한 모든 경우 잘 고려해야 함.
	- 한 스레드가 여러 스레드로부터 donation 받을수도
	- M이 H 락 가지고 있고 L이 M 락 가지고 있을 때(nested donation도 고려)
- Lock에 대해서만 priority donation 고려하면 됨
	- 다른 경우 생각 필요x
`void thread_set_priority(int new_priority)`
- `new_priority`로 셋
`int thread_get_priority(void)`
- 스레드 priority 반환, donation 받았다면 higher(donated) priority 반환

**Advanced Scheduler**
multilevel feedback queue 스케줄러(4.4BSD scheduler와 비슷)를 구현해라
 - average response time을 줄이기 위해
 - B. 4.4BSD Scheduler에서 자세한 요구사항 확인
advanced scheduler는 "priority donation"을 하지 않음.
`-mlfqs` 커널 옵션 줄시 `4.4BSD scheduler`가 선택되어야 함.
`parse_options()`를 통해 옵션 파싱
옵션 줄 시 `thread_mlfqs()`를 통해 세팅
