# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Important: Role Constraint

This is a manually coded project. Claude's role here is **discussion and code review only** — do not write, generate, or suggest code changes. Engage with architecture questions, explain concepts, and review code for correctness, but leave all implementation to the human author.

## Build & Run

```bash
mkdir -p build && cd build && cmake .. && make
./build/rtos_scheduler
```

Headers go in `include/`, sources in `src/`. CMake uses `GLOB_RECURSE` on `src/*.cpp`, so new source files are picked up automatically on the next `cmake ..`.

## Architecture

A single-threaded deterministic tick engine simulating an RTOS kernel. No host OS concurrency primitives are used — all scheduling, blocking, and resource management is implemented from scratch.

**Zero-dynamic-allocation invariant:** `new`/`delete`/`malloc`/`free` are banned after startup. All structures live in pre-allocated pools with stable addresses.

### Key design points

- **TCB (Task Control Block)** — the complete task snapshot. Contains `state`, `original_priority`, `effective_priority`, `quantum_remaining`, `sleep_ticks_remaining`, `waiting_on_` (`Resource*`), `held_resources_` (`uint32_t` bitmask), and intrusive list pointers (`next`/`prev`). No side-tables anywhere in the kernel.
- **Intrusive linked lists** — `next`/`prev` live inside each TCB, enabling O(1) insert/remove without allocation.
- **State ownership** — the kernel is the sole authority on task state transitions. Tasks never set their own state.
- **`TaskInterface` / `KernelContext`** — tasks implement `bool execute_one_tick(KernelContext& ctx)`. Returns `true` when complete, `false` otherwise. All kernel calls go through `KernelContext`; tasks have no direct kernel access. Kernel reads context after every tick to determine state transitions.
- **`KernelIntent`** — context carries one intent + `int value` per tick (`NONE`, `SLEEP`, `ACQUIRE`, `YIELD`). Double context calls within a single tick trigger an assert.
- **Acquire fast path** — if a resource is free, it is granted immediately within the same tick and no intent is set. Intent is only set on the contended path.
- **Release** — immediate direct action. Kernel calls `resource.release_and_get_waiter()` and wakes the returned TCB directly. No intent, no deferred processing.
- **Resource ownership** — kernel owns all resources. Tasks identify them by `ResourceId` enum and access them exclusively through `KernelContext`.
- **Abstract `Resource` base class** — `get_owner()` and `release_and_get_waiter()` enable polymorphic handling across `MemoryPool`, `Mutex`, etc.
- **Ready queue** — array of 5 intrusive list heads, one per priority level. Highest non-empty level runs first (O(1)).
- **`BlockedTasksList`** — kernel-owned list for timer-sleep only. Resource-blocked tasks live in the resource's own private wait queue.
- **`BLOCKED` state disambiguation** — single enum value; check `sleep_ticks_remaining > 0` to distinguish sleep from resource wait.
- **`waiting_on_`** — `Resource*` on TCB set by the resource when enqueuing a task. Enables O(1) transitive priority inheritance chain traversal. Cleared by the resource on handoff.
- **`held_resources_`** — `uint32_t` bitmask on TCB. Scanned on task completion for force-release of any unreleased resources.
- **Priority inheritance** — `effective_priority` is elevated when a higher-priority task blocks on a resource held by a lower-priority task; restored on release. Chains transitively via `waiting_on_`.
- **Per-resource wait queues** — resources own their wait queues and hand off directly to the head waiter on release, preventing thundering herd.

### Roadmap phases

1. **Phase 1** — TCB, enums, `TaskInterface`, `KernelContext`, `KernelIntent`, abstract `Resource`, `IntrusiveLinkedList`, `TaskPool`, Kernel skeleton
2. **Phase 2** — Tick engine, ready queue, round-robin (quantum = 3), sleep countdown, `KernelIntent` dispatch
3. **Phase 3** — `MemoryPool`, resource wait queues, `BLOCKED`→`READY` transitions, event log ring buffer
4. **Phase 4** — `Mutex`, Mars Pathfinder priority inversion scenario, transitive priority inheritance

### Constants

| Constant | Value |
|---|---|
| `NUM_PRIORITY_LEVELS` | 5 |
| `DEFAULT_QUANTUM` | 3 |
