# Banker's Algorithm – Multithreaded Deadlock Avoidance

> **CSE 302 – Operating Systems Project**
> Language: C | Concurrency: POSIX Threads | Synchronisation: Mutex

---

## Overview

This program implements **Dijkstra's Banker's Algorithm** using a multithreaded simulation. Multiple process threads concurrently request and release resources from a shared bank. The banker grants a request **only if** the resulting state is **safe** — meaning no deadlock can occur.

All inputs (number of processes, resources, allocation matrix, max demand matrix) are entered **at runtime**.

---

## Three OS Concepts Demonstrated

| Concept | Implementation |
|---|---|
| **Multithreading** | One `pthread` per process — all run concurrently |
| **Race condition prevention** | `pthread_mutex_t lock` guards every read/write of shared arrays |
| **Deadlock avoidance** | Safety algorithm runs before every grant; rolls back if unsafe |

---

## File Structure

```
OS Project/
└── bankers.c     — entire program (single file)
```

---

## How to Compile & Run

### On Linux / WSL

```bash
gcc -Wall -pthread -o bankers bankers.c
./bankers
```

---

## Input Format

The program prompts for all values at runtime in this order:

```
1. Number of processes
2. Number of resources
3. Available resources    (space-separated, one line)
4. Max demand matrix      (one row per process)
5. Allocation matrix      (one row per process)
```

### Example Input (5 processes, 3 resources)

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

### Example Output

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
  [GRANTED] P3 request approved.
...

=== All processes finished. No deadlock. ===
```

---

## Algorithm

### Arrays

| Array | Meaning |
|---|---|
| `avail[r]` | Free instances of each resource |
| `max_demand[n][r]` | Maximum each process will ever request |
| `alloc[n][r]` | Currently allocated to each process |
| `need[n][r]` | Remaining need = `max - alloc` |

### Safety Algorithm (`is_safe`)

```
work[]  = copy of avail
finish[] = { false ... }

repeat:
  find process i where finish[i] == false AND need[i] <= work
  if found:
    work += alloc[i]        // simulate i completing
    finish[i] = true
  else:
    break

if all finish[i] == true → SAFE
else → UNSAFE
```

### Request Algorithm (`request_resources`)

```
Step 1: request <= need?      → else DENY (exceeds max claim)
Step 2: request <= avail?     → else DENY (must wait)
Step 3: Tentatively allocate
Step 4: Run safety check
          safe   → GRANT (keep allocation)
          unsafe → DENY  (rollback allocation)
```

---

## Why a Single Mutex?

The tentative allocation (Step 3) and safety check (Step 4) must happen **atomically** — no other thread can modify the shared arrays between them. A single `pthread_mutex_t lock` covering both steps prevents any thread from sneaking in with a corrupted state view.
