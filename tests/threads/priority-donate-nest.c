/* Low-priority main thread L acquires lock A.  Medium-priority
   thread M then acquires lock B then blocks on acquiring lock A.
   High-priority thread H then blocks on acquiring lock B.  Thus,
   thread H donates its priority to M, which in turn donates it
   to thread L.

   Based on a test originally submitted for Stanford's CS 140 in
   winter 1999 by Matt Franklin <startled@leland.stanford.edu>,
   Greg Hutchins <gmh@leland.stanford.edu>, Yu Ping Hu
   <yph@cs.stanford.edu>.  Modified by arens. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

struct locks
  {
    struct lock *a;
    struct lock *b;
  };

static thread_func medium_thread_func;
static thread_func high_thread_func;

void
test_priority_donate_nest (void)
{
  struct lock a, b;
  struct locks locks;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  lock_init (&a);
  lock_init (&b);

  lock_acquire (&a);

  locks.a = &a;
  locks.b = &b;
  thread_create ("medium", PRI_DEFAULT + 1, medium_thread_func, &locks);
  thread_yield ();
  msg ("Low thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 1, thread_get_priority ());

  thread_create ("high", PRI_DEFAULT + 2, high_thread_func, &b);
  thread_yield ();
  msg ("Low thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 2, thread_get_priority ());

/*
  msg("--!-- ANTES DE thread (%s) release a",thread_name());
  msg("--!-- thread (%s) va a soltar a, con donations received size %d",thread_name(), list_size(&thread_current()->donations_received_list));
  msg("--!-- lock (a) tiene holder (%s), con donations received sizeee %d\n",a.holder->name, list_size(&a.holder->donations_received_list));
  struct donation_received_elem *donation_elem1 = list_entry(list_pop_front(&a.holder->donations_received_list), struct donation_received_elem, elem);
    msg("--!-- lock (a). accediento a donation_elem1=>thread=>name test2 (%s) ",donation_elem1->thread->name);
    msg("--!-- lock (a). accediendo a donation+elem1=>priority (%d)", donation_elem1->priority);
    msg("--!-- lock (a).holder tiene una prioridad 1 donada por (%s) de priority (%d)",donation_elem1->thread->name, donation_elem1->priority);
    msg("--!-- el donation_elem1 es por causa del lock (a)? (%s)\n",(donation_elem1->lock == &a) ? "true" : "false");
  struct donation_received_elem *donation_elem2 = list_entry(list_pop_front(&a.holder->donations_received_list), struct donation_received_elem, elem);
    msg("--!-- lock (a).holder tiene una prioridad 2 donada por (%s) de priority (%d)",donation_elem2->thread->name, donation_elem2->priority);
    msg("--!-- el donation_elem2 es por causa del lock (a)? (%s)\n",(donation_elem2->lock == &a) ? "true" : "false");
    list_push_front(&a.holder->donations_received_list, &donation_elem2->elem);
    list_push_front(&a.holder->donations_received_list, &donation_elem1->elem);
    msg("Se insertaron.");*/


  lock_release (&a);
  thread_yield ();
  msg ("Medium thread should just have finished.");
  msg ("Low thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT, thread_get_priority ());
}

static void
medium_thread_func (void *locks_)
{
  struct locks *locks = locks_;

  lock_acquire (locks->b);
  lock_acquire (locks->a);

  msg ("Medium thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 2, thread_get_priority ());
  msg ("Medium thread got the lock.");

  lock_release (locks->a);
  thread_yield ();

  lock_release (locks->b);
  thread_yield ();

  msg ("High thread should have just finished.");
  msg ("Middle thread finished.");
}

static void
high_thread_func (void *lock_)
{
  struct lock *lock = lock_;

  lock_acquire (lock);
  msg ("High thread got the lock.");
  lock_release (lock);
  msg ("High thread finished.");
}
