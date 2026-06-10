#pragma once

#include "types.h"

class Resource;
class Task;

struct TCB {
  uint8_t original_priority;
  uint8_t effective_priority;

  uint8_t quantum_remaining;
  uint32_t sleep_ticks_remaining;
  uint32_t held_resources;

  Task* task;
  TaskState state;
  Resource* waiting_on;

  TCB* next;
  TCB* prev;

  TCB(Task* t)
      : original_priority(0),
        effective_priority(0),
        quantum_remaining(0),
        sleep_ticks_remaining(0),
        held_resources(0),
        task(t),
        state(TaskState::INACTIVE),
        waiting_on(nullptr),
        next(nullptr),
        prev(nullptr) {}

  TCB(const TCB&) = delete;
  TCB& operator=(const TCB&) = delete;
  TCB(TCB&&) = delete;
  TCB& operator=(TCB&&) = delete;
};
