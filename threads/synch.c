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

#include "tests/threads/tests.h"

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
  /* --- CÓDIGO ORIGINAL ---
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  sema_down (&lock->semaphore);
  lock->holder = thread_current ();
  */
  enum intr_level old_level;

  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  //printf('lock_acquire antes de intr_disable\n');

  old_level = intr_disable ();

  struct thread *current_lock_holder = lock->holder; // current holder thread
  struct semaphore *lock_semaphore = &lock->semaphore; // current holder thread

  int nesting = 0;
  bool test_nesting = true;
  //printf("sema try down de lock con value %d",lock_semaphore->value);

  if(!lock_try_acquire(lock)){
    //msg('hola\n');
    //Si el lock no está disponible:

    if(!test_nesting){


      thread_priority_donate(current_lock_holder, thread_current()->priority);

      //Sin importar si la prioridad donada es la más alta donada, se guarda a la lista de donadas
      struct donation_received_elem donation_elem;
      donation_elem.lock = lock;
      donation_elem.priority = thread_current()->priority;
      //Guarda en la lista de donaciones del thread holder el elemento donado.
      list_push_front(&current_lock_holder->donations_received_list, &donation_elem.elem);





    }else{
      struct lock *lock_causing_donation = lock;
      struct thread *thread_donating = thread_current();
      int priority_to_set = thread_current()->priority;
      //Donación de prioridad, como ciclo en caso de nesting.
      //Dona al current holder, si el current_holder está esperando otro lock, se dona a su current_holder, y así enciclado.
      while(test_nesting && current_lock_holder != NULL && nesting < 8 ){

        //intentar darle priority del thread actual al holder del lock:
        if(current_lock_holder->priority <= priority_to_set){
          thread_priority_donate(current_lock_holder, priority_to_set);
        }

        //Sin importar si la prioridad donada es la más alta donada, se guarda a la lista de donadas
        struct donation_received_elem donation_elem;
        donation_elem.thread = thread_donating;
        donation_elem.lock = lock_causing_donation;
        donation_elem.priority = priority_to_set;
        //Guarda en la lista de donaciones del thread holder el elemento donado.
        list_push_front(&current_lock_holder->donations_received_list, &donation_elem.elem);


        if(&current_lock_holder->waiting_lock->holder != NULL){  //Si waiting lock existe, es decir, está esperando por otro lock
          lock_causing_donation = current_lock_holder->waiting_lock;   //El nuevo current_lock_holder es el del lock que está esperando el actual
          current_lock_holder = current_lock_holder->waiting_lock->holder;   //El nuevo current_lock_holder es el del lock que está esperando el actual
        }else{
          current_lock_holder = NULL;
        }
        nesting++;
      }
    }

    //guardar en nuestro thread que estamos esperando por este lock.
    thread_current()->waiting_lock = lock;

    sema_down (&lock->semaphore);
    lock->holder = thread_current ();
    }


  // si sí estaba disponible, lock_try_acquire ya hizo lo necesario. Solo guardar que mi thread ya no espera al lock.
  thread_current()->waiting_lock = NULL;

  intr_set_level (old_level);
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
  int next_priority = thread_current()->true_priority;

  struct list_elem *e;
      for (e = list_begin (&thread_current()->donations_received_list); e != list_end (&thread_current()->donations_received_list); e = list_next (e)) {
        struct donation_received_elem *donation_elem = list_entry(e, struct donation_received_elem, elem);
        if(donation_elem->lock == lock){
          //Quita de la lista de donaciones recibidas la donación causada por el lock actual que se está liberando.
          list_remove(e);
        }
      }

    //Con la lista actualizada, obtiene la siguiente prioridad más alta
    next_priority = get_highest_donation_prio_received(thread_current(), lock);


  //le coloca al thread la siguiente prio más alta.
  thread_priority_donate(thread_current(), next_priority);

  lock->holder = NULL;
  sema_up (&lock->semaphore);



  intr_set_level (old_level);
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
    /* aqui ? */
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
  //list_push_back (&cond->waiters, &waiter.elem);
  list_insert_ordered (&cond->waiters, &waiter.elem, &cond_priority_compare, NULL);
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
    list_sort(&cond->waiters, cond_priority_compare, NULL);
    sema_up (&list_entry (list_pop_front (&cond->waiters),struct semaphore_elem, elem)->semaphore);
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



//NUEVAS:

/* Used to keep the ready list in effective priority order. */
bool cond_priority_compare (const struct list_elem *a, const struct list_elem *b, void *aux)
{
  //printf("dentro de thread_priority_compare\n");
  struct semaphore_elem *semaphore_elem1 = list_entry (a, struct semaphore_elem, elem);
  struct semaphore_elem *semaphore_elem2 = list_entry (b, struct semaphore_elem, elem);

  struct semaphore *semaphore1 = &semaphore_elem1->semaphore;
  struct semaphore *semaphore2 = &semaphore_elem2->semaphore;

  /*struct thread *first_item = list_entry (list_pop_front (&sema->waiters),struct thread, elem);*/

  struct thread *thread_sema1 = list_entry (list_begin(&semaphore1->waiters), struct thread, elem);
  struct thread *thread_sema2 = list_entry (list_begin(&semaphore2->waiters), struct thread, elem);


  ASSERT (thread_sema1 != NULL);
  ASSERT (thread_sema2 != NULL);

  //printf("thread_sema1 prio = %d\nthread_sema2 prio = %d\n", thread_sema1->priority , thread_sema2->priority);
  return thread_sema1->priority > thread_sema2->priority;
}
