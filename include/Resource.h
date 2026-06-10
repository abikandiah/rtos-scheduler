#pragma once

#include "IntrusiveLinkedList.h"

class TCB;

class Resource {
 public:
  virtual ~Resource() = default;
  virtual TCB* get_owner() const = 0;
  TCB* release_and_get_waiter(TCB* releasing_task);
  void queue_waiter(TCB* waiter) { wait_queue.add_to_tail(waiter); }

 protected:
  IntrusiveLinkedList wait_queue;
  virtual void on_release(TCB* releasing_task) = 0;
  virtual void on_handoff(TCB* new_owner) = 0;
};
