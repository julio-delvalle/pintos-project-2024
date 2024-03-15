#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
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

#include "threads/synch.h" // Para poder usar sema_up y sema_down

#define MAX_ARGS 128
#define MAX_ARG_LENGTH 128 //el argumento solo puede tener 128 caracteres

static thread_func start_process NO_RETURN;
static bool load (void *process_data_arg, void (**eip) (void), void **esp);
static bool setup_stack (int argc, char* full_cmdline, void **esp);

//Nuevas funciones para userprog:
static bool install_page (void *upage, void *kpage, bool writable);
void parse_args(char *line, char *argv[], int argc);
size_t calc_argv_size(char **argv, int argc);
int count_args(char *line);



/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (int argc, char* full_cmdline, void **esp)
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success){
        *esp = PHYS_BASE;

        //printf("esp: %x\n", *esp);

        unsigned long int argument_adresses[argc];
        char *token, *save_ptr;
        int saved_arguments = 0;

        //DEBUG:: QUITAR
        //uintptr_t start = (uintptr_t)*esp;



        //Paso 2: obtiene los argumentos, y luego los agrega en orden inverso
        char *arguments[argc]; //array que va a tener los argumentos
        for (token = strtok_r(full_cmdline, " ", &save_ptr); token != NULL && saved_arguments < MAX_ARGS; token = strtok_r(NULL, " ", &save_ptr)){
          arguments[saved_arguments] = malloc((strlen(token)+1)*sizeof(char));
          snprintf(arguments[saved_arguments],strlen(token)+1, "%s", token);
          saved_arguments++;
        }
        for (int i=0;i<saved_arguments;i++){
          token = arguments[(saved_arguments-1) - i];
          *esp -= (strlen(token)+1); //Mueve el stack pointer el tamaño del string
          //printf("esp: %x\n", *esp);
          memcpy(*esp, token, strlen(token)+1); //Agrega el string al stack
          argument_adresses[i] = (unsigned long int)*esp; //guarda la dirección del arg.
        }


        //Paso 3, word align:
        unsigned long int word_align = (unsigned long int)*esp % 4;
        *esp -= word_align; //Va a mover el esp 0, 1, 2 o 3;
        memset(*esp, 0x0, word_align);


        //Paso 4, argumento de 4 bytes de 0s
        *esp -= 4;
        memset(*esp, 0x0, 4);

        //Paso 5: guardar las direcciones al stack. Recordar que las direcciones se fueron guardando en argument_adressesi
        for (int i = 0; i < saved_arguments; i++){
          *esp -= sizeof(char*);
          memcpy(*esp, &argument_adresses[i], sizeof(char*));
        }


        //Paso 6: guardar address de argv[0] ¿¿¿QUÉ ES ESTO??? Address del primer argumento?
        unsigned long int argument_start_address = (unsigned long int)*esp;
        *esp -= sizeof(char**);
        memcpy(*esp, &argument_start_address, sizeof(char**));

        //Paso 7 guardar número de argumentls
        *esp -= 4;
        memcpy(*esp, &argc, 4); //argc es un int que viene como argumento de setup_stack;


        //Paso 8: guardar null pointer como return address
        *esp -= sizeof(char**);
        memset(*esp, 0, sizeof(char**));

        //DEBUG:: QUITAR
        /*char buf[start - (uint32_t)*esp + 1];
        memcpy(buf, *esp, start - (uint32_t)*esp);
        hex_dump((int)*esp, buf, start - (uintptr_t)*esp, true);*/

      }else{
        palloc_free_page (kpage);
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
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}



/////--------------------------









/* Defining struct of data used for process stack setup */
struct process_data {
  char* file_name;
  int argc;
  char **argv;
  char* full_cmdline;
} process_data;

/* Function that parses command line input */
void parse_args(char *line, char *argv[], int argc) {
  //printf("dentro parse args, con %d args\n", argc);

  char *token, *save_ptr;
  int count = 0;

  // Getting arguments, increasing args size and checking current size
  //argv = malloc(argc*sizeof(char*));

  for (token; token != NULL && count < MAX_ARGS; token = strtok_r(NULL, " ", &save_ptr))
  {
    if(count != 0){
      //printf("token %s\n",token);
      size_t arg_len = strlen(token) + 1; // Consider null terminator

    }
    //argv[count] = palloc_get_page(0);
    /*if (argv[count] == NULL) {
      return TID_ERROR;
    }
    strlcpy(argv[count], token, arg_len);*/
    count++;
  }
}

/* Function that counts args in command line */
int count_args(char *line)
{
  int argc = 0;
  char *token, *save_ptr;

  for (token = strtok_r(line, " ", &save_ptr) ; token != NULL ; token = strtok_r(NULL, " ", &save_ptr)){
    //printf("counted %d args\n", argc);
    argc++;
  }
    // Counting args

  return argc;

}

/* Calculate size of all elements of argv */
size_t calc_argv_size(char **argv, int argc)
{
  size_t size = 0;
  int i = 0;
  for(i ; i < argc ; i++){
    size += strlen(argv[i]) + 1;
  }
  size += (4 - size % 4) % 4; // Alignment
  return size;
}


/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
{
  /*
  * MAX_PAGE_SIZE 4096
  * MEAN_ARG_SIZE 32
  * MAX_ARGS = MAX_PAGE_SIZE/MEAN_ARG_SIZE = 128
  */

  char *fn_copy, *token_ptr, *fn_copy2, *fn_copy3;
  tid_t tid;
  struct process_data *process_data_struct;

  /* Allocationg memory for process_data struct */
  struct process_data *args_data = palloc_get_page(sizeof(struct process_data));
  /*if (args_data == NULL) {
      palloc_free_page(argv);
      return TID_ERROR;
  }*/

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);
    /* Counting arguments */
  int argc = count_args(fn_copy);

  // Get First token
  fn_copy2 = palloc_get_page (0);
  if (fn_copy2 == NULL)
  {
    palloc_free_page (fn_copy2);
    return TID_ERROR;  // No args
  }
  strlcpy (fn_copy2, file_name, PGSIZE);
  args_data->file_name = strtok_r(fn_copy2, " ", &token_ptr);
  //printf("Se obtuvo file_name_token con valor %s\n",args_data->file_name);



  args_data->full_cmdline = palloc_get_page (0);
  if (args_data->full_cmdline == NULL)
  return TID_ERROR;
  strlcpy (args_data->full_cmdline, file_name, PGSIZE);




  /* Parse file_name and arguments */
  char **argv = palloc_get_page(argc*sizeof(char *)); // Get memmory for arguments array
  if (argv == NULL) {
    palloc_free_page(fn_copy);
    return TID_ERROR;
  }
  //parse_args(fn_copy3, argv, argc);



  /* Assing data */
  args_data->argc = argc;
  //args_data->file_name = malloc(sizeof(file_name_token));
  //strlcpy(args_data->file_name, file_name_token, sizeof(file_name_token));
  //args_data->file_name = file_name_token;
  args_data->argv = argv;

  //printf("se creó args_data con argc %d , file_name %s , argv %d\n", args_data->argc, args_data->file_name, args_data->argv);


  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (args_data->file_name, PRI_DEFAULT, start_process, args_data);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy);
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *process_data_arg)
{
  struct process_data *process_data = (struct process_data *) process_data_arg;
  char *file_name = process_data->file_name;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (process_data, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page (file_name);
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

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED)
{
  sema_down(&thread_current()->wait_child_sema);
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
  // Process Termination msg
  printf ("%s: exit(%d)\n", cur->name, cur->exit_status);
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

static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (void *process_data_arg, void (**eip) (void), void **esp)
{
  struct process_data *process_data = (struct process_data *) process_data_arg;
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (process_data->file_name);
  if (file == NULL)
    {
      printf ("load: %s: open failed\n", process_data->file_name);
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
      printf ("load: %s: error loading executable\n", process_data->file_name);
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
  if (!setup_stack (process_data->argc, process_data->full_cmdline, esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
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
  while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false;
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable))
        {
          palloc_free_page (kpage);
          return false;
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

