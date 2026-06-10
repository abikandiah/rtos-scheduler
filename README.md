# C++ RTOS Kernel Simulator

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
| `waiting_on_` | `Resource*` | Pointer to the resource this task is blocked on — enables O(1) transitive priority inheritance traversal. Null when not resource-blocked |
| `held_resources_` | `uint32_t` | Bitmask of currently held resource IDs (bit position = `ResourceId` value). Scanned on task completion to force-release any unreleased resources |
| `next` | `TCB*` | Intrusive list forward pointer |
| `prev` | `TCB*` | Intrusive list backward pointer |
| `task` | `TaskInterface*` | Pointer to the polymorphic task implementation |

#### Task States

A TCB moves through a strict set of states. The kernel owns all transitions — a task never sets its own state.

```
         ┌────────────(done)────────────┐
         │                             ▼
INACTIVE ──►(spawn)──► READY ◄──(wake)── BLOCKED
                         │                   ▲
                         ▼(schedule)          │
                       RUNNING ──(block)──────┘
```

| State | Description |
|---|---|
| `INACTIVE` | Pre-allocated slot in the task pool, not yet assigned |
| `READY` | Enqueued in a priority list, waiting for CPU time |
| `RUNNING` | Currently executing — `execute_one_tick()` is being called |
| `BLOCKED` | Waiting on a resource (in a resource's wait queue) or sleeping (in the scheduler's BlockedTasksList). Distinguish by checking `sleep_ticks_remaining > 0` |

---

### 2. Task Pool

A fixed-size array of `TCB` structures allocated once at startup. Tasks have permanently stable memory addresses — no allocation or deallocation occurs at runtime.

An **inactive pool** tracks which slots are unassigned and available for spawning. Spawning a task means pulling a `TCB*` from the inactive pool and initializing its fields.

---

### 3. TaskInterface & KernelContext

Application tasks inherit from an abstract `TaskInterface` base class and execute as bounded state machines. Each tick, the kernel calls:

```cpp
bool execute_one_tick(KernelContext& ctx);
```

Returns `true` when the task is complete, `false` otherwise. The kernel determines the task's next state by reading `KernelContext` after the tick — the task never sets its own state.

`KernelContext` is a thin kernel API layer owned by the kernel. Tasks interact with kernel services exclusively through it — no global state, no direct kernel access. This mirrors how real RTOS tasks call kernel API functions (e.g. `vTaskDelay`, `xSemaphoreTake` in FreeRTOS). The kernel sets `ctx.current_tcb_` before each tick so the context knows which task is active.

```cpp
ctx.sleep(N);                    // Block for N ticks — always sets intent
ctx.acquire_memory(MEMORY_A);    // Request memory block — fast path if free, intent if contended
ctx.acquire_mutex(MUTEX_A);      // Acquire mutex — fast path if free, intent if contended
ctx.release(MUTEX_A);            // Release resource — immediate direct action, no intent
ctx.yield();                     // Yield remaining quantum — always sets intent
```

Context carries one `KernelIntent` per tick. Calling a second context method in the same tick is a programming error and triggers an assert. Sequential operations are handled across ticks via the task's own phase state machine.

| `KernelIntent` | Value | Kernel Action |
|---|---|---|
| `NONE` | — | Task still working — decrement quantum, round-robin if expired |
| `SLEEP` | N (ticks) | Set `sleep_ticks_remaining = N`, transition to `BLOCKED` |
| `ACQUIRE` | `ResourceId` | Resource was contended — transition to `BLOCKED`, task already enqueued on resource wait queue |
| `YIELD` | — | Reset quantum, move task to tail of its priority level |

**Acquire fast path:** if the requested resource is free, it is granted immediately within the same tick — no intent is set, no tick is wasted. Intent is only set on the contended path.

**Release:** always immediate. The kernel calls `resource.release_and_get_waiter()`, receives the next waiting `TCB*` (or null), and calls `wake_task()` to transition that task to `READY` and insert it into the ready queue. The releasing task continues its tick uninterrupted.

---

### 4. Kernel Engine

The kernel is the sole authority on task state transitions. It contains:

- **Ready Queue** — an array of `NUM_PRIORITY_LEVELS` (= 5) intrusive list heads, one per priority level. Always evaluates the highest non-empty level first for O(1) priority access.
- **BlockedTasksList** — a global intrusive list of TCBs sleeping via `ctx.sleep(N)`. Each tick the kernel decrements `sleep_ticks_remaining` for each entry and moves tasks back to the ready queue when it reaches zero. Tasks blocked on resources are tracked by those resources' own private wait queues, not here.
- **Resource Registry** — the kernel owns all resources (`MemoryPool`, `Mutex` etc.). Tasks identify resources by `ResourceId` enum and interact with them exclusively through `KernelContext`. `ResourceId` values are explicit sequential integers enabling O(1) bitmask operations.

#### Round-Robin

When a task's `quantum_remaining` hits zero, the kernel detaches it from the front of its priority list and appends it to the tail, then resets `quantum_remaining` to `DEFAULT_QUANTUM` (= 3). Tasks at the same priority level share CPU time fairly.

#### Preemption

If a task transitions from `BLOCKED` → `READY` and its `effective_priority` is higher than the currently running task, preemption occurs on the very next tick.

---

### 5. Resource Base Class

All kernel resources (`MemoryPool`, `Mutex`) inherit from an abstract `Resource` base class using the **Template Method pattern**:

```cpp
class Resource {
public:
    TCB* release_and_get_waiter(TCB* releasing_task);  // non-virtual, shared logic
    virtual TCB* get_owner() const = 0;

protected:
    virtual void on_handoff(TCB* releasing_task, TCB* new_owner) = 0;  // resource-specific
    IntrusiveLinkedList wait_queue_;
};
```

`release_and_get_waiter(TCB* releasing_task)` is non-virtual — it pops the head waiter from the shared wait queue and calls `on_handoff()` for resource-specific ownership transfer. The releasing task is passed in (sourced from `ctx.current_tcb` in the kernel) so each resource can identify what is being released:

- `Mutex::on_handoff` — clears `owner_`, ignores `releasing_task`
- `MemoryPool::on_handoff` — scans `block_owners` for `releasing_task`, frees that block, assigns it to `new_owner`

`get_owner()` is meaningful for `Mutex` only and enables O(1) transitive priority inheritance chain traversal via `TCB::waiting_on`. For `MemoryPool`, ownership is per-block — `get_owner()` returns `nullptr`. Priority inheritance therefore applies to `Mutex` only.

### 6. Memory Pool

Simulates deterministic memory allocation. Fixed block size and block count are `constexpr` — the compiler determines the memory footprint at compile time. The buffer is a 2D array (`uint8_t buffer[BLOCK_COUNT][BLOCK_SIZE]`) stored as a member — no heap allocation.

Ownership is tracked **per block**, not per pool: `TCB* block_owners[BLOCK_COUNT]`, null when free. A free bitmask enables O(1) block search. The pool owns a **private wait queue** of blocked `TCB*` pointers for tasks waiting on any free block.

When a block is freed, `release_and_get_waiter(releasing_task)` identifies the freed block via `block_owners`, assigns it to the head waiter in `on_handoff`, and returns that waiter to the kernel, which transitions it to `READY`. This bypasses any global broadcast and eliminates the "Thundering Herd" problem.

**Storage:** the `Kernel` object (and therefore all pools it owns) must be declared `static` or global — not as a local variable in `main`. This moves the buffer out of the stack into the BSS/data segment, which is necessary for large block sizes (e.g. network packet buffers) and mirrors how embedded systems manage pre-allocated memory.

---

### 7. Tick Engine

A single-threaded loop acting as the virtual CPU:

```cpp
while (sim_running && current_tick < MAX_TICKS) {
    tick();
    current_tick++;
    if (all_tasks_inactive()) sim_running = false;
}
```

Each iteration represents one logical tick. Tasks are state machines — `execute_one_tick()` performs a bounded chunk of work and immediately returns control to the loop.

---

### 8. Event Log

A ring buffer of structured tick events, capturing task state transitions, resource acquisitions, preemptions, and round-robin cycles. Built in from the start to support debugging and correctness verification.

---

## Advanced: Priority Inheritance

A classic failure in priority-based schedulers is **priority inversion**: a low-priority task holds a resource needed by a high-priority task, but gets starved by an intermediate task.

The simulator defends against this with **Priority Inheritance**. When a high-priority task blocks on a `Mutex`, the kernel elevates `effective_priority` of the holding task to match. On release, `effective_priority` is restored to `original_priority`. Priority inheritance applies to `Mutex` only — `MemoryPool` has no single owner to elevate.

Both fields live in the TCB from day one. Inheritance can chain transitively — if task C holds resource 1 and also blocks on resource 2 held by task D, the elevation propagates through the chain.

The Mars Pathfinder priority inversion bug is used in Phase 4 as a concrete validation scenario.

---

## Design Decisions

| Decision | Resolution |
|---|---|
| Dynamic allocation | Banned after startup — all structures pre-allocated |
| Intrusive lists | `next`/`prev` pointers inside TCB for O(1) insert/remove |
| State ownership | Kernel owns all transitions — tasks never set own state |
| Task-kernel interface | `bool execute_one_tick(KernelContext& ctx)` — `true` = done |
| Task→kernel communication | `KernelIntent` enum + `int value` on `KernelContext`, read after each tick |
| Acquire fast path | Resource free → granted same tick, no intent set. Contended → intent set, task blocked |
| Release | Immediate direct action — `resource.release_and_get_waiter()` → kernel calls `wake_task()` |
| Resource ownership | Kernel owns all resources. Tasks identify by `ResourceId` enum, interact via `KernelContext` |
| Resource base class | Template Method pattern — `release_and_get_waiter(TCB*)` non-virtual shared logic, `on_handoff()` pure virtual per resource. `get_owner()` meaningful for `Mutex` only |
| Priority levels | `constexpr NUM_PRIORITY_LEVELS = 5` |
| Time quantum | `constexpr DEFAULT_QUANTUM = 3`, reset on wake, counter stored in TCB |
| BLOCKED distinction | Single state — check `sleep_ticks_remaining > 0` to distinguish sleep vs. resource wait |
| Global BlockedTasksList | Retained in kernel for timer-based sleep only |
| Per-resource wait queues | Resources own their wait queues — no thundering herd |
| `waiting_on` | `Resource*` on TCB — enables O(1) transitive priority inheritance traversal (Mutex chain only) |
| `held_resources` | `uint32_t` bitmask on TCB — force-release scan on task completion |
| TCB field naming | No trailing underscores — TCB is a `struct` (public data). Trailing underscores reserved for private class members |
| `Kernel` storage | Declared `static` or global — keeps large pool buffers off the stack in BSS/data segment |
| Priority inheritance scope | `Mutex` only — `MemoryPool` has no single owner, inheritance does not apply |
| `MemoryPool` ownership | Per-block `TCB* block_owners[BLOCK_COUNT]`, not pool-level. Pool-level `get_owner()` returns `nullptr` |
| Double context call | Assert — one `KernelIntent` per tick enforced at boundary |
| Re-acquire guard | No-op if task already owns resource |
| Pool sizes | All `constexpr` at compile time |
| Termination | All tasks `INACTIVE` or `MAX_TICKS` safety cap reached |

---

## Project Roadmap

### Phase 1: Foundations & Static Layout
- Implement the `TCB` struct with all fields including `waiting_on_` and `held_resources_`
- Define the `TaskState` and `ResourceId` enums
- Author `TaskInterface` abstract base class and `KernelContext` API layer
- Author the abstract `Resource` base class
- Author the `IntrusiveLinkedList` class managing raw `TCB*` pointer manipulation
- Build the `TaskPool` with inactive slot tracking
- Initialize the structural memory footprint of the `Kernel`

### Phase 2: Core Tick Loop & Scheduling
- Construct the main `while(sim_running)` tick engine
- Build the priority-level ready queue array inside the `Kernel`
- Implement round-robin decrement and preemption logic
- Implement `BlockedTasksList` with per-tick sleep countdown
- Wire up `KernelContext` intent dispatch and `bool` return handling

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
