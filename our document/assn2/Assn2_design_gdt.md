### Global Descriptor Table (GDT)
80x86은 segmented architecture로 GDT가 존재한다. Global Descriptor Table (GDT)는 권한에 따라 모든 프로세스에서 사용할 가능성이 있는 segment들을 정의하는 테이블이다. GDT의 각 entry는 세그먼트를 나타낸다. 중요히 생각해야할 segment는 code, data, TSS descriptor 세가지 유형의 세그먼트뿐이다. 

아래는 이런 GDT를 셋업하기 위한 변수 및 함수로 이번 프로젝트에서는 변경할 필요는 없으며 호출해 사용할 가능성은 있다.
```c
#define SEL_NULL        0x00    /* Null selector. */
#define SEL_KCSEG       0x08    /* Kernel code selector. */
#define SEL_KDSEG       0x10    /* Kernel data selector. */
```
from `threads/loader.h`
```c
#define SEL_UCSEG       0x1B    /* User code selector. */
#define SEL_UDSEG       0x23    /* User data selector. */
#define SEL_TSS         0x28    /* Task-state segment. */
#define SEL_CNT         6       /* Number of segments. */
```
GDT는 위의 6가지 유형의 segment만 관리한다.  

```c
static uint64_t gdt[SEL_CNT];
```
GDT를 나타내는 전역 변수 `gdt`로 6가지 segment 정보를 담을 것을 나타내고 있다.

#### `gdt_init(void)`
```c
void
gdt_init (void)
{
  uint64_t gdtr_operand;

  /* Initialize GDT. */
  gdt[SEL_NULL / sizeof *gdt] = 0;
  gdt[SEL_KCSEG / sizeof *gdt] = make_code_desc (0);
  gdt[SEL_KDSEG / sizeof *gdt] = make_data_desc (0);
  gdt[SEL_UCSEG / sizeof *gdt] = make_code_desc (3);
  gdt[SEL_UDSEG / sizeof *gdt] = make_data_desc (3);
  gdt[SEL_TSS / sizeof *gdt] = make_tss_desc (tss_get ());

  /* Load GDTR, TR.  See [IA32-v3a] 2.4.1 "Global Descriptor
     Table Register (GDTR)", 2.4.4 "Task Register (TR)", and
     6.2.4 "Task Register".  */
  gdtr_operand = make_gdtr_operand (sizeof gdt - 1, gdt);
  asm volatile ("lgdt %0" : : "m" (gdtr_operand));
  asm volatile ("ltr %w0" : : "q" (SEL_TSS));
}
```
> userprog 관련 segment를 포함해 `gdt`를 초기화하는 함수
`make_*_desc` 함수들을 통해 생성한 segment descriptor를 `gdt` 내 적절한 위치에 segment descriptor를 넣는다.
이후 `make_gdtr_operand`를 통해 `lgdt`에 넘겨줄 operand를 만든다. 해당 operand는 `gdt`의 주소와 크기를 담고 있다. 위에서 생성한 operand를 이용해 `lgdt`를 호출해 `gdtr`에 새로 만든 `gdt` 정보를 저장하게 한다. 이로써 userprog 관련 segment descriptor들이 GDT에 추가된 것이다.

#### `enum seg_class`
```c
enum seg_class
  {
    CLS_SYSTEM = 0,             /* System segment. */
    CLS_CODE_DATA = 1           /* Code or data segment. */
  };
```
system에 관한 segment인지, code나 data에 관련된 segment인지 구별하는 enum

#### `enum seg_granularity`
```c
enum seg_granularity
  {
    GRAN_BYTE = 0,              /* Limit has 1-byte granularity. */
    GRAN_PAGE = 1               /* Limit has 4 kB granularity. */
  };
```
seg가 어떤 크기로 세분되는지 나타내는 enum으로 `GRAN_BYTE`(1byte), `GRAN_PAGE`(4kB)로 구분할 수 있다.

#### `make_seg_desc`
```c
static uint64_t
make_seg_desc (uint32_t base,
               uint32_t limit,
               enum seg_class class,
               int type,
               int dpl,
               enum seg_granularity granularity)
{
  uint32_t e0, e1;

  ASSERT (limit <= 0xfffff);
  ASSERT (class == CLS_SYSTEM || class == CLS_CODE_DATA);
  ASSERT (type >= 0 && type <= 15);
  ASSERT (dpl >= 0 && dpl <= 3);
  ASSERT (granularity == GRAN_BYTE || granularity == GRAN_PAGE);

  e0 = ((limit & 0xffff)             /* Limit 15:0. */
        | (base << 16));             /* Base 15:0. */

  e1 = (((base >> 16) & 0xff)        /* Base 23:16. */
        | (type << 8)                /* Segment type. */
        | (class << 12)              /* 0=system, 1=code/data. */
        | (dpl << 13)                /* Descriptor privilege. */
        | (1 << 15)                  /* Present. */
        | (limit & 0xf0000)          /* Limit 16:19. */
        | (1 << 22)                  /* 32-bit segment. */
        | (granularity << 23)        /* Byte/page granularity. */
        | (base & 0xff000000));      /* Base 31:24. */

  return e0 | ((uint64_t) e1 << 32);
}
```
> 주어진 정보(base, 세그먼트 class, granularity, type, dpl)를 바탕으로 64비트 segment descriptor를 생성해 반환하는 함수
`make_code_desc`,`make_data_desc`,`make_tss_desc`에서 호출하여 각 세그먼트 descriptor를 생성할 때 사용한다.
입력한 `base`, `class`,`type`,`dpl`,`granularity`를 형식에 맞춰 64비트 내 적절한 위치에 배치하고 `limit`을 통해 `base` 등 값 노출 여부를 조절하여 segment descriptor를 제작해 반환한다.

#### `make_code_desc(int dpl)`
```c
static uint64_t
make_code_desc (int dpl)
{
  return make_seg_desc (0, 0xfffff, CLS_CODE_DATA, 10, dpl, GRAN_PAGE);
}
```
> 주어진 `dpl`을 가지는 code segment descriptor를 생성해 반환하는 함수

#### `make_data_desc(int dpl)`
```c
static uint64_t
make_data_desc (int dpl)
{
  return make_seg_desc (0, 0xfffff, CLS_CODE_DATA, 2, dpl, GRAN_PAGE);
}
```
> 주어진 `dpl`을 가지는 data segment descriptor를 생성해 반환하는 함수

#### `make_tss_desc(void *laddr)`
```c
static uint64_t
make_tss_desc (void *laddr)
{
  return make_seg_desc ((uint32_t) laddr, 0x67, CLS_SYSTEM, 9, 0, GRAN_BYTE);
}
```
> 주어진 `dpl`을 가지는 tss segment descriptor를 생성해 반환하는 함수

#### `make_gdtr_operand(uint16_t limit, void *base)`
```c
static uint64_t
make_gdtr_operand (uint16_t limit, void *base)
{
  return limit | ((uint64_t) (uint32_t) base << 16);
}
```
> `lgdt`를 수행할 때 넘길 operand를 생성하는 함수로 gdt의 주소 및 크기를 담고 있다.
