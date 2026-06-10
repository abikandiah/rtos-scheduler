#include "Resource.h"

#include "TCB.h"

TCB* Resource::release_and_get_waiter(TCB* releasing_task) {
  on_release(releasing_task);

  TCB* waiter = wait_queue.pop_head();
  if (waiter) on_handoff(waiter);
  return waiter;
}
