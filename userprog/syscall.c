#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"// Include para validación de direcciones
#include "filesys/file.h" // para poder usar file_write
#include "filesys/off_t.h" // para poder utilizar off_t

static void syscall_handler (struct intr_frame *);
int halt();
int write(int fd, const void* buffer, unsigned size);
bool create(const char* file, off_t initial_size);
bool remove(const char* file);
static void exit (int status);
int exec(char* cmdline);
int wait(int child_tid);
int open(const char* file_name);
void close(int fd);

//helper functions:
static bool get_int_arg (const uint8_t *uaddr, int pos, int *pi);
void* check_addr(const void*);
struct process_file* files_search(struct list* files, int fd);

//helper structs:
struct process_file{
  struct file* fileptr;
  int fd;
  struct list_elem elem;
};

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  int sys_code = *(int*)f->esp;

  //Validacion de address, la función llama a exit si está mal
  check_addr((int*)f->esp);

  switch(sys_code){
    case SYS_HALT:
        halt();
        break;
    case SYS_EXIT:
        {
        int status = *((int*)f->esp + 1);
        exit(status);
        break;
        }
    case SYS_EXEC:
        {
          check_addr(((int*)f->esp + 1));
		      check_addr(*((int*)f->esp + 1));
          char *cmd_line = (char*)(*((int*)f->esp + 1));
          f->eax = exec(cmd_line);
          break;
        }
    case SYS_WAIT:
        {
          check_addr((int*)f->esp + 1);
          int child_tid = *((int*)f->esp + 1);
          f->eax = wait(child_tid);
          break;
        }
    case SYS_CREATE:
        {
          check_addr(((int*)f->esp + 1));
		      check_addr(*((int*)f->esp + 1));
          char* file = (char*)(*((int*)f->esp + 1));
          off_t initial_size = (off_t)(*((int*)f->esp + 2));
          f->eax = create(file, initial_size);
          break;
        }
    case SYS_REMOVE:
        {
          char* file = (char*)(*((int*)f->esp + 1));
          f->eax = remove(file);
          break;
        }
    case SYS_OPEN:
        {
          char* file = (char*)(*((int*)f->esp + 1));
          f->eax = open(file);
          break;
        }
    case SYS_FILESIZE:
        //printf("SYS_FILESIZE.\n");
        check_addr(((int*)f->esp + 1));
        struct process_file* pfile = *(((int*)f->esp + 1));
        lock_acquire(&filesys_lock);
        f->eax = file_length (files_search(&thread_current()->files, pfile->fileptr));
        lock_release(&filesys_lock);
        break;
    case SYS_READ:
        //printf("SYS_READ.\n");
        break;
    case SYS_WRITE:
        {
          int fd = *((int*)f->esp + 1);
        void* buffer = (void*)(*((int*)f->esp + 2));
        unsigned size = *((unsigned*)f->esp + 3);
        //run the syscall, a function of your own making
        //since this syscall returns a value, the return value should be stored in f->eax
        f->eax = write(fd, buffer, size);
        break;
        }
    case SYS_SEEK:
        //printf("SYS_SEEK.\n");
        break;
    case SYS_TELL:
        //printf("SYS_TELL.\n");
        break;
    case SYS_CLOSE:
        {
          //int fd = *((int*)f->esp + 1);
          //close(fd);
          //f->eax = fd;
          break;
        }
    default:
        //printf ("system call %d !\n", sys_code);
        thread_exit ();
    }

}

void close(int fd) {
  if (fd != NULL) {
    file_close(fd);
  }
}

int open(const char* file_name) {
  /*if (file_name == NULL || strlen(file_name) == 0 || !is_user_vaddr(file_name)) {
    return -1;
  }*/

  //check_addr(file_name);
  //check_addr(*file_name);

  lock_acquire(&filesys_lock);
  struct file* opened_file = filesys_open(file_name);
  lock_release(&filesys_lock);

  if(opened_file == NULL){
    return -1;
  }else{
    struct process_file *pfile = malloc(sizeof(*pfile));
    pfile->fileptr = opened_file;
    pfile->fd = thread_current()->fd_count;
    thread_current()->fd_count++;
    list_push_back(&thread_current()->files,&pfile->elem);
    return pfile->fd;
  }
}

int exec(char* cmdline){
  lock_acquire(&filesys_lock);
	char * fn_cp = malloc (strlen(cmdline)+1);
	  strlcpy(fn_cp, cmdline, strlen(cmdline)+1); //crea una copia de cmdline

	  char * save_ptr;
	  fn_cp = strtok_r(fn_cp," ",&save_ptr);//obtiene el primer item (nobmre del archivo)

	 struct file* f = filesys_open (fn_cp);//Lo carga

	  if(f==NULL)//Si el archivo existe lo ejecuta, si no existe retorna -1
	  {
	  	lock_release(&filesys_lock);
	  	return -1;
	  }else{
	  	file_close(f);
	  	lock_release(&filesys_lock);
	  	return process_execute(cmdline);
	  }
}

int wait(int child_tid){
  //check_addr(child_tid);
  return process_wait(child_tid);
}

bool create(const char* file, off_t initial_size){
  check_addr(file);
  lock_acquire(&filesys_lock);
  bool result = filesys_create(file, initial_size);
  lock_release(&filesys_lock);
  return result;
}

bool remove(const char* file){
  bool result = filesys_remove(file);
  return result;
}

int write(int fd, const void* buffer, unsigned size){
  int bytes_written;

  if(!is_user_vaddr(buffer) || !is_user_vaddr(buffer+size)){ // SI las direcciones no son validas, sale
    thread_exit();
  }

  bytes_written = fd_write (fd, buffer, size);

  return bytes_written;

}

int halt(){
  shutdown_power_off();
}

void exit(int status){
  thread_current()->exit_status = status;
  thread_current()->parent->child_status = status;
  // EL PRINT DE STATUS SE MOVIÓ A THREAD_EXIT
  //printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();

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

}



/// HELPER FUNCTIONS: funciones de ayuda para parsear argumentos:
/* Gets an integer argument at the specified positon from user space. */
static bool get_int_arg (const uint8_t *uaddr, int pos, int *pi){
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


//Funcion para buscar files en la lista de files:
struct process_file* files_search(struct list* files, int fd)
{

	struct list_elem *e;

      for (e = list_begin (files); e != list_end (files);
           e = list_next (e))
        {
          struct process_file *f = list_entry (e, struct process_file, elem);
          if(f->fd == fd)
          	return f;
        }
   return NULL;
}


//FUnción que ayuda a verificar punteros. Si es válido lo devuelve, sno de una lo cierra
void* check_addr(const void *vaddr)
{
	if (!is_user_vaddr(vaddr))
	{
		exit(-1);
		return 0;
	}
	void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
	if (!ptr)
	{
		exit(-1);
		return 0;
	}
	return ptr;
}
