/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value)
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0)
    {
      //printf("sema down %d\n", sema->value);
      list_insert_ordered(&sema->waiters, &thread_current ()->elem, thread_priority_compare,NULL);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema)
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0)
    {
      sema->value--;
      success = true;
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  sema->value++;
  if (!list_empty (&sema->waiters)){
    list_sort(&sema->waiters, thread_priority_compare, NULL);
    struct thread *first_item = list_entry (list_pop_front (&sema->waiters),struct thread, elem);
    thread_unblock(first_item);
  }

  intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void)
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++)
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_)
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++)
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  //inicialización de lock, semaphore en 1, holder en NULL.
  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  enum intr_level old_level;

  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  old_level = intr_disable ();

  // priority donation, when locking
  struct lock *current_lock = lock;
  struct thread *current_lock_holder = lock->holder; // current holder thread
  struct thread *current_thread = thread_current();

  //Si nadie tiene el lock, se lo da al que lo pide (thread_current)
  if(current_lock_holder == NULL){
    sema_down (&lock->semaphore);   //baja semáforo a 0
    lock->holder = thread_current();  //el holder del lock es el thread actual
    list_push_front(&current_thread->locks_owned_list, &current_lock->elem); //el thread ahora tiene el lock
  }else{
    //ya alguien tiene el lock
    bool success = sema_try_down (&lock->semaphore); //success debería SIEMPRE ser false
    if(!success){
      //inserta el thread que pide a la lista de waiters del lock
      struct semaphore *current_lock_semaphore = &current_lock->semaphore;
      list_push_back(&(current_lock_semaphore->waiters), &current_thread->elem);
      current_thread->waiting_lock = lock;  //el thread está ahora esperando por el lock.

      //intenta donar al holder actual
      //Esto debería cambiar la prioridad del current holder si la del actual es mayor.
      //Si no, pues no pasa nada.
      thread_priority_donate(current_lock_holder, current_thread->priority);

      thread_block();
    }
  }

  intr_set_level (old_level);


  /*while (t_holder != NULL && t_holder->priority < t_current->priority) {
    // Donate priority to [t_holder]
    thread_priority_donate(t_holder, t_current->priority);

    if (current_lock->holder->priority < t_current->priority) {
      current_lock->holder->priority = t_current->priority;
    }

  // ESTO PARA CHAINING. DONA primero ^ , después revisa si el holder está esperando a otro lock
  //si está esperando a otro lock, repite el proceso que el holder le done al holder del que está esperando
  // y así enciclado, hasta que ya no hayan holders. (por eso while holder != null.)
      current_lock = t_holder->waiting_lock;
      if(current_lock == NULL) break;
      t_holder = current_lock->holder;
  }*/





  /* MMI CODIGO

  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  //printf("current thread %s quiere lock\n",thread_current()->name);

  struct thread *lock_holder_thread = lock->holder;
  if(lock_holder_thread != NULL){
    //printf("Ya %s tiene el lock con prioridad %d\n", lock_holder_thread->name, lock_holder_thread->priority);
    //printf("current prioridad %d\n\n", thread_current()->priority);
    thread_priority_donate(thread_current(), lock_holder_thread->priority);
    /*if(thread_current()->priority > lock_holder_thread->priority){
      lock_holder_thread->priority = thread_current()->priority;
      //printf("se va a hacer yield\n");
        //thread_yield();
    }else{

    }
  }else{
    //printf("El lock está libre\n\n");
    list_push_front (&thread_current ()->locks_owned_list, &lock->elem);
    thread_current ()->waiting_lock = NULL;
  }

  sema_down (&lock->semaphore);
  lock->holder = thread_current ();*/
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock)
{
  enum intr_level old_level;

  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  struct list_elem *e;
  struct thread *waiting_thread; //PENDIENTE DE USAR
  int highest_waiting_priority = PRI_MIN; //PENDIENTE DE USAR
  struct lock *current_lock = lock;
  struct thread *current_lock_holder = lock->holder; // current holder thread
  struct thread *current_thread = thread_current();
  struct semaphore *current_lock_semaphore = &current_lock->semaphore;


  old_level = intr_disable ();  //apagar interrupciones


  //PASOS PARA SOLTAR EL LOCK:

  //quita el lock de los locks_owned_list del thread
  printf('ANTES DEL PANIC DE LIST NEXT\n');
  for (e = list_begin(&current_thread->locks_owned_list); e != list_end(&current_thread->locks_owned_list); e = list_next (e) ){
    //Itera a través de todos los locks que tiene el thread buscando el lock que se está liberando
    struct lock *locks_owned_list_item = list_entry(e, struct lock, elem);
    if (current_lock == locks_owned_list_item){
      e = list_remove (e);  //Quita de la lista el lock actual que se acaba de liberar
    }
  }
  lock->holder = NULL;          //libera el lock (ya nadie lo holdea)

  //Listo, ya se quitó el lock del thread. Ya no es holder ni lo tiene en su lista de locks_owned.


  //si el thread actual ya no tiene más locks, regresa a su priority original
  if(list_empty(&current_thread->locks_owned_list)){
    thread_priority_donate(current_thread, current_thread->true_priority);
  }else{
    //Cambiar prioridad a la siguiente prioridad más alta de los locks_owned.
    //Implementar esto para multiple donation.
  }

  //libera al primer item del waiters list: (COMENTADO PORQUE ESTO LO HACE SEMA_UP)
  /*if(!list_empty(&current_lock_semaphore->waiters)){
    struct list_elem *first_waiter_elem = list_begin(&current_lock_semaphore->waiters);
    struct thread *first_waiter_thread = list_entry(first_waiter_elem, struct thread, elem);

    list_remove(first_waiter_elem); //lo saca de los waiters
    thread_unblock(first_waiter_thread);// lo desbloquea para que se puede ejecutar
  }*/

  sema_up (current_lock_semaphore);   //aumenta el semaphore, dando permiso al siguiente en espera a tomar el lock

  intr_set_level (old_level); //habilitar interrupciones








  /*struct list *cur_thread_locks_owned_list = &cur_thread->locks_owned_list; //lista de locks que tiene el thread actual

  for (e = list_begin(cur_thread_locks_owned_list); e != list_end(cur_thread_locks_owned_list);   ){
    //Itera a través de todos los locks que tiene el thread
    struct lock *owned_lock = list_entry(e, struct lock, elem);
    if (lock == owned_lock){
      e = list_remove (e);  //Quita de la lista el lock actual que se acaba de liberar
      //break;
    }else{
      //Aplicar aquí condiciones para multiple donation / nested donation
      if(e != list_end(cur_thread_locks_owned_list)){
        e = list_next (e);
      }
    }
  }

  //Regresa a priority original, o a siguiente priority donada:
  if (list_empty(cur_thread_locks_owned_list)) {
    // si ya no hay locks, regresa prioridad a su prioridad original/real
    thread_priority_donate(cur_thread, cur_thread->true_priority);
  }else {
    // donated: lookup the donors, find the highest priority lock
    // then it should be the (newly updated) donated priority of t

    /*list_sort(cur_thread_locks_owned_list, thread_priority_compare, NULL); // TODO why it is needed?
    struct lock *highest_lock = list_entry( list_front(&(t_current->locks)), struct lock, lockelem );
    thread_priority_donate(t_current, highest_lock->priority);
  }

  //cuando se regresó priority después de donation, verificar si hay que hacer yield:
  verificar_yield_a_ready_thread();*/



  //AL soltar el lock, buscar entre la lista de donors a ver si hay alguna otra
  //prioridad mayor a la real. Obtener la prioridad mayor de los donantes restantes
  // ^ ESTO EN EL IF / FOR
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock)
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock)
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  sema_init (&waiter.semaphore, 0);
  list_insert_ordered (&cond->waiters, &waiter.elem, thread_priority_compare, NULL);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)){
    list_sort(&cond->waiters, thread_priority_compare, NULL);
    sema_up (&list_entry (list_pop_front (&cond->waiters), struct semaphore_elem, elem)->semaphore);

  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
