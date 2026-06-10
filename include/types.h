#pragma once

#include <cstdint>

enum class TaskState : uint8_t { INACTIVE, READY, RUNNING, BLOCKED };

enum class KernelIntent : uint8_t { NONE, ACQUIRE, SLEEP, YIELD };

enum class ResourceId : uint8_t { MUTEX_A = 0, MUTEX_B = 1, MEMORY_POOL = 2 };
