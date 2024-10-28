### TSS
TSS(Task-State Segment)란 80x86 architectural task switching에서 사용되는 segment이다. 자세히는 TSS는 프로세서에 내장된 멀티태스킹 지원인 'Task'를 정의하는 구조체로 OS 내 거의 활용처가 없다. 핀토스에서는 User mode에서 interrupt가 발생하여 stack을 전환할 때 TSS를 사용한다. 

user mode에서 interrupt 발생시, 프로세서는 현재 TSS의 ss0과 esp0을 참조하여 어떤 stack에서 interrupt를 핸들링할지 결정한다. 이를 위해 TSS를 생성하고 ss0, esp0을 초기화해야한다. 

아래는 이번 프로젝트에서 변경하지 않아도 되지만 가지고 와서 활용할 TSS에 관련된 함수들이다.
//TODO: 더 자세히 쓰던지
#### `struct tss`
```c
struct tss
  {
    uint16_t back_link, :16;
    void *esp0;                         /* Ring 0 stack virtual address. */
    uint16_t ss0, :16;                  /* Ring 0 stack segment selector. */
    void *esp1;
    uint16_t ss1, :16;
    void *esp2;
    uint16_t ss2, :16;
    uint32_t cr3;
    void (*eip) (void);
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint16_t es, :16;
    uint16_t cs, :16;
    uint16_t ss, :16;
    uint16_t ds, :16;
    uint16_t fs, :16;
    uint16_t gs, :16;
    uint16_t ldt, :16;
    uint16_t trace, bitmap;
  };
```

TSS(Task-State Segment)를 표현하는 struct이다. 주로 `ss0`, `esp0`만 사용된다.
- `ss0`은 Ring 0 stack의 virtual address이다. 즉 kernel stack의 virtual address이다.
- `esp0`은 Ring 0 stack의 segment selector이다. 즉 kernel stack의 segment selector이다.

#### `static tss`
```c
static struct tss *tss;
```
`uerprog/tss.c`의 `tss`는 kernel tss로 static variable이다.

#### `tss_init (void)`
```c
void
tss_init (void) 
{
  tss = palloc_get_page (PAL_ASSERT | PAL_ZERO);
  tss->ss0 = SEL_KDSEG;
  tss->bitmap = 0xdfff;
  tss_update ();
}
```
> 전역의 kernel tss인 `tss`를 초기화하는 함수

tss를 본래 의도와 다르게 위에서 설명한대로 매우 한정적인 곳에서만 사용할 것이기에 `tss`의 일부 필드만 초기화해준다. 
`palloc_get_page`를 통해 `tss`에 0으로 채워진 페이지를 할당한다. 그리고 `tss`의 `ss0`을 `SEL_KDSEG`로 초기화하고 `bitmap`도 `0xdfff`로 초기화해준다.
이후 `tss_update`를 통해 현재 스레드의 stack 끝으로 `esp0`를 초기화한다.

#### `tss_get(void)`
```c
struct tss *
tss_get (void) 
{
  ASSERT (tss != NULL);
  return tss;
}
```
> kernel tss를 반환하는 함수

`tss`가 초기화가 이루어졌는지 확인하고 `tss`를 반환한다.
#### `tss_update(void)`
```c
void
tss_update (void) 
{
  ASSERT (tss != NULL);
  tss->esp0 = (uint8_t *) thread_current () + PGSIZE;
}
```
kernel tss `tss`의 stack pointer `esp0`를 현재 스레드 stack의 끝을 가리키게 변경하는 함수이다.


