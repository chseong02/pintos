### Page Directory
Page table이란 CPU가 virtual address를 physical address로 변환할 때 사용하는 데이터 구조로 page -> frame으로 변환한다. 
아래 그림에서 알 수 있듯이 virtual address는 page number와 offset으로 이루어진다. page table을 이용하여 page number를 frame number로 변환 뒤 offset과 결합한다.
```c
                         +----------+
        .--------------->|Page Table|-----------.
       /                 +----------+            |
   0   |  12 11 0                            0   V  12 11 0
  +---------+----+                          +---------+----+
  |Page Nr  | Ofs|                          |Frame Nr | Ofs|
  +---------+----+                          +---------+----+
   Virt Addr   |                             Phys Addr   ^
                \_______________________________________/
```

아래는 80x86 hardware page table에 대한 간단한 관리자 page directory에 관한 함수, 변수들이다. 이번 프로젝트에서 변경할 필요는 없을 것으로 예상되며 아래 함수들을 호출해 사용할 예정이다.

#### `pagedir_create(void)`
```c
uint32_t *
pagedir_create (void) 
{
  uint32_t *pd = palloc_get_page (0);
  if (pd != NULL)
    memcpy (pd, init_page_dir, PGSIZE);
  return pd;
}
```
> 새로운 page directory를 생성하고 반환하는 함수

`palloc_get_page`를 통해 page directory에 페이지를 할당하고 `init_page_dir`을 복사하여 kernel virtual address에 대한 mapping을 추가해준다. 그리고 해당 page directory를 반환한다.

#### `pagedir_destroy(uint32_t *pd)`
```c
void
pagedir_destroy (uint32_t *pd) 
{
  uint32_t *pde;

  if (pd == NULL)
    return;

  ASSERT (pd != init_page_dir);
  for (pde = pd; pde < pd + pd_no (PHYS_BASE); pde++)
    if (*pde & PTE_P) 
      {
        uint32_t *pt = pde_get_pt (*pde);
        uint32_t *pte;
        
        for (pte = pt; pte < pt + PGSIZE / sizeof *pte; pte++)
          if (*pte & PTE_P) 
            palloc_free_page (pte_get_page (*pte));
        palloc_free_page (pt);
      }
  palloc_free_page (pd);
}
```
> 입력 받은 `pd` 페이지 디렉토리를 삭제하고 해당 페이지가 참조하는 모든 페이지를 해제(free)하는 함수
입력 받은 `pd`의 페이지 디렉토리 엔트리를 순회하며 `pde_get_pt`를 통해 페이지 디렉토리 엔트리에 해당하는 페이지 테이블 `pt`를 찾고 `pt`의 엔트리를 돌며 이에 해당되는 페이지를 `palloc_free_page`를 통해 모두 해제한다. 이후 페이지 테이블 `pt`, 페이지 디렉토리 `pd` 또한 해제한다.

#### `loopup_page(uint32_t *pd, const void *vaddr, bool create)`
```c
static uint32_t *
lookup_page (uint32_t *pd, const void *vaddr, bool create)
{
  uint32_t *pt, *pde;

  ASSERT (pd != NULL);

  /* Shouldn't create new kernel virtual mappings. */
  ASSERT (!create || is_user_vaddr (vaddr));

  /* Check for a page table for VADDR.
     If one is missing, create one if requested. */
  pde = pd + pd_no (vaddr);
  if (*pde == 0) 
    {
      if (create)
        {
          pt = palloc_get_page (PAL_ZERO);
          if (pt == NULL) 
            return NULL; 
      
          *pde = pde_create (pt);
        }
      else
        return NULL;
    }

  /* Return the page table entry. */
  pt = pde_get_pt (*pde);
  return &pt[pt_no (vaddr)];
}
```
> 입력한 `pd` 페이지 디렉토리에서 `vaddr` virtual address에 대한 페이지 엔트리 주소를 반환하는 함수. `create` 인수에 따라 `vaddr`에 대한 페이지 테이블이 없으면 생성하고 그 주소를 반환하거나 `null`포인터를 반환한다.

`pd + pd_no (vaddr)`을 통해 `vaddr`의 주소가 포함되는 page number을 얻고 이를 페이지 디렉토리 주소 `pd`로부터 더해 page directory entry 얻은 뒤 존재하는지 조회한다.
만약 없다면 `crate`가 참이면 `pde_crate`를 통해 생성하고 거짓이면 `null`을 반환한다.
이후 `pde_get_pt`를 통해 디렉토리 엔트리 `pde`에 대한 page table `pt`를 얻고 `&pt[pt_no(vaddr)]`을 통해 `vaddr`에 해당하는 페이지 엔트리 주소를 반환한다.

#### `pagedir_set_page`
```c
bool
pagedir_set_page (uint32_t *pd, void *upage, void *kpage, bool writable)
{
  uint32_t *pte;

  ASSERT (pg_ofs (upage) == 0);
  ASSERT (pg_ofs (kpage) == 0);
  ASSERT (is_user_vaddr (upage));
  ASSERT (vtop (kpage) >> PTSHIFT < init_ram_pages);
  ASSERT (pd != init_page_dir);

  pte = lookup_page (pd, upage, true);

  if (pte != NULL) 
    {
      ASSERT ((*pte & PTE_P) == 0);
      *pte = pte_create_user (kpage, writable);
      return true;
    }
  else
    return false;
}
```
> 페이지 디렉토리 `pd`에 user virtual page `upage`-> kernel virtual address인 physical 프레임`kpage`로의 매핑을 추가하는 함수

`loopup_page`를 통해 `pd` 페이지 디렉토리에 `upage` 에 해당되는 페이지 테이블 엔트리 주소를 찾는다. 없으면 생성한다. 찾지 못하거나 생성하지 못하였다면 false를 반환한다. 이후 찾은 페이지 테이블 엔트리 `pte`에 `pte_create_user`를 사용해 `kpage`를 향하는 page entry 넣는다. 이 때 writable 여부가 포함된다.

#### `pagedir_get_page(uint32_t *pd, const void *uaddr)`
```c
void *
pagedir_get_page (uint32_t *pd, const void *uaddr) 
{
  uint32_t *pte;

  ASSERT (is_user_vaddr (uaddr));
  
  pte = lookup_page (pd, uaddr, false);
  if (pte != NULL && (*pte & PTE_P) != 0)
    return pte_get_page (*pte) + pg_ofs (uaddr);
  else
    return NULL;
}
```
> `pd` page directory에서 `uaddr` user virtual address에 대응되는 physical address를 찾고 이에 대응되는 kernel virtual address를 반환하는 함수. 
`loopup_page`를 통해 `pd` 페이지 디렉토리에서 `uaddr`에 해당되는 페이지 테이블 엔트리 주소를 얻는다. `pte_get_page`를 통해 해당 page table entry가 가리키는 페이지 포인터를 얻고 이에 `pg_ofs(uaddr)` 오프셋을 더해 반환한다. 만약 `loopup_page`에서 `uaddr`에 대응되는 페이지 테이블 엔트리를 찾지 못하였다면 `uaddr`에 대응되는 주소가 없는 것이므로 null pointer를 반환한다.

#### `pagedir_clear_page(uint32_t *pd, void *upage)`
```c
void
pagedir_clear_page (uint32_t *pd, void *upage) 
{
  uint32_t *pte;

  ASSERT (pg_ofs (upage) == 0);
  ASSERT (is_user_vaddr (upage));

  pte = lookup_page (pd, upage, false);
  if (pte != NULL && (*pte & PTE_P) != 0)
    {
      *pte &= ~PTE_P;
      invalidate_pagedir (pd);
    }
}
```
> page directory `pd`에서의 user virtual page `upage`를 존재하지 않게 보이게 만드는 함수

`loopup_page`를 통해 `pd`에서 `upage`에 해당되는 페이지 테이블 엔트리를 찾고 이 엔트리를 초기화한다. 이후 `invalidate_pagedir`을 통해 `pd`와 관련된 TLB를 invalidate한다.

#### `pagedir_is_dirty(uint32_t *pd, const void *vpage)`
```c
bool
pagedir_is_dirty (uint32_t *pd, const void *vpage) 
{
  uint32_t *pte = lookup_page (pd, vpage, false);
  return pte != NULL && (*pte & PTE_D) != 0;
}
```
> `pd`에서 `vpage` virtual page에 대한 PTE가 dirty한 여부를 반환하는 함수

만약 `pd`에서 `vpage`에 대한 page table entry를 찾을 수 없거나 `*pte & PTE_D` 결과 dirty하지 않으면 false를 반환, 두 경우가 아니라면 true를 반환한다.

#### `pagedir_set_dirty(uint32_t *pd, const void *vpage, bool dirty)`
```c
void
pagedir_set_dirty (uint32_t *pd, const void *vpage, bool dirty) 
{
  uint32_t *pte = lookup_page (pd, vpage, false);
  if (pte != NULL) 
    {
      if (dirty)
        *pte |= PTE_D;
      else 
        {
          *pte &= ~(uint32_t) PTE_D;
          invalidate_pagedir (pd);
        }
    }
}
```
> `pd` page directory의 `vpage` virtual page에 해당하는 page table entry의 ditry 값을 변경하는 함수



#### `pagedir_is_accessed(uint32_t *pd, const void *vpage)`

#### `pagedir_set_accessed(uint32_t *pd, const void *vpage, bool accessed)`

#### `pagedir_activate(uint32_t *pd)`

#### `active_pd(void)`

#### `invalidate_pagedir(uint32_t *pd)`