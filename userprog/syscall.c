#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"// Include para validación de direcciones
#include "filesys/file.h" // para poder usar file_write

static void syscall_handler (struct intr_frame *);
int write(int fd, const void* buffer, unsigned size);
static void exit (int status);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  int sys_code = *(int*)f->esp;
  switch(sys_code){
    case SYS_HALT:
        printf("SYS_HALT.\n");
        break;
    case SYS_EXIT:
        printf("SYS_EXIT.\n");
        exit(999);
        break;
    case SYS_EXEC:
        printf("SYS_EXEC.\n");
        break;
    case SYS_WAIT:
        printf("SYS_WAIT.\n");
        break;
    case SYS_CREATE:
        printf("SYS_CREATE.\n");
        break;
    case SYS_REMOVE:
        printf("SYS_REMOVE.\n");
        break;
    case SYS_OPEN:
        printf("SYS_OPEN.\n");
        break;
    case SYS_FILESIZE:
        printf("SYS_FILESIZE.\n");
        break;
    case SYS_READ:
        printf("SYS_READ.\n");
        break;
    case SYS_WRITE:
        printf("");
        int fd = *((int*)f->esp + 1);
        void* buffer = (void*)(*((int*)f->esp + 2));
        unsigned size = *((unsigned*)f->esp + 3);
        //run the syscall, a function of your own making
        //since this syscall returns a value, the return value should be stored in f->eax
        f->eax = write(fd, buffer, size);
        break;
    case SYS_SEEK:
        printf("SYS_SEEK.\n");
        break;
    case SYS_TELL:
        printf("SYS_TELL.\n");
        break;
    case SYS_CLOSE:
        printf("SYS_CLOSE.\n");
        break;
    default:
        printf ("system call %d !\n", sys_code);
        thread_exit ();
    }

}

int write(int fd, const void* buffer, unsigned size){
  int bytes_written;

  if(!is_user_vaddr(buffer) || !is_user_vaddr(buffer+size)){ // SI las direcciones no son validas, sale
    thread_exit();
  }

  bytes_written = fd_write (fd, buffer, size);

  return bytes_written;

}

void exit(int status){
  printf("%s: exit(%d)\n", thread_current()->name, status);

  // The process exits.
  // wake up the parent process (if it was sleeping) using semaphore,
  // and pass the return code.
  /*struct process_control_block *pcb = thread_current()->pcb;
  if(pcb != NULL) {
    pcb->exitcode = status;
  }
  else {
    // pcb == NULL probably means that previously
    // page allocation has failed in process_execute()
  }*/

  thread_exit();
}



/// HELPER FUNCTIONS: funciones de ayuda para parsear argumentos:
/* Gets an integer argument at the specified positon from user space. */
static bool get_int_arg (const uint8_t *uaddr, int pos, int *pi)
{
  return read_int (uaddr + sizeof (int) * pos, pi);
}



// HELPER FUNCTIONS 2: otras funciones de ayuda para los syscalls

//Función que imprime, a consola si es 1, o al archivo si es distinto de 1.
int
fd_write (int fd, const void *buffer, int size)
{
  struct file *file;
  int bytes_written = -1;

  if (fd == 1)
    {
      /* if fd =1  write to the console. */
      putbuf (buffer, size);
      bytes_written = size;
    }
  else
    {
      /* esto para imprimir a archivo, pero aún no implementado*/
      /*file = fd_get_file (fd);
      if (file != NULL && !file_is_dir (file))
        bytes_written = file_write (file, buffer, size);*/
    }
  return bytes_written;
}
