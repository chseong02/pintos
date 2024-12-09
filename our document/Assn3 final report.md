# Pintos Assn3 Final Report
Team 37 
20200229 김경민, 20200423 성치호

## 구현
### 1. Frame Table
### 2. Lazy Loading
### 3. Supplemental Page Table
### 4. Stack Growth
### 5. File Memory Mapping

#### process.h / process.c
파일 맵핑은 각 프로세스마다 관리하므로, `process.h`에 다음과 같이 각 맵핑에 대한 정보를 나타내는 구조체와 맵핑 정보를 저장할 리스트를 추가했다.

```c
struct fmm_data
{
  mapid_t id;
  struct file *file;
  int file_size;
  int page_count;
  void *upage;
  struct list_elem fmm_data_list_elem;
};
```
```c
struct process
{
  ...
  struct list fmm_data_list;
  int mmap_count;
};
```

Design에서 설계했던 구조에서 약간의 변동사항이 있다. Design에서는 각 맵핑마다 맵핑된 페이지들을 관리하는 리스트인 `struct list page_list`를 추가하여 페이지들을 관리하고자 했는데, 한 맵핑 상에 있는 모든 페이지는 연속적으로 위치해야 하고 각 페이지를 일일이 할당해주는 데 `O(n)`의 시간이 필요하므로 리스트를 사용하는 데 이점이 없다고 판단하여 대신 파일 크기, 페이지 수 및 할당된 가상 공간 주소를 저장하도록 변경하였다.

```c
void
init_process (struct process *p)
{
  ...
  list_init (&p->fmm_data_list);
}
```
맵핑 정보를 담는 리스트인 `fmm_data_list`는 프로세스 시작 중 초기화한다.

#### syscall.h / syscall.c

`mmap`과 `munmap`은 시스템 콜의 일종이므로 Project 2에서 시스템 콜을 구현한 코드에 이어 구현하였다.

```c
static mapid_t
sys_mmap (int fd, void *addr)
{
  /* check file validity */
  int file_size = sys_filesize(fd);
  if(file_size <= 0)
  {
    /* zero or error */
    return MAP_FAILED;
  }

  /* check address align */
  if(addr == NULL || (int)addr % PGSIZE)
  {
    return MAP_FAILED;
  }

  /* check if page already exists in range */
  for(int i = 0; i < file_size; i += PGSIZE)
  {
    if(find_s_page_table_entry_from_upage(addr + i) != NULL)
    {
      return MAP_FAILED;
    }
  }
  ...
```
`fd`와 `addr` 관련 validity check을 우선 수행한다. 체크해야 할 사항은 다음과 같다.
- `fd`가 가리키는 파일이 정상적이고 파일의 크기가 0보다 클 것
- `addr`이 0이 아니고 page aligned 되어있을 것
- 파일이 맵핑되어야 할 가상 메모리 공간에 이미 할당되어있는 페이지가 없을 것

`fd`의 validity check는 `sys_filesize`를 호출할 때와 동일하게 진행하므로 중복 코드를 사용하지 않았다. 그리고 맵핑 범위 내 페이지 존재 체크는 `find_s_page_table_entry_from_upage`를 통해 범위 내 각 페이지들마다 supplemental page가 존재하는지를 체크해줬다.

```c
  ...
  file_lock_acquire();

  /* open same file again because original file can be 
     closed after mmap but mmap should stay */
  struct process *cur = thread_current()->process_ptr;
  struct file *f = cur->fd_table[fd].file;
  struct file *new_f = file_reopen(f);
  if(new_f == NULL)
  {
    file_lock_release();
    return MAP_FAILED;
  }

  /* allocate mapping data & setup */
  struct fmm_data *fmm = malloc(sizeof(struct fmm_data));
  if(fmm == NULL)
  {
    file_lock_release();
    return MAP_FAILED;
  }
  fmm->id = cur->mmap_count++;
  fmm->file = new_f;
  fmm->file_size = file_size;
  fmm->page_count = 0;
  fmm->upage = addr;
  ...
```
다음 과정으로 맵핑하고자 하는 파일을 다시 열어준 뒤, 맵핑 정보를 담을 구조체를 동적할당하여 정보를 담는다. 파일 시스템을 건드리므로 동시성 확보를 위해 Project 2에서 사용했던 filelock을 걸어줘야 한다.

```c
  ...
  /* set page table for every pages in range */
  for(int i = 0; i < file_size; i += PGSIZE)
  {
    int page_data_size = file_size - i >= PGSIZE ? PGSIZE : file_size - i;
    s_page_table_file_add(addr + i, true, new_f, i, 
    page_data_size, PGSIZE - page_data_size, FAL_USER);
    fmm->page_count++;
  }

  /* add mapping data to list */
  list_push_back(&(cur->fmm_data_list), &(fmm->fmm_data_list_elem));

  file_lock_release();
    
  return fmm->id;
}
```
실질적으로 맵핑을 수행하는 파트. 파일 크기만큼의 데이터가 모두 할당될 수 있을 만큼의 페이지를 `s_page_table_file_add` 함수를 통해 할당하고 마지막 페이지에 공간이 남을 시 `file_zero_bytes` 파라미터로 넘겨준다. 마지막으로 프로세스의 `fmm_data_list`에 새로 생성한 맵핑 정보를 넣고 filelock을 해제 후 새로 할당받은 맵핑 id를 반환한다.

```c
static void
sys_munmap (mapid_t mapping)
{
  struct process *cur = thread_current()->process_ptr;
  struct list *fmm_list = &(cur->fmm_data_list);

  struct fmm_data *found_entry = NULL;
  struct list_elem *e;

  file_lock_acquire();

  /* search for mapping with (id: mapping) in list */
  for(e = list_begin(fmm_list); e != list_end(fmm_list); e = list_next(e))
	{
		struct fmm_data *entry = list_entry(e, struct fmm_data, fmm_data_list_elem);
    if(entry->id == mapping)
    {
      found_entry = entry;
      break;
    }
  }

  /* mapping not found */
  if(found_entry == NULL)
  {
    file_lock_release();
    return;
  }
  ...
```

`sys_munmap`은 `mmap`에서 할당받았던 맵핑 id를 파라미터로 받아 해당하는 맵핑을 찾아 해제하는 함수이다. 우선 프로세스의 `fmm_data_list`에서 맵핑 id에 해당하는 맵핑이 존재하는지 검색하고, 존재할 시 `found_entry`에 저장한다.

```c
  ...
  /* free each s_page entry. modify original file if dirty_bit is on. */
  for(int i = 0; i < found_entry->file_size; i += PGSIZE)
  {
    struct s_page_table_entry *s_page 
      = find_s_page_table_entry_from_upage(found_entry->upage + i);
    if(pagedir_is_dirty(thread_current()->pagedir, found_entry->upage + i))
    {
      void *page = pagedir_get_page(thread_current()->pagedir, found_entry->upage + i);
      file_write_at(s_page->file, page, s_page->file_read_bytes, s_page->file_ofs);
    }
    s_page_table_delete_from_upage(found_entry->upage + i);
  }

  /* remove entry from mapping list */
  list_remove(e);
  free(found_entry);

  file_lock_release();
}
```

찾은 `found_entry`의 정보를 기반으로 mmap에서 할당한 페이지들을 동일한 방법으로 순회하며 할당 해제한다. 만약 `pagedir_is_dirty`에 따라 해당 페이지가 변경된 적이 있다고 판별될 시 `file_write_at`를 호출하여 해당 페이지의 변경 사항을 파일에 lazy하게 반영한다. 확인한 각 페이지는 `s_page_table_delete_from_upage`를 통해 해제한다.

마지막으로 `fmm_data_list`에서 찾은 맵핑 정보를 삭제한 후 동적할당까지 해제해준다.

```c
void
sys_exit (int status)
{
  ...
  /* naive free logic for mmap on exit */
  for(mapid_t i = 0; i < cur->process_ptr->mmap_count; i++){
    sys_munmap(i);
  }
  ...
}
```
프로세스가 종료될 때 맵핑된 파일들이 있다면 모두 할당 해제해줘야 하는데, 이는 `sys_exit`에서 수행한다. 현재까지 맵핑된 파일 수를 `mmap_count`에다 기록한 후 해제할 때 모든 맵핑 id를 확인하여 `munmap`해준다. 만약 이미 해제한 맵핑이라 하더라도 `sys_munmap`에서 아무 일도 일어나지 않도록 처리한다.


### 6. Swap Table
### 7. On Process Termination

## 발전한 점

## 한계

## 배운 점