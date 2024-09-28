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
