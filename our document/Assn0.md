## Docu 분석
#### 주요 필요 폴더(Assn1 기준)
`threads`: base kernel 코드
`devices`: 키보드, 타이머 등 I/O 인터페이스, 타이머 구현 수정 필요

사용할 수도 있는 것
`lib`,`lib/kernel`: Standard C 라이브러리의 서브셋, 유저 프로그램, 커널 소스 코드 포함
`utils`

#### 핀토스 실행 방법
`build` 폴더에서 커맨드 입력
``` shell
pintos run alarm-multiple
``` 
핀토스 커널에 `run alarm-multiple` 커맨드 전달.
커널은 `alarm-multiple` 실행
이런 커맨드는 Bochs를 실행하고 `bochsrc.txt` 파일 생성한다.
Bochs는 OS 에뮬레이터로 메세지 등을 디스플레이로 보여준다.
만약 디스플레이가 보이지 않았다면 다음처럼 명시적인 명령도 가능.
```
pintos -v --run alarm-multiple
```
명령을 핀토스를 통해 실행하였다면 터미널에 동일한 텍스트가 표시된다. 핀토스가 모든 출력을 VGA 디스플레이와 첫번째 serial port에 보내고 직렬 포트는 Bochs의 stdin, stdout에 연결되어 있기 때문이다.
다음처럼 별도의 로그파일에 저장도 가능.
```
pintos run alarm-multiple > logfile
```

`--`를 통해 핀토스에서 simulator를 조절할 수 있음.
```
pintos option -- argument
```
`--qemu`: 기본적으로 Bochs 실행이지만 QEMU 선택 가능
GDB(디버거) 실행 설정 등 다양한 옵션 존재
`pintos -h`로 다양한 명령어 확인 가능.

#### 디버깅
Bochs는 시드가 같은 한 재현 가능성(reproductibility) 있다.
jitter 기능을 통해 랜덤한 시간 간격으로 timer interrupts를 발생시킬 수 있다. `-j seed`를 통해 사용가능.
같은 시드 내라면 재현 가능성이 보존된다.
다양한 시드에서 테스트하는 것을 권장한다.
QEMU는 많은 경우에서 Bochs보다 훨씬 빠르지만 재현 가능성에 관련된 기능은 없다.

#### 테스트
각 프로젝트의 `build` 디렉토리에서 
```
make check
```
프로젝트1에서는 Bochs가 빠르고 나머지에서는 QEMU가 빠름.

**개별 테스트 실행 방법**
`t.output`에 출력값을, `t.result`에 결과를 기록.
개별 테스트에 해당되는 `.result`파일을 make하면 된다.
```
make tests/threads/alarm-multiple.result
```
이후에는 `make clean`하면 된다.

pintos에 옵션 주는 방법
```
make check PINTOSOPTS='-j 1'
```
지터 값 1 주는 테스트

```
make check VERBOSE=1
```
실행 중 피드백 제공

#### Design Document
각 프로젝트마다 디자인 도큐먼트 템플릿 제공한다.

**Data Structures**
바뀌거나 추가된 `struct`나 global, static variable, `typedef` 등의 declaration을 복붙해라. 25 단어 내로 간단히 구조의 목적도 서술

**Algorithms**
코드가 어떻게 작동하는가?
요구사항 문서의 높은 수준 설명보다 낮은 수준으로 설명해야 함.
근데 코드보다는 높은 수준으로 설명해라.
한 줄 한 줄 설명하지마

**Synchronization**
멀티 스레드 동기화 어려운데 동기화 어떻게 했는가?

**Rationale**(근거)
왜 그런 디자인 했는가?
왜 바꾼 것이 이전 것보다 좋은가?
공간, 시간 복잡도를 언급할 수도 있다.

#### Source Code
```
diff -uprb pintos.origpintos.submitted
```
같은 명령어로 원래 것과 비교해서 채점할 것이다.
해당 과제에서 중점적으로 중요한 것을 잘 구현하는 것이 중요하고 나머지는 훨씬 덜 중요하다.
기존 핀토스 코드 스타일에 잘 녹아들게 작성해라.
코드 스타일을 통일해라.
모든 구조체, 구조체 멤버, global,static 변수, typedef, enum, 함수 등에 간단한 주석 추가 필요.
assertion을 사용해 불변성을 문서화해라.

