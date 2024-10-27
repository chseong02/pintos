## File System

### Inode
Inode (Index node)란 UNIX 계통의 파일 시스템에서 주로 사용하는 자료구조이며, 운영체제 상의 각 파일은 하나의 Inode와 대응된다. 각 Inode는 데이터의 시작 부분이 담겨있는 Block의 인덱스, 데이터의 길이, 현재 파일의 상태 등의 메타데이터를 담고 있다. Pintos 역시 Inode 자료구조를 사용하며 `filesys/inode.c`에 해당 구현체가 정의되어 있지만 실제 UNIX계열 운영체제에서 사용하는 방식보다는 간략화되어있다.

디스크에 존재하는 모든 데이터는 일정한 크기의 Block 단위로 저장되어 구분되는데, 해당 크기인 `BLOCK_SECTOR_SIZE`는 `devices/block.h`에 512B로 전처리 되어있다.

```c
/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };
```
Inode의 구조체. 현재 열려있는 `inode`들의 리스트인 `open_inodes`와 상호작용하기 위한 `elem`, 디스크에 저장되어 있는 inode block을 가리키기 위한 인덱스 `sector`와 해당 block 데이터인 `data`, 그리고 현재 inode의 오픈 상태를 나타내는 메타데이터인 `open_cnt`, `removed`, `deny_write_cnt`로 이루어져 있다.


```c
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[125];               /* Not used. */
  };
```

실제 디스크에 저장되는 Inode block을 나타내는 구조체. 파일의 데이터를 직접 담고 있지 않고 해당 데이터가 담겨있는 첫 번째 블록의 인덱스를 `start`에 저장한다. Inode block을 나타내는 데 필요한 메타데이터가 현재로선 많지 않음에도 불구하고 `BLOCK_SECTOR_SIZE`를 채우기 위해 `unused`를 선언하여 더미 데이터로 512B를 채운 모습을 볼 수 있다.

해당 구현체를 보면서 `inode`와 `inode_disk`가 굳이 분리되어 선언되어있는 이유에 대해 의문점이 생겼는데, `inode_disk`의 데이터는 파일 자체의 정적인 정보를 나타내는 데 반해 `inode`의 데이터는 런타임 중의 동적인 정보를 나타내기 때문이라고 추측한다.

```c
/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
```
멤버 변수 중 `magic`에는 전처리된 값인 `INODE_MAGIC (0x494e4f44 = "INOD")`이 들어가는데, 해당 블록 영역이 Inode block임을 인식하는 데 사용된다.


```c
/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}
```
`size`만큼의 바이트를 할당하는 데 필요한 블럭 영역의 수를 계산한다.

```c
/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
}
```
주어진 `inode` 위에 있는 `pos`번째 바이트가 어떤 블럭 위에 있는지를 계산한다. 위의 계산식은 단순히 첫 인덱스인 `inode->data.start`부터 `pos`바이트만큼의 블럭 수를 더해주므로, 한 데이터를 이루는 블럭들은 모두 메모리 상의 연속된 공간에 배치되어있다는 가정이 수반되어야 한다. 만약 `pos`가 데이터 길이를 넘어설 경우 `-1`을 반환한다.


```c
/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}
```
현재 오픈된 Inode들의 리스트인 상술한 `open_inodes`와 이를 초기화하는 함수이다. 본 코드에서 전역적으로 선언되었고 초기화가 필요한 변수는 `open_inodes`밖에 없기 때문에 시작 시 이를 초기화해주는 것으로 충분하다.


```c
/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
```
메모리 위의 블럭들 중 `sector` 인덱스의 블럭에 `length` 바이트의 데이터를 가리킬 Inode 블럭을 생성한다.
- 임시로 `inode_disk` 데이터를 담아둘 공간을 동적할당한 뒤 필요한 메타데이터를 채워준다.
- 디스크 위에 `length` 바이트의 데이터가 들어갈 만큼의 블럭들을 `free_map_allocate` 함수를 통해 확보해준다.
- 확보에 성공할 시 임시로 만들어놨던 `inode_disk` 블럭의 내용을 `block_write` 함수를 통해 `sector` 위치의 블럭에다 복사해준 뒤, 앞서 확보한 데이터 블럭들을 `0`으로 초기화해준다.
- 마지막으로 임시로 할당한 블럭을 `free`해주고 성공 여부를 반환한다.


```c
/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
```
`sector` 위치의 Inode 블럭을 가리키는 Inode를 찾거나 생성하여 주솟값을 반환한다.
- `open_inodes` 리스트를 순회하며 해당 Inode가 이미 open되어있는지를 확인한다. 만약 존재할 경우 `inode_reopen`을 호출한 뒤 해당 Inode를 반환한다.
- 리스트에서 찾지 못했을 경우 새로운 Inode를 동적할당하여 멤버 변수들을 설정해준 뒤 반환한다.


```c
/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}
```
앞선 `inode_open`의 경우와 같이 이미 열려있는 Inode를 다시 열려고 할 때 호출되는 함수. 해당 Inode를 연 횟수인 `inode->open_cnt`를 증가시킨다.


```c
/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
```
`inode->open_cnt`를 감소시켜 열려있던 Inode를 닫는다.
- 만약 `inode->open_cnt`를 감소시켜 `0`이 되었을 경우, 더이상 해당 Inode에 대한 opener가 없다는 뜻이므로 할당했던 Inode를 해제해준다.
- 이떄 만약 `inode_remove` 함수에 의해 `removed` 플래그가 설정되었던 상태라면 해당 Inode 블럭과 데이터 블럭들을 `free_map_release` 함수를 통해 모두 해제해준다.


```c
/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
```

`inode`의 데이터 중 `offset` 위치부터의 데이터를 최대 `size` 바이트만큼 `buffer_` 공간에 복사한다. 한 번에 최대 `BLOCK_SECTOR_SIZE`만큼의 데이터를 복사하며 총 `size`만큼의 바이트를 복사했거나 파일 데이터의 끝에 도달한 경우 종료한다.


```c
/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
```

위의 `inode_read_at`과 유사하게 `inode`의 데이터 블럭 중 `offset` 바이트의 위치에 `buffer_` 공간에 들어있는 데이터를 최대 `size` 바이트만큼 복사한다. 만약 해당 Inode에 `deny_write_cnt` 플래그가 설정되어있을 경우 write를 수행하지 않고 종료한다.


```c
/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}
```
Inode의 `deny_write_cnt` 플래그를 설정하고 해제할 때 호출하는 함수들. 각 opener가 `deny_write_cnt`를 최대 한 번 증가시켰을 것을 ASSERT문을 통해 약하게 검증한다. 완전히 검증하려면 `deny_write_cnt`를 설정한 opener를 리스트화시켜 관리하는 식으로 보완할 수 있을 것이라 생각한다.

```c
/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
```
이외에 편의성을 위해 정의된 getter 함수들.


### File

```c
/* An open file. */
struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };
```

`file` 구조체는 OS에서 관리하는 파일들의 표현 형식으로, 기본적으로 `inode`의 wrapping class 형식으로 구현되어 있다. 추가적으로 현재까지 읽었던 파일의 오프셋인 `pos`와 수정 가능 여부에 대한 플래그인 `deny_write`가 멤버 변수로 존재한다.

`inode`의 `deny_write_cnt`는 `int`형으로 0 이상의 정수값을 가질 수 있도록 로직이 짜여진 반면 `file`의 `deny_write`가 `bool` 형식인 이유는 `inode`는 여러 파일들이 동시에 가리킬 수 있기 때문에 이에 대한 중복 처리가 필요하지만 각 `file`은 독립적으로 존재하기 때문이다.

```c
/* Opens a file for the given INODE, of which it takes ownership,
   and returns the new file.  Returns a null pointer if an
   allocation fails or if INODE is null. */
struct file *
file_open (struct inode *inode) 
{
  struct file *file = calloc (1, sizeof *file);
  if (inode != NULL && file != NULL)
    {
      file->inode = inode;
      file->pos = 0;
      file->deny_write = false;
      return file;
    }
  else
    {
      inode_close (inode);
      free (file);
      return NULL; 
    }
}
```
새로운 `file` 구조체를 동적할당하여 주어진 `inode`를 가리키도록 만들고 멤버 변수를 초기화해준다. 파라미터로 주어진 `inode`에 대해 함수 내에서 따로 `inode_open`이나 `inode_reopen`을 호출해주지 않는 이유가 궁금했는데, Pintos 프로젝트 내에서 `file_open`이 사용된 경우들을 확인하니 모두 함수 호출 이전에 `inode`를 미리 열어주는 식으로 구현이 되어있었다.


```c
/* Opens and returns a new file for the same inode as FILE.
   Returns a null pointer if unsuccessful. */
struct file *
file_reopen (struct file *file) 
{
  return file_open (inode_reopen (file->inode));
}
```
해당 파일이 가리키는 Inode를 가리키는 새로운 `file`을 생성한다. 기존 Inode는 `inode_reopen`을 통해 opener 카운트를 올려줘야 한다.


```c
/* Closes FILE. */
void
file_close (struct file *file) 
{
  if (file != NULL)
    {
      file_allow_write (file);
      inode_close (file->inode);
      free (file); 
    }
}
```
열린 파일을 닫는다(삭제한다). 후술할 `file_allow_write` 함수에서 `deny_write` 플래그가 설정되었을 경우에 대한 로직을 수행한 뒤, 가리키고 있던 `inode`를 닫고 `file` 구조체의 동적할당을 해제한다.


```c
/* Reads SIZE bytes from FILE into BUFFER,
   starting at the file's current position.
   Returns the number of bytes actually read,
   which may be less than SIZE if end of file is reached.
   Advances FILE's position by the number of bytes read. */
off_t
file_read (struct file *file, void *buffer, off_t size) 
{
  off_t bytes_read = inode_read_at (file->inode, buffer, size, file->pos);
  file->pos += bytes_read;
  return bytes_read;
}
```
현재까지 읽은 파일의 오프셋인 `file->pos`부터 최대 `size` 바이트만큼의 정보를 `inode_read_at` 함수를 통해 읽어와 `buffer`에 저장한다. 이후 `file->pos`를 읽어온 바이트 수만큼 증가시켜준다.


```c
/* Reads SIZE bytes from FILE into BUFFER,
   starting at offset FILE_OFS in the file.
   Returns the number of bytes actually read,
   which may be less than SIZE if end of file is reached.
   The file's current position is unaffected. */
off_t
file_read_at (struct file *file, void *buffer, off_t size, off_t file_ofs) 
{
  return inode_read_at (file->inode, buffer, size, file_ofs);
}
```
위의 `file_read`와 흡사하지만, 파일의 오프셋을 `file_ofs` 파라미터를 통해 임의로 지정 가능하다. `file->pos`는 변화시키지 않는다.


```c
/* Writes SIZE bytes from BUFFER into FILE,
   starting at the file's current position.
   Returns the number of bytes actually written,
   which may be less than SIZE if end of file is reached.
   (Normally we'd grow the file in that case, but file growth is
   not yet implemented.)
   Advances FILE's position by the number of bytes read. */
off_t
file_write (struct file *file, const void *buffer, off_t size) 
{
  off_t bytes_written = inode_write_at (file->inode, buffer, size, file->pos);
  file->pos += bytes_written;
  return bytes_written;
}

/* Writes SIZE bytes from BUFFER into FILE,
   starting at offset FILE_OFS in the file.
   Returns the number of bytes actually written,
   which may be less than SIZE if end of file is reached.
   (Normally we'd grow the file in that case, but file growth is
   not yet implemented.)
   The file's current position is unaffected. */
off_t
file_write_at (struct file *file, const void *buffer, off_t size,
               off_t file_ofs) 
{
  return inode_write_at (file->inode, buffer, size, file_ofs);
}
```
`file_read`, `file_read_at`과 동일하게 각각 현재 파일의 오프셋과 명시된 오프셋 `file_ofs`에서 시작하여, `buffer`에 있는 내용을 최대 `size` 바이트만큼 파일에 작성한다.


```c
/* Prevents write operations on FILE's underlying inode
   until file_allow_write() is called or FILE is closed. */
void
file_deny_write (struct file *file) 
{
  ASSERT (file != NULL);
  if (!file->deny_write) 
    {
      file->deny_write = true;
      inode_deny_write (file->inode);
    }
}
```
현재 파일에 이미 `deny_write` 플래그가 설정되어있지 않다면 해당 플래그를 설정하고 `inode_deny_write` 함수를 호출하여 inode의 `deny_write` 카운트 역시 올려준다.


```c
/* Re-enables write operations on FILE's underlying inode.
   (Writes might still be denied by some other file that has the
   same inode open.) */
void
file_allow_write (struct file *file) 
{
  ASSERT (file != NULL);
  if (file->deny_write) 
    {
      file->deny_write = false;
      inode_allow_write (file->inode);
    }
}
```
위의 `file_deny_write`와 정확히 반대의 동작을 수행한다.


```c
/* Returns the size of FILE in bytes. */
off_t
file_length (struct file *file) 

/* Sets the current position in FILE to NEW_POS bytes from the
   start of the file. */
void
file_seek (struct file *file, off_t new_pos)

/* Returns the current position in FILE as a byte offset from the
   start of the file. */
off_t
file_tell (struct file *file)
```
이외에 편의성을 위해 정의된 getter 함수들.