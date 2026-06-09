# C++ RTOS Scheduler Simulator - Project Instructions

This document provides essential context and instructions for working on the `rtos-scheduler` project.

## Project Overview

A deterministic, software-based simulator designed to model Real-Time Operating System (RTOS) task scheduling, resource management, and core kernel mechanics. It executes within a **single-threaded deterministic tick engine**, avoiding host OS concurrency primitives to achieve true O(1) predictability.

### Core Architecture
- **Zero-Dynamic Allocation:** No heap allocation occurs during simulation runs. All structures (TCBs, Task Pools, etc.) are pre-allocated.
- **Intrusive Data Structures:** Tasks are managed using raw pointers within TCBs (`next`/`prev`) to form intrusive linked lists, enabling O(1) insertion and removal.
- **State Machine Tasks:** Tasks implement `TaskInterface` and are executed as bounded state machines via `execute_one_tick(KernelContext& ctx)`.
- **Deterministic Tick Engine:** A single loop drives the simulation, incrementing a virtual clock and allowing tasks to perform work in discrete chunks.

## Technical Stack
- **Language:** C++17
- **Build System:** CMake 3.15+
- **Compiler Options:** `-Wall -Wextra -Wpedantic` (configured in `CMakeLists.txt`)

## Building and Running

### Build Commands
```bash
mkdir -p build
cd build
cmake ..
make
```

### Running the Simulator
```bash
./rtos_scheduler
```

## Development Conventions

### Coding Standards
- **Memory Management:** Strictly avoid `new`, `delete`, `malloc`, and `free` after the simulation starts. Use pre-allocated pools.
- **Data Structures:** Use intrusive linked lists for managing task queues (Ready, Blocked, Wait queues).
- **Kernel/Task Separation:** Tasks must interact with the kernel only through the `KernelContext` API. No global state or direct scheduler access from tasks.
- **Error Handling:** Use return codes or status enums (e.g., `TickResult`, `TaskState`). Avoid exceptions if they compromise determinism or require dynamic allocation.

### Architectural Rules
- **State Ownership:** The kernel (Scheduler) owns all task state transitions. Tasks never set their own state directly.
- **Priority Levels:** Fixed at 5 levels (0-4).
- **Round-Robin:** Default quantum is 3 ticks.
- **Resource Management:** Resources (like `MemoryPool`) own their own private wait queues to prevent the "Thundering Herd" problem.

## Roadmap & Progress

The project is currently in **Phase 1: Foundations & Static Layout**.

1.  **Phase 1: Foundations & Static Layout** (Current)
    - Implement `TCB`, `TaskState`, `TickResult`.
    - Build `TaskInterface` and `KernelContext`.
    - Implement `IntrusiveLinkedList`.
    - Set up `TaskPool` and `Scheduler` skeleton.
2.  **Phase 2: Core Tick Loop & Scheduling**
3.  **Phase 3: Resource Blocking Mechanics**
4.  **Phase 4: Validation & Edge Cases**

## Key Files
- `README.md`: Comprehensive architectural documentation.
- `CMakeLists.txt`: Build configuration.
- `src/main.cpp`: Entry point (currently empty).
- `include/`: Destination for header files.
