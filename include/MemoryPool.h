#pragma once

#include "Resource.h"
#include "types.h"

constexpr uint8_t BLOCK_SIZE = 64;
constexpr uint8_t BLOCK_COUNT = 8;

class MemoryPool : public Resource {
  uint8_t buffer[BLOCK_COUNT][BLOCK_SIZE];
  TCB* block_owners[BLOCK_COUNT];

 public:
  virtual TCB* get_owner() { return nullptr; };
  virtual TCB* release_and_get_waiter() {};
};