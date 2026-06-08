# C++ RTOS Scheduler Simulator

A deterministic, software-based simulator designed to model Real-Time Operating System (RTOS) task scheduling, resource management, and core kernel mechanics without relying on host OS concurrency primitives.

This project serves as a deep dive into low-level systems programming, exploring concepts utilized by modern embedded kernels like FreeRTOS and the Linux scheduler.

---

## Architecture Overview

Unlike high-level concurrent software that leverages host OS threads, `std::mutex`, or `std::condition_variable`, this simulator executes entirely inside a **single-threaded deterministic tick engine**. OS abstractions are built manually from scratch to achieve true O(1) real-time execution predictability.

The simulator operates on a **zero-dynamic-allocation model** after startup. All tasks are pre-allocated into a fixed pool. Ownership across scheduling states is managed exclusively by manipulating raw pointers — no heap allocation occurs during the simulation run.

---

## Core Components

### 1. Task Control Block (TCB)

The TCB is the complete snapshot of a task's existence. Every field needed to describe where a task is and what it is waiting for lives here. The scheduler never maintains side-tables.

| Field | Type | Purpose |
|---|---|---|
| `id` | `int` | Unique task identifier |
| `state` | `TaskState` | Current lifecycle state |
| `original_priority` | `int` | Base priority (0–4) |
| `effective_priority` | `int` | Elevated during priority inheritance, otherwise equals `original_priority` |
| `quantum_remaining` | `int` | Ticks remaining before round-robin preemption |
| `sleep_ticks_remaining` | `int` | Ticks remaining before waking from sleep |
| `next` | `TCB*` | Intrusive list forward pointer |
| `prev` | `TCB*` | Intrusive list backward pointer |
| `task` | `TaskInterface*` | Pointer to the polymorphic task implementation |

#### Task States

A TCB moves through a strict set of states. The kernel owns all transitions — a task never sets its own state.

```
INACTIVE ──►(spawn)──► READY ◄──(wake)── BLOCKED
                         │                   ▲
                         ▼(schedule)          │
                       RUNNING ──(block)──────┘
                         │
                         ▼(done)
                      TERMINATED
```

| State | Description |
|---|---|
| `INACTIVE` | Pre-allocated slot in the task pool, not yet assigned |
| `READY` | Enqueued in a priority list, waiting for CPU time |
| `RUNNING` | Currently executing — `execute_one_tick()` is being called |
| `BLOCKED` | Waiting on a resource (in a resource's wait queue) or sleeping (in the scheduler's BlockedTasksList). Distinguish by checking `sleep_ticks_remaining > 0` |
| `TERMINATED` | Execution complete |

---

### 2. Task Pool

A fixed-size array of `TCB` structures allocated once at startup. Tasks have permanently stable memory addresses — no allocation or deallocation occurs at runtime.

An **inactive pool** tracks which slots are unassigned and available for spawning. Spawning a task means pulling a `TCB*` from the inactive pool and initializing its fields.

---

### 3. TaskInterface & KernelContext

Application tasks inherit from an abstract `TaskInterface` base class and execute as bounded state machines. Each tick, the scheduler calls:

```cpp
TickResult execute_one_tick(KernelContext& ctx);
```

`KernelContext` is a thin kernel API layer owned by the scheduler. Tasks interact with kernel services exclusively through it — no global state, no direct scheduler access. This mirrors how real RTOS tasks call kernel API functions (e.g. `vTaskDelay`, `xSemaphoreTake` in FreeRTOS).

```cpp
// Example kernel services exposed via KernelContext
ctx.sleep(N);           // Block for N ticks
ctx.acquire(pool);      // Request a memory block
ctx.acquire(mutex);     // Acquire a mutex
```

`execute_one_tick()` returns a `TickResult` enum expressing the task's intent. The kernel acts on it:

| Return Value | Meaning | Kernel Action |
|---|---|---|
| `CONTINUE` | Still working | Decrement quantum, round-robin if expired |
| `DONE` | Execution complete | Set state → `TERMINATED` |
| `BLOCKED` | Waiting on resource or sleep | Remove from ready list, add to appropriate wait queue |

---

### 4. Scheduler Engine

The scheduler is the traffic controller. It contains:

- **Ready Queue** — an array of `NUM_PRIORITY_LEVELS` (= 5) intrusive list heads, one per priority level. Always evaluates the highest non-empty level first for O(1) priority access.
- **BlockedTasksList** — a global intrusive list of TCBs sleeping via `ctx.sleep(N)`. Each tick the scheduler decrements `sleep_ticks_remaining` for each entry and moves tasks back to the ready queue when it reaches zero. Tasks blocked on resources are tracked by those resources' own private wait queues, not here.

#### Round-Robin

When a task's `quantum_remaining` hits zero, the scheduler detaches it from the front of its priority list and appends it to the tail, then resets `quantum_remaining` to `DEFAULT_QUANTUM` (= 3). Tasks at the same priority level share CPU time fairly.

#### Preemption

If a task transitions from `BLOCKED` → `READY` and its `effective_priority` is higher than the currently running task, preemption occurs on the very next tick.

---

### 5. Memory Pool

Simulates deterministic memory allocation. Manages a fixed count of block units and owns a **private wait queue** of blocked `TCB*` pointers.

When a block is freed, the pool bypasses any global broadcast and directly hands the resource to the TCB at the front of its wait queue, transitioning that task's state to `READY` and signalling the scheduler. This eliminates the "Thundering Herd" problem.

---

### 6. Tick Engine

A single-threaded loop acting as the virtual CPU:

```cpp
while (sim_running && current_tick < MAX_TICKS) {
    tick();
    current_tick++;
    if (all_tasks_terminated()) sim_running = false;
}
```

Each iteration represents one logical tick. Tasks are state machines — `execute_one_tick()` performs a bounded chunk of work and immediately returns control to the loop.

---

### 7. Event Log

A ring buffer of structured tick events, capturing task state transitions, resource acquisitions, preemptions, and round-robin cycles. Built in from the start to support debugging and correctness verification.

---

## Advanced: Priority Inheritance

A classic failure in priority-based schedulers is **priority inversion**: a low-priority task holds a resource needed by a high-priority task, but gets starved by an intermediate task.

The simulator defends against this with **Priority Inheritance**. When a high-priority task blocks on a resource, the scheduler elevates `effective_priority` of the holding task to match. On release, `effective_priority` is restored to `original_priority`.

Both fields live in the TCB from day one. Inheritance can chain transitively — if task C holds resource 1 and also blocks on resource 2 held by task D, the elevation propagates through the chain.

The Mars Pathfinder priority inversion bug is used in Phase 4 as a concrete validation scenario.

---

## Design Decisions

| Decision | Resolution |
|---|---|
| Dynamic allocation | Banned after startup — all structures pre-allocated |
| Intrusive lists | `next`/`prev` pointers inside TCB for O(1) insert/remove |
| State ownership | Kernel owns all transitions — tasks never set own state |
| Task-kernel interface | `execute_one_tick(KernelContext& ctx)` |
| `TickResult` | Enum: `CONTINUE`, `DONE`, `BLOCKED` |
| Priority levels | `constexpr NUM_PRIORITY_LEVELS = 5` |
| Time quantum | `constexpr DEFAULT_QUANTUM = 3`, counter stored in TCB |
| BLOCKED distinction | Single state — check `sleep_ticks_remaining > 0` to distinguish sleep vs. resource wait |
| Global BlockedTasksList | Retained in scheduler for timer-based sleep only |
| Per-resource wait queues | Resources own their wait queues — no thundering herd |
| Pool sizes | All `constexpr` at compile time |
| Termination | All tasks `TERMINATED` or `MAX_TICKS` safety cap reached |

---

## Project Roadmap

### Phase 1: Foundations & Static Layout
- Implement the `TCB` struct with all fields
- Define the `TaskState` enum and `TickResult` enum
- Author `TaskInterface` abstract base class and `KernelContext` API layer
- Author the `IntrusiveLinkedList` class managing raw `TCB*` pointer manipulation
- Build the `TaskPool` with inactive slot tracking
- Initialize the structural memory footprint of the `Scheduler`

### Phase 2: Core Tick Loop & Scheduling
- Construct the main `while(sim_running)` tick engine
- Build the priority-level ready queue array inside the `Scheduler`
- Implement round-robin decrement and preemption logic
- Implement `BlockedTasksList` with per-tick sleep countdown
- Wire up the `KernelContext` and `TickResult` dispatch

### Phase 3: Resource Blocking Mechanics
- Design a deterministic `MemoryPool` with static block tracking
- Implement private wait queues within `MemoryPool`
- Implement `BLOCKED` → `READY` transitions on resource release
- Build the structured event log ring buffer
- Produce logging output illustrating exact pointer moves across ticks

### Phase 4: Validation & Edge Cases
- Implement a custom `Mutex` primitive
- Write a test reproducing the Mars Pathfinder priority inversion bug
- Implement priority inheritance with transitive chain propagation
- Verify system recovery behavior under stress

---

## Technical Stack

- **Language:** C++17
- **Paradigms:** OOP, Polymorphism, Intrusive Data Structures, Discrete Event Simulation
- **Tooling:** CMake 3.15+, GCC / Clang / MSVC
- **Allocation model:** Zero dynamic allocation after startup
- **Concurrency model:** Single-threaded deterministic tick engine
