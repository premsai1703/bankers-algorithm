# Banker's Algorithm – Multithreaded Deadlock Avoidance

> **CSE 302 – Operating Systems Project**  
> **Author:** Prem Sai Thotakura  
> **Language:** C (C99) | **Concurrency:** POSIX Threads (`pthreads`) | **Synchronization:** Mutex (`pthread_mutex_t`)

---

## 1. Project Overview

This program implements **Dijkstra's Banker's Algorithm** within a multithreaded simulation in C. Multiple customer processes run concurrently as independent POSIX threads, competing for a shared pool of diverse resource types. 

The bank grants a resource request **only if** the resulting state remains **safe** — mathematically guaranteeing that no deadlock can occur. If granting a request would transition the system into an unsafe state, the request is denied, and the system executes an immediate atomic rollback.

All parameters (process count, resource count, available instances, maximum demand matrix, and current allocation matrix) are collected interactively at runtime.

---

## 2. Three Core OS Concepts Demonstrated

| Concept | Implementation in Code |
|---|---|
| **Multithreading** | `pthread_create` / `pthread_join` — each process executes as an independent, concurrent worker thread |
| **Race Condition Prevention** | `pthread_mutex_t lock` guards every critical section reading or modifying shared state |
| **Deadlock Avoidance** | Dijkstra's Safety Algorithm executes before every allocation decision, with atomic rollback on rejection |

---

## 3. Theoretical Background

### 3.1 The Deadlock Problem
In a multiprogramming operating system, processes dynamically request, hold, and release finite resources. A **deadlock** occurs when two or more processes are permanently blocked because each process holds a resource needed by another, and none can proceed without acquiring more.

A system state is vulnerable to deadlock if and only if all four **Coffman conditions** hold simultaneously:
1. **Mutual Exclusion:** At least one resource must be held in a non-shareable mode.
2. **Hold and Wait:** A process holds at least one resource while waiting to acquire additional resources held by other processes.
3. **No Preemption:** Resources cannot be forcibly preempted; they are released only voluntarily by the holding process.
4. **Circular Wait:** A closed chain of processes exists where each process holds resources needed by the next process in the cycle.

### 3.2 Deadlock Handling: Why Avoidance?
* **Deadlock Prevention:** Strictly eliminates one of the four Coffman conditions (e.g., ordering resource acquisition), but severely degrades resource utilization and system throughput.
* **Deadlock Detection & Recovery:** Allows deadlocks to happen and resolves them through preemption or process termination, incurring overhead and loss of computation.
* **Deadlock Avoidance (Banker's Algorithm):** Dynamically regulates resource allocation. Before granting any request, the OS verifies whether the allocation maintains a **Safe State** — a state where at least one ordering of process execution (**Safe Sequence**) exists that allows all processes to finish without deadlocking.

### 3.3 The Concurrency Challenge (Race Conditions)
In a multithreaded environment, multiple process threads run asynchronously and access the bank's shared ledger (`avail`, `alloc`, `need`). Unsynchronized concurrent access causes data races, dirty reads, and split-brain decisions where two threads are concurrently granted conflicting claims, causing the very deadlock the algorithm is intended to avoid.

---

## 4. System Architecture & Data Structures

```
       ┌────────────────────────────────────────────────────────┐
       │                Main Thread (Orchestrator)              │
       │  - Collects N, R, Available, Max Demand, Allocation    │
       │  - Computes Need Matrix = Max Demand - Allocation      │
       │  - Verifies Initial System Safety                      │
       │  - Spawns N Concurrent Process Threads                 │
       └──────────────────────────┬─────────────────────────────┘
                                  │ pthread_create()
         ┌────────────────────────┼────────────────────────┐
         ▼                        ▼                        ▼
  ┌───────────────┐        ┌───────────────┐        ┌───────────────┐
  │Process Thread0│        │Process Thread1│        │Process ThreadN│
  └───────┬───────┘        └───────┬───────┘        └───────┬───────┘
          │                        │                        │
          └────────────────────────┼────────────────────────┘
                                   │
                                   ▼
        ┌───────────────────────────────────────────────────────┐
        │        Critical Section (Guarded by bank_mutex)       │
        │                                                       │
        │  1. Check: Request <= Need ? (Exceed max claim?)      │
        │  2. Check: Request <= Available ? (Wait if scarce)    │
        │  3. Tentative Allocation:                             │
        │       Available  -= Request                           │
        │       Allocation += Request                           │
        │       Need       -= Request                           │
        │  4. Run Safety Check: is_safe()                       │
        │       ├─ SAFE   → Commit Allocation & Print Sequence  │
        │       └─ UNSAFE → Atomic Rollback & Deny Request      │
        └───────────────────────────────────────────────────────┘
```

### Shared State Variables
* `avail[r]`: A vector of length $R$ recording currently free instances of each resource type.
* `max_demand[n][r]`: An $N \times R$ matrix declaring the maximum demand each process may ever request.
* `alloc[n][r]`: An $N \times R$ matrix recording the instances of each resource currently allocated to each process.
* `need[n][r]`: An $N \times R$ matrix representing remaining resource needs, computed as:
  $$\text{need}[i][j] = \text{max\_demand}[i][j] - \text{alloc}[i][j]$$
* `pthread_mutex_t lock`: A POSIX mutual exclusion lock guarding all reads and writes across the four shared structures.

---

## 5. Algorithmic Implementation

### 5.1 Safety Algorithm (`is_safe()`)
The safety routine determines whether the system can satisfy all processes without deadlocking:
1. Initialize working copies: $\text{work} = \text{avail}$ and finish status $\text{f}[i] = 0 \quad (\forall i \in [0, n-1])$.
2. Find an unfinished process $P_i$ ($\text{f}[i] == 0$) whose remaining need can be satisfied by current working capacity:
   $$\text{need}[i][j] \le \text{work}[j] \quad (\forall j \in [0, r-1])$$
3. If found, simulate $P_i$ completing and returning all its held resources:
   $$\text{work}[j] = \text{work}[j] + \text{alloc}[i][j], \quad \text{f}[i] = 1, \quad \text{ans}[\text{ind}++] = i$$
4. Repeat steps 2 and 3 until no further processes can proceed.
5. If all processes finish ($\text{ind} == n$), the state is **SAFE** and the ordered safe sequence is displayed. If any process remains unfinished, the state is **UNSAFE**.

### 5.2 Resource-Request Algorithm (`request_resources()`)
When process $P_i$ submits a request vector `req[]`:
1. **Integrity Validation:** If $\text{req}[j] > \text{need}[i][j]$, deny request with error (process exceeded declared maximum claim).
2. **Availability Validation:** If $\text{req}[j] > \text{avail}[j]$, deny request (insufficient free resources; process must wait).
3. **Tentative Allocation:** The banker provisionally adjusts state:
   $$\text{avail} = \text{avail} - \text{req}, \quad \text{alloc}[i] = \text{alloc}[i] + \text{req}, \quad \text{need}[i] = \text{need}[i] - \text{req}$$
4. **Safety Check & Rollback:**
   * If `is_safe()` returns `1`: The allocation is confirmed and committed as **[GRANTED]**.
   * If `is_safe()` returns `0`: The state would become unsafe. The banker performs an immediate **atomic rollback**:
     $$\text{avail} = \text{avail} + \text{req}, \quad \text{alloc}[i] = \text{alloc}[i] - \text{req}, \quad \text{need}[i] = \text{need}[i] + \text{req}$$
     The request is denied as **[DENIED]**.

### 5.3 Resource-Release Algorithm (`release_resources()`)
Upon task completion (`sleep(1)`), each process returns all its allocated units back to the pool:
$$\text{avail} = \text{avail} + \text{rel}, \quad \text{alloc}[i] = \text{alloc}[i] - \text{rel}, \quad \text{need}[i] = \text{need}[i] + \text{rel}$$
This immediately increases available resources, allowing blocked processes to pass future safety evaluations.

---

## 6. Concurrency & Synchronization Design

### Why a Single Global Mutex?
The tentative allocation (Step 3), safety check (Step 4), and potential rollback must execute **atomically**. 
If two threads were permitted to evaluate the safety algorithm concurrently:
1. Both threads could inspect partially modified state simultaneously.
2. Both could receive a false "SAFE" result.
3. Both would commit their allocations, inadvertently driving the system into an irrecoverable deadlock.

Wrapping the entire evaluation and rollback sequence inside `pthread_mutex_lock(&lock)` and `pthread_mutex_unlock(&lock)` guarantees sequential consistency, data race elimination, and absolute deadlock avoidance.

---

## 7. Compilation & Execution

### Prerequisites
* GCC compiler with POSIX Threads support (`-pthread`)
* Supported platforms: Linux, macOS, WSL, or Windows (MSYS2 / MinGW-w64)

### Build
```bash
gcc -Wall -pthread -o bankers bankers.c
```

### Run
```bash
./bankers
```

---

## 8. Benchmark Scenario & Verification

### Test Configuration (Silberschatz Standard Benchmark)
* **Processes:** 5 ($P_0$ through $P_4$)
* **Resource Types:** 3 ($A, B, C$)
* **Initial Available:** `[3, 3, 2]`

| Process | Max Demand | Current Allocation | Remaining Need |
| :---: | :---: | :---: | :---: |
| **$P_0$** | `[7, 5, 3]` | `[0, 1, 0]` | `[7, 4, 3]` |
| **$P_1$** | `[3, 2, 2]` | `[2, 0, 0]` | `[1, 2, 2]` |
| **$P_2$** | `[9, 0, 2]` | `[3, 0, 2]` | `[6, 0, 0]` |
| **$P_3$** | `[2, 2, 2]` | `[2, 1, 1]` | `[0, 1, 1]` |
| **$P_4$** | `[4, 3, 3]` | `[0, 0, 2]` | `[4, 3, 1]` |

### Example Input
```
Enter number of processes: 5
Enter number of resources: 3

Enter available resources (3 values): 3 3 2

Enter MAX demand matrix (5 processes x 3 resources):
  P0 max: 7 5 3
  P1 max: 3 2 2
  P2 max: 9 0 2
  P3 max: 2 2 2
  P4 max: 4 3 3

Enter ALLOCATION matrix (5 processes x 3 resources):
  P0 allocation: 0 1 0
  P1 allocation: 2 0 0
  P2 allocation: 3 0 2
  P3 allocation: 2 1 1
  P4 allocation: 0 0 2
```

### Program Execution Output
```
=== Banker's Algorithm ===
Processes: 5  |  Resources: 3

Proc    Max / Allocation / Need
------  ------------------------------------------
P0      Max:[7 5 3]  Alloc:[0 1 0]  Need:[7 4 3]
P1      Max:[3 2 2]  Alloc:[2 0 0]  Need:[1 2 2]
P2      Max:[9 0 2]  Alloc:[3 0 2]  Need:[6 0 0]
P3      Max:[2 2 2]  Alloc:[2 1 1]  Need:[0 1 1]
P4      Max:[4 3 3]  Alloc:[0 0 2]  Need:[4 3 1]

Available: [3 3 2]

--- Initial Safety Check ---
  [SAFE] Safe sequence: P1 -> P3 -> P0 -> P2 -> P4

--- Spawning Process Threads ---

P1 requesting: [1 2 2]
  [SAFE] Safe sequence: P3 -> P0 -> P2 -> P4 -> P1
  [GRANTED] P1 request approved.

P3 requesting: [0 1 1]
  [SAFE] Safe sequence: P0 -> P2 -> P4 -> P1 -> P3
  [GRANTED] P3 request approved.

P1 released:   [3 2 2]
P3 released:   [2 2 2]
...
=== All processes finished. No deadlock. ===
```

---

## 9. Repository Structure

```
bankers-algorithm/
├── .gitignore     – Excludes build artifacts and temporary files
├── bankers.c      – Full multithreaded implementation in C
└── README.md      – Complete documentation, theoretical foundations, and execution guide
```
