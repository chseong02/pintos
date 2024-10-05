#### `ptov(uintptr_t paddr)`
```c
static inline void *
ptov (uintptr_t paddr)
{
  ASSERT ((void *) paddr < PHYS_BASE);

  return (void *) (paddr + PHYS_BASE);
}
```

> 매개변수로 받은 physical 주소 `paddr`과 매핑되는 kernel의 가상 주소를 반환하는 함수이다.


### 리스트 in `kernel/lish.h,list.c`
Pintos에서는 `list_elem`, `list` 구조체 및 이를 이용한 함수를 통해 Double Linked List를 구현하였다. 해당 함수 및 구조체를 통해 구현한 Double Linked List는 리스트를 구성하는 element들이 연속된 메모리 공간에 위치하거나 dynamic allocated memory 사용을 요구하지 않는다. 하지만 만일 `A` 구조체로 이루어진 리스트를 구현하고자 하면 `A` 구조체가 멤버 변수로 `list_elem` 구조체의 변수를 포함하고 그 값을 관리해주어야만 한다. 그 이유는 뒤에서 리스트 구현을 이루는 구조체와 함수를 설명하며 차근차근 설명하고자 한다.

#### `list_elem` 
```c
struct list_elem 
  {
    struct list_elem *prev;     /* Previous list element. */
    struct list_elem *next;     /* Next list element. */
  };
```
리스트를 이루는 각 element의 **위치 정보**를 나타내는 구조체로 리스트 내에서 해당 위치의 앞, 뒤에 위치한 element의 위치정보를 담고 있는 `list_elem` 구조체의 주소를 각각 `prev`, `next` 에 담는다.

주의할 점은 `list_elem` 구조체 자체는 실제 구현하고자 하는 리스트의 element 자체를 가지거나 가리키는 것이 아닌 해당 element의 리스트 내 **위치정보**만을 담고 있는 것이다.
Ex. `Foo` 구조체로 이루어진 리스트를 구현하였다고 할 때, `Foo` 구조체 `foo1`의 `list_elem`은 어떤 리스트 내 `foo1`의 위치 정보만을 담고 있다.
#### `list`
```c
struct list 
  {
    struct list_elem head;      /* List head. */
    struct list_elem tail;      /* List tail. */
  };
```

구현하고자 하는 리스트의 


#### `list_entry(LIST_ELEM, STRUCT, MEMBER)`
```c
#define list_entry(LIST_ELEM, STRUCT, MEMBER)           \
        ((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
                     - offsetof (STRUCT, MEMBER.next)))
```
> `LIST_ELEM`이 가리키는 `list_elem`가 포함된 `STRUCT` 구조체 변수의 주소를 반환하는 매크로. 즉 (`LIST_ELEM`이 가리키는 `list_elem`)에 대응되는 `STRUCT` 구조체 변수 주소를 반환한다.

`STRUCT`: 어떤 리스트를 이루는 구조체 (구조체 구조 그 자체)
`LIST_ELEM`: 어떤 `STRUCT` 구조체 `a` 내에 저장된 `list_elem` 구조체 변수의 주소, 즉 리스트 내 `a`의 위치정보를 저장한 `list_elem`의 주소
`MEMBER`: `STRUCT` 내에 `list_element` 구조체를 저장하는 변수명

