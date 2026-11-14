# Project Report: Multithreaded Banker's Algorithm for Deadlock Avoidance

**Course:** CSE 302 – Operating Systems  
**Project:** Multithreaded Banker's Algorithm  
**Author:** Prem Sai Thotakura  
**Language / Environment:** C (C99), POSIX Threads (`pthreads`), GCC  
**Repository Files:** [bankers.c](file:///C:/Users/Asus/OneDrive/Desktop/college/SEM-5/OS%20-%20CSE%20302/OS%20Project/bankers.c) | [README.md](file:///C:/Users/Asus/OneDrive/Desktop/college/SEM-5/OS%20-%20CSE%20302/OS%20Project/README.md)

---

## 1. Executive Summary & Problem Definition

### 1.1 The Deadlock Problem
In a multiprogramming and multithreaded environment, multiple processes compete for a finite set of shared system resources (e.g., memory blocks, I/O devices, file locks). A **deadlock** occurs when two or more processes are permanently blocked because each process holds a resource that another process needs while waiting for a resource held by that other process.

A system state is susceptible to deadlock if and only if all four **Coffman conditions** hold simultaneously:
1. **Mutual Exclusion:** At least one resource is held in a non-shareable mode.
2. **Hold and Wait:** A process holds at least one resource and is waiting to acquire additional resources held by other processes.
3. **No Preemption:** Resources cannot be forcibly preempted from a process; they are released only voluntarily.
4. **Circular Wait:** A closed chain of processes exists such that each process holds at least one resource needed by the next process in the chain.

### 1.2 Deadlock Handling: Why Avoidance?
* **Prevention** eliminates deadlocks by strictly invalidating one of the four Coffman conditions (e.g., ordering resource acquisition to eliminate circular wait), but this causes severe underutilization of resources and reduced throughput.
* **Detection & Recovery** allows deadlocks to occur and resolves them via process preemption or termination, incurring significant overhead and potential loss of computation.
* **Deadlock Avoidance** (the approach used here) dynamically regulates resource allocation. Before granting any request, the operating system assesses whether granting the request will leave the system in a **Safe State**—a state where at least one execution sequence exists that allows every process to complete without deadlocking.

### 1.3 The Multithreading Concurrency Challenge
Implementing the Banker's algorithm in a concurrent environment introduces a major engineering challenge: **Race Conditions**.
Because multiple threads execute asynchronously and concurrently read and modify shared data structures (the bank's available funds, process allocations, and remaining needs), unsynchronized access would result in inconsistent state reads, race hazards, and split-brain allocation decisions where two threads are concurrently granted conflicting claims, causing the very deadlock the algorithm is designed to prevent.

---

## 2. System Architecture & Program Design

The project is structured around Dijkstra's classic Banker's Algorithm adapted for POSIX multithreading.

```
       ┌────────────────────────────────────────────────────────┐
       │                Main Thread (Orchestrator)              │
       │  - Reads N, R, Available, Max Demand, Allocation       │
       │  - Computes Need Matrix = Max Demand - Allocation      │
       │  - Verifies Initial State Safety                       │
       │  - Spawns N Process Threads                            │
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
        │  2. Check: Request <= Available ? (Wait if scarce)   │
        │  3. Tentative Allocation:                             │
        │       Available  -= Request                           │
        │       Allocation += Request                           │
        │       Need       -= Request                           │
        │  4. Run Safety Check: is_safe()                       │
        │       ├─ SAFE   → Commit Allocation & Print Sequence  │
        │       └─ UNSAFE → Atomic Rollback & Deny Request      │
        └───────────────────────────────────────────────────────┘
```

### 2.1 Core Data Structures
Shared state variables represent the bank's ledgers:
* `int avail[MAX_R]`: A vector of length $R$ indicating free instances of each resource type.
* `int max_demand[MAX_P][MAX_R]`: An $N \times R$ matrix defining the maximum resource demand each process can request during its execution.
* `int alloc[MAX_P][MAX_R]`: An $N \times R$ matrix storing resources currently allocated to each process.
* `int need[MAX_P][MAX_R]`: An $N \times R$ matrix computed as $\text{need}[i][j] = \text{max\_demand}[i][j] - \text{alloc}[i][j]$, representing remaining resource requirements.

---

## 3. Algorithmic Implementation

### 3.1 Safety Algorithm (`is_safe()`)
The safety algorithm determines whether the current system state has a safe sequence $\langle P_0, P_1, \dots, P_{n-1} \rangle$ where each process can obtain its maximum claim, execute, and release all held resources to unblock downstream processes.

1. **Working Vectors:**
   $$\text{work} = \text{avail}, \quad \text{finish}[i] = \text{false} \quad \forall i \in [0, n-1]$$
2. **Scan for Runnable Process:** Find an index $i$ such that:
   $$\text{finish}[i] == \text{false} \quad \land \quad \text{need}[i][j] \le \text{work}[j] \quad (\forall j \in [0, r-1])$$
3. **Simulate Execution:** If found:
   $$\text{work}[j] = \text{work}[j] + \text{alloc}[i][j], \quad \text{finish}[i] = \text{true}$$
   Record $P_i$ into the safe sequence array `ans[]`. Repeat step 2.
4. **Conclusion:** If all processes finish ($\text{finish}[i] == \text{true}$ for all $i$), the state is **SAFE** and the ordered safe sequence is printed; otherwise, the state is **UNSAFE**.

### 3.2 Resource-Request Algorithm (`request_resources()`)
When customer thread $P_i$ requests a vector `req[]`:
1. **Rule 1 (Integrity Check):** If $\text{req}[j] > \text{need}[i][j]$, terminate request with error (exceeded declared claim).
2. **Rule 2 (Availability Check):** If $\text{req}[j] > \text{avail}[j]$, deny request (insufficient resources; process must wait).
3. **Rule 3 (Tentative Allocation):**
   $$\text{avail} = \text{avail} - \text{req}, \quad \text{alloc}[i] = \text{alloc}[i] + \text{req}, \quad \text{need}[i] = \text{need}[i] - \text{req}$$
4. **Rule 4 (Safety Check & Atomic Rollback):**
   * If `is_safe()` returns `1`: The allocation is preserved and marked **[GRANTED]**.
   * If `is_safe()` returns `0`: The state would become unsafe. The bank performs an **atomic rollback**:
     $$\text{avail} = \text{avail} + \text{req}, \quad \text{alloc}[i] = \text{alloc}[i] - \text{req}, \quad \text{need}[i] = \text{need}[i] + \text{req}$$
     The request is marked **[DENY]**.

### 3.3 Resource-Release Algorithm (`release_resources()`)
When a process completes its task, it returns its held allocation back to the pool:
$$\text{avail} = \text{avail} + \text{rel}, \quad \text{alloc}[i] = \text{alloc}[i] - \text{rel}, \quad \text{need}[i] = \text{need}[i] + \text{rel}$$
This immediately increases `avail`, allowing other blocked or previously denied threads to satisfy their safety checks.

---

## 4. Key Operating System Features Used

### 4.1 Multithreading (`pthreads`)
* **Thread Creation (`pthread_create`):**
  Each customer/process is instantiated as an independent OS thread executing `process_thread()`. This directly mirrors real-world concurrent execution where tasks request resources asynchronously.
  ```c
  pthread_create(&tid[i], NULL, process_thread, pid);
  ```
* **Thread Termination & Synchronization (`pthread_join`):**
  The main thread acts as a coordinator, invoking `pthread_join()` on all worker threads to block until every customer process has completed execution, ensuring zero zombie threads and predictable teardown.
  ```c
  for (i = 0; i < n; i++)
      pthread_join(tid[i], NULL);
  ```

### 4.2 Race Condition Prevention (`pthread_mutex_t`)
* **Mutex Lock (`pthread_mutex_lock` / `pthread_mutex_unlock`):**
  A POSIX mutex lock (`pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER`) is used to implement strict mutual exclusion over the shared state arrays (`avail`, `alloc`, `need`, `max_demand`).
* **Atomic Critical Section:**
  The entire resource evaluation pipeline—reading current need, tentative allocation, safety algorithm traversal, and potential rollback—is executed inside a single continuous critical section. This guarantees:
  1. No dirty or interleaved state reads during simulation.
  2. No split-brain allocations where two threads simultaneously claim identical remaining resources.

---

## 5. Test Scenario & Verification

### 5.1 Test Configuration (Silberschatz Standard Benchmark)
* **Processes:** 5 ($P_0$ through $P_4$)
* **Resource Types:** 3 ($A, B, C$)
* **Initial Available:** `[3, 3, 2]`
* **Initial State Matrix:**

| Process | Max Demand | Current Allocation | Remaining Need |
| :---: | :---: | :---: | :---: |
| **$P_0$** | `[7, 5, 3]` | `[0, 1, 0]` | `[7, 4, 3]` |
| **$P_1$** | `[3, 2, 2]` | `[2, 0, 0]` | `[1, 2, 2]` |
| **$P_2$** | `[9, 0, 2]` | `[3, 0, 2]` | `[6, 0, 0]` |
| **$P_3$** | `[2, 2, 2]` | `[2, 1, 1]` | `[0, 1, 1]` |
| **$P_4$** | `[4, 3, 3]` | `[0, 0, 2]` | `[4, 3, 1]` |

### 5.2 Verification Outcomes
1. **Initial Safety:** Verified safe sequence: $\langle P_1 \to P_3 \to P_0 \to P_2 \to P_4 \rangle$.
2. **Concurrent Execution:** All 5 threads concurrently made requests. Valid requests were approved, unsafe requests were denied without corrupting state, and releasing resources unblocked waiting processes.
3. **Final Result:** All threads completed execution safely with zero deadlocks and 100% of resources reclaimed by the system.

---

## 6. Conclusion

This project demonstrates the synergy between three foundational operating systems principles:
1. **Multithreading** provides true concurrent task execution.
2. **Mutex Synchronization** guarantees thread safety and prevents race conditions on critical system state.
3. **Banker's Algorithm** offers a mathematically provable safeguard against deadlocks by dynamically granting requests only when a guaranteed safe completion sequence exists.
