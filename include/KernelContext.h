#pragma once

#include "types.h"

class KernelContext {
  KernelIntent intent;

 public:
  void sleep(int count);
  void acquire_memory();
  void acquire_mutex();
  void release();
  void yield();
};