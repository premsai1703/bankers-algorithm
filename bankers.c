/*
 * Banker's Algorithm - Multithreaded Implementation
 * CSE 302 - Operating Systems
 *
 * Compile : gcc -Wall -pthread -o bankers bankers.c
 * Run     : ./bankers
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_P 20   /* upper limit on processes */
#define MAX_R 10   /* upper limit on resources */

/* ── Runtime dimensions (entered by user) ── */
int n, r;

/* ── Shared state ── */
int alloc[MAX_P][MAX_R];
int max_demand[MAX_P][MAX_R];
int avail[MAX_R];
int need[MAX_P][MAX_R];

/* ── Mutex to prevent race conditions ── */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* ══════════════════════════════════════════════════
 *  SAFETY CHECK
 *  Returns 1 if system is in safe state, else 0.
 *  Caller must hold the mutex before calling this.
 * ══════════════════════════════════════════════════ */
int is_safe()
{
    int work[MAX_R], f[MAX_P], ans[MAX_P], ind = 0;
    int i, j, k, flag;

    for (j = 0; j < r; j++) work[j] = avail[j];
    for (i = 0; i < n; i++) f[i] = 0;

    for (k = 0; k < n; k++) {
        for (i = 0; i < n; i++) {
            if (f[i] == 0) {
                flag = 0;
                for (j = 0; j < r; j++) {
                    if (need[i][j] > work[j]) {
                        flag = 1;
                        break;
                    }
                }
                if (flag == 0) {
                    for (j = 0; j < r; j++)
                        work[j] += alloc[i][j];
                    ans[ind++] = i;
                    f[i] = 1;
                }
            }
        }
    }

    if (ind == n) {
        printf("  [SAFE] Safe sequence: ");
        for (i = 0; i < n - 1; i++) printf("P%d -> ", ans[i]);
        printf("P%d\n", ans[n - 1]);
        return 1;
    }
    return 0;
}

/* ══════════════════════════════════════════════════
 *  REQUEST RESOURCES
 *  Returns 1 if granted, 0 if denied.
 * ══════════════════════════════════════════════════ */
int request_resources(int pid, int req[])
{
    int j;

    pthread_mutex_lock(&lock);

    printf("\nP%d requesting: [", pid);
    for (j = 0; j < r; j++) printf("%d%s", req[j], j < r-1 ? " " : "");
    printf("]\n");

    /* Step 1: request must not exceed need */
    for (j = 0; j < r; j++) {
        if (req[j] > need[pid][j]) {
            printf("  [DENY] P%d exceeded max claim.\n", pid);
            pthread_mutex_unlock(&lock);
            return 0;
        }
    }

    /* Step 2: request must not exceed available */
    for (j = 0; j < r; j++) {
        if (req[j] > avail[j]) {
            printf("  [DENY] P%d must wait, resources unavailable.\n", pid);
            pthread_mutex_unlock(&lock);
            return 0;
        }
    }

    /* Step 3: tentatively allocate */
    for (j = 0; j < r; j++) {
        avail[j]       -= req[j];
        alloc[pid][j]  += req[j];
        need[pid][j]   -= req[j];
    }

    /* Step 4: safety check — rollback if unsafe */
    if (is_safe()) {
        printf("  [GRANTED] P%d request approved.\n", pid);
        pthread_mutex_unlock(&lock);
        return 1;
    }

    /* Rollback */
    for (j = 0; j < r; j++) {
        avail[j]       += req[j];
        alloc[pid][j]  -= req[j];
        need[pid][j]   += req[j];
    }
    printf("  [DENY] P%d request denied — leaves system unsafe.\n", pid);

    pthread_mutex_unlock(&lock);
    return 0;
}

/* ══════════════════════════════════════════════════
 *  RELEASE RESOURCES
 * ══════════════════════════════════════════════════ */
void release_resources(int pid, int rel[])
{
    int j;

    pthread_mutex_lock(&lock);

    for (j = 0; j < r; j++) {
        avail[j]      += rel[j];
        alloc[pid][j] -= rel[j];
        need[pid][j]  += rel[j];
    }

    printf("\nP%d released:   [", pid);
    for (j = 0; j < r; j++) printf("%d%s", rel[j], j < r-1 ? " " : "");
    printf("]\n");

    pthread_mutex_unlock(&lock);
}

/* ══════════════════════════════════════════════════
 *  PROCESS THREAD
 * ══════════════════════════════════════════════════ */
void *process_thread(void *arg)
{
    int pid = *(int *)arg;
    free(arg);

    int req[MAX_R];
    int j;

    /* Request whatever the process still needs */
    pthread_mutex_lock(&lock);
    for (j = 0; j < r; j++) req[j] = need[pid][j];
    pthread_mutex_unlock(&lock);

    if (request_resources(pid, req)) {
        sleep(1);   /* simulate using the resources */

        int rel[MAX_R];
        pthread_mutex_lock(&lock);
        for (j = 0; j < r; j++) rel[j] = alloc[pid][j];
        pthread_mutex_unlock(&lock);

        release_resources(pid, rel);
    }

    return NULL;
}

/* ══════════════════════════════════════════════════
 *  MAIN
 * ══════════════════════════════════════════════════ */
int main()
{
    int i, j;

    /* ── Take input at runtime ── */
    printf("Enter number of processes: ");
    scanf("%d", &n);

    printf("Enter number of resources: ");
    scanf("%d", &r);

    if (n < 1 || n > MAX_P || r < 1 || r > MAX_R) {
        printf("Error: processes must be 1-%d, resources must be 1-%d\n",
               MAX_P, MAX_R);
        return 1;
    }

    printf("\nEnter available resources (%d values): ", r);
    for (j = 0; j < r; j++)
        scanf("%d", &avail[j]);

    printf("\nEnter MAX demand matrix (%d processes x %d resources):\n", n, r);
    for (i = 0; i < n; i++) {
        printf("  P%d max: ", i);
        for (j = 0; j < r; j++)
            scanf("%d", &max_demand[i][j]);
    }

    printf("\nEnter ALLOCATION matrix (%d processes x %d resources):\n", n, r);
    for (i = 0; i < n; i++) {
        printf("  P%d allocation: ", i);
        for (j = 0; j < r; j++)
            scanf("%d", &alloc[i][j]);
    }

    /* Compute need = max - alloc */
    for (i = 0; i < n; i++)
        for (j = 0; j < r; j++)
            need[i][j] = max_demand[i][j] - alloc[i][j];

    /* ── Print initial state ── */
    printf("\n=== Banker's Algorithm ===\n");
    printf("Processes: %d  |  Resources: %d\n\n", n, r);

    printf("%-6s  %-s\n", "Proc", "Max / Allocation / Need");
    printf("------  ------------------------------------------\n");
    for (i = 0; i < n; i++) {
        printf("P%-5d  Max:[", i);
        for (j = 0; j < r; j++) printf("%d%s", max_demand[i][j], j<r-1?" ":"");
        printf("]  Alloc:[");
        for (j = 0; j < r; j++) printf("%d%s", alloc[i][j], j<r-1?" ":"");
        printf("]  Need:[");
        for (j = 0; j < r; j++) printf("%d%s", need[i][j], j<r-1?" ":"");
        printf("]\n");
    }
    printf("\nAvailable: [");
    for (j = 0; j < r; j++) printf("%d%s", avail[j], j<r-1?" ":"");
    printf("]\n");

    /* ── Initial safety check ── */
    printf("\n--- Initial Safety Check ---\n");
    pthread_mutex_lock(&lock);
    if (!is_safe()) {
        printf("[UNSAFE] Initial state is not safe. Exiting.\n");
        pthread_mutex_unlock(&lock);
        return 1;
    }
    pthread_mutex_unlock(&lock);

    /* ── Spawn one thread per process ── */
    printf("\n--- Spawning Process Threads ---\n");
    pthread_t tid[MAX_P];
    for (i = 0; i < n; i++) {
        int *pid = malloc(sizeof(int));
        *pid = i;
        pthread_create(&tid[i], NULL, process_thread, pid);
    }

    for (i = 0; i < n; i++)
        pthread_join(tid[i], NULL);

    printf("\n=== All processes finished. No deadlock. ===\n");

    pthread_mutex_destroy(&lock);
    return 0;
}
