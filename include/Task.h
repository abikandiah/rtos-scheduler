#pragma once

class KernelContext;

class Task {
  virtual ~Task() = default;
  virtual const char* name() const = 0;
  virtual bool execute_one_tick(KernelContext& ctx) = 0;
};
