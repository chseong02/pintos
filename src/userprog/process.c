#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/user/syscall.h"
#include "vm/s-page-table.h"

/*---------------------------------------------------------------------------*/
/* Process Control Block */

static struct lock pid_lock;
static struct lock file_lock;
static int file_lock_shares;

/*---------------------------------------------------------------------------*/

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static void parse_args (char *cmd_line_str, char **argv, size_t *argv_len, 
  uint32_t *argc);
static void setup_args_stack (char **argv, size_t *argv_len, uint32_t argc, 
  void **esp);
static pid_t allocate_pid (void);

/* Initialize the locks used throughout process management, 
   initialize the process struct of the main thread, 
   and create the connections between the main thread and process. */
void
process_init(void)
{
  struct process *p;
  struct thread *t;

  // Main Thread
  t = thread_current();

  lock_init(&pid_lock);
  file_lock_shares = 0;
  lock_init(&file_lock);
  
  p = palloc_get_page (PAL_ZERO);
  if(p == NULL)
  {
    palloc_free_page(p);
    PANIC("Main Process Init Fail");
  }

  init_process(p);
  t->process_ptr = p;
  p->tid = t->tid;
}

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  /* Command: File Name + Arguments */
  char *full_cmd_line_copy;
  /* Just File Name */
  char *file_name_copy;
  char *save_ptr;
  struct process *p;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  full_cmd_line_copy = palloc_get_page (0);
  if (full_cmd_line_copy == NULL)
    return TID_ERROR;
  file_name_copy = palloc_get_page (0);
  if (file_name_copy == NULL)
    return TID_ERROR;

  strlcpy (full_cmd_line_copy, file_name, PGSIZE);
  strlcpy (file_name_copy, file_name, PGSIZE);

  strtok_r (file_name_copy, " ", &save_ptr);
  
  /*-------------------------------------------------------------------------*/
  /* Initialize Child Process Info */
  p = palloc_get_page (PAL_ZERO);
  if (p == NULL)
  {
    return TID_ERROR;
  }
  init_process(p);
  list_push_back(&thread_current()->process_ptr->children, &p->elem);
  /*-------------------------------------------------------------------------*/

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create_with_pcb (file_name_copy, PRI_DEFAULT, p, start_process, 
    full_cmd_line_copy);
  palloc_free_page (file_name_copy);
  if (tid == TID_ERROR)
  {
    palloc_free_page (full_cmd_line_copy);
    palloc_free_page (p);
    return TID_ERROR;
  }

  /* System call Exec Load Sync */
  sema_down(&(p->exec_load_sema));
  if (p->pid == PID_ERROR)
  {
    palloc_free_page (p);
    return TID_ERROR;
  }
    
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  struct thread *t;

  /* argv: process arguments str point array */
  /* argv_len: process arguments str length array */
  /* argc: process arguments count */
  char **argv;
  size_t *argv_len;
  uint32_t argc = 0;
  success = true;
  t = thread_current();

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  
  argv = palloc_get_page (0);
  if (argv == NULL)
    success = false;

  argv_len = palloc_get_page (0);
  if (argv_len == NULL)
    success = false;
  
  /* file_name stops at the null character only at the end of the pure file name */
  parse_args (file_name, argv, argv_len, &argc);
  success = load (file_name, &if_.eip, &if_.esp) && success;
  if (success)
    setup_args_stack (argv, argv_len, argc, &if_.esp);
  else
    t->process_ptr->pid = PID_ERROR;
  
  palloc_free_page (file_name);
  palloc_free_page (argv);
  palloc_free_page (argv_len);

  /* System call Exec Load Sync */
  sema_up (&t->process_ptr->exec_load_sema);

  /* If load failed, quit. */
  if (!success) 
    thread_exit ();
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Parse the cmd_line_str and store the starting addresses of the arguments in 
   argv, the lengths of each argument in argv_len, 
   and the number of arguments in argc. */
static void
parse_args (char *cmd_line_str, char **argv, size_t *argv_len, uint32_t *argc)
{
  char *arg, *save_ptr;

  for (arg = strtok_r (cmd_line_str, " ", &save_ptr); arg != NULL; 
    arg = strtok_r (NULL, " ", &save_ptr))
  {
    argv_len[*argc] = strlen (arg) + 1;
    argv[*argc] = arg;
    (*argc)++;
  }
}

/* A function that formats and pushes arguments for the loaded program 
   onto the stack.
   The stack is set according to the following format.

   Example cmd line: "/bin/ls -l foo bar" 
   Address 	    Name 	            Data 	        Type
   0xbffffffc 	argv[3][...] 	    "bar\0" 	    char[4]
   0xbffffff8 	argv[2][...] 	    "foo\0" 	    char[4]
   0xbffffff5 	argv[1][...] 	    "-l\0" 	      char[3]
   0xbfffffed 	argv[0][...] 	    "/bin/ls\0" 	char[8]
   0xbfffffec 	word-align 	      0 	          uint8_t
   0xbfffffe8 	argv[4] 	        0 	          char *
   0xbfffffe4 	argv[3] 	        0xbffffffc 	  char *
   0xbfffffe0 	argv[2] 	        0xbffffff8 	  char *
   0xbfffffdc 	argv[1] 	        0xbffffff5 	   char *
   0xbfffffd8 	argv[0] 	        0xbfffffed 	  char *
   0xbfffffd4 	argv 	            0xbfffffd8 	  char **
   0xbfffffd0 	argc 	            4 	          int
   0xbfffffcc 	return address 	  0 	          void (*) ()   <- esp
*/
static void
setup_args_stack (char **argv, size_t *argv_len, uint32_t argc, 
  void **esp)
{
  /* argv string */
  char *ptr_argv = (char*) *esp;
  for (int i = argc - 1; i >= 0; i--)
  {
    ptr_argv = ptr_argv - (char*) (argv_len[i]);
    strlcpy (ptr_argv, (const char*) argv[i], (size_t) (argv_len[i]));
  }

  /* Word Align */
  ptr_argv = (char *)(((uint32_t) ptr_argv) - ((uint32_t) ptr_argv) % 4);
  ptr_argv -= 4;

  char** ptr_argv_addr = (char **) ptr_argv;
  
  *((uint32_t *) ptr_argv_addr) = 0;
  ptr_argv_addr--;

  /* argv string pointer */
  char* argv_addr_iter_ptr = (char*) *esp; 
  for(int i = argc-1; i >= 0; i--)
  {
    argv_addr_iter_ptr -= argv_len[i];
    *ptr_argv_addr = (char *) argv_addr_iter_ptr;
    ptr_argv_addr--;
  }

  /* argv address */
  *ptr_argv_addr = (char**) (ptr_argv_addr + 1);
  ptr_argv_addr--;

  /* argument count */
  *(uint32_t*) ptr_argv_addr = argc;
  ptr_argv_addr--;
  
  *ptr_argv_addr = 0;
  
  void* ori_if_esp = *esp;
  *esp = (void*) ptr_argv_addr;
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  struct thread *cur = thread_current();
  struct list *children = &cur->process_ptr->children;
  if (child_tid == TID_ERROR)
  {
    return -1;
  }
  
  for (struct list_elem *e = list_begin (children); e != list_end (children); 
    e = list_next (e))
  {
    struct process *p = list_entry(e, struct process, elem);
    if (p->tid == child_tid)
    {
      sema_down (&p->exit_code_sema);
      list_remove (e);
      int exit_code = p->exit_code;
      palloc_free_page (p);
      return exit_code;
    }
  }
  return -1;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  init_s_page_table();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file_lock_acquire();
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;
  file_deny_write(file);
  t->process_ptr->file_exec = file;

 done:
  /* We arrive here whether the load is successful or not. */
  if(!success) file_close(file);
  file_lock_release();
  return success;
}

/* load() helpers. */

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  off_t new_ofs = ofs;
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      
      bool success = s_page_table_file_add(upage, writable, file, new_ofs, 
        page_read_bytes, page_zero_bytes, FAL_USER);
      if (!success)
      {
        // TODO: 앞서 성공한 것 delete
        return false;
      }
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      new_ofs += page_read_bytes;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  void *upage;
  bool success = false;

  /* PALLOC -> FALLOC */
  upage = ((uint8_t *) PHYS_BASE) - PGSIZE;
  kpage = falloc_get_frame_w_upage (FAL_USER | FAL_ZERO, upage);
  if (kpage != NULL) 
    {
      success = install_page (upage, kpage, true) && 
        s_page_table_binded_add(upage, kpage, true, FAL_USER | FAL_ZERO);
      if (success)
        *esp = PHYS_BASE;
      else
      {
        /* PALLOC -> FALLOC */
        falloc_free_frame_from_frame (kpage);
      }
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

/*---------------------------------------------------------------------------*/
/* Process Control Block */

static pid_t
allocate_pid (void)
{
  static pid_t next_pid = 1;
  pid_t pid;

  lock_acquire (&pid_lock);
  pid = next_pid++;
  lock_release (&pid_lock);

  return pid;
}

void
init_process (struct process *p)
{
  memset (p, 0, sizeof *p);
  p->pid = allocate_pid ();
  sema_init (&p->exit_code_sema, 0);
  sema_init (&p->exec_load_sema, 0);
  list_init (&p->children);

  /* Initialize fd table */
  p->fd_table[0].in_use = true;
  p->fd_table[0].type = FILETYPE_STDIN;
  p->fd_table[1].in_use = true;
  p->fd_table[1].type = FILETYPE_STDOUT;
}

/* Wrapper function for file_lock acquire */
void
file_lock_acquire (void)
{
  if (file_lock.holder == thread_current())
  {
    file_lock_shares += 1;
    return;
  }
  lock_acquire (&file_lock);
}

/* Wrapper function for file_lock release */
void
file_lock_release (void)
{
  if (file_lock_shares > 0)
  {
    file_lock_shares -= 1;
    return;
  }
  lock_release (&file_lock);
}

/* Returns minimum available fd, or else -1 if
   no fd is available*/
int
get_available_fd (struct process *p)
{
  for (size_t i = 0; i < OPEN_MAX; i++)
  {
    if (!p->fd_table[i].in_use)
      return i;
  }
  /* No more available fd in the table */
  return -1;
}

/* Allocates the file descriptor FD on process P,
   points file _FILE */
bool
set_fd (struct process *p, int fd, struct file *_file)
{
  if (!(0 <= fd && fd < OPEN_MAX)) return false;
  if (p->fd_table[fd].in_use) return false;
  p->fd_table[fd].file = _file;
  p->fd_table[fd].in_use = true;
  /* Currently there's no way to open STDIN or STDOUT
     Unless there's dup syscall or something */
  p->fd_table[fd].type = FILETYPE_FILE;
  return true;
}

/* Free the file descriptor FD on process P */
void
remove_fd (struct process *p, int fd)
{
  if(!(2 <= fd && fd < OPEN_MAX)) return;
  /* Intended not to check the validity */
  p->fd_table[fd].in_use = false;
}