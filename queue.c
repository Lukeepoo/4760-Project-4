#include <stdio.h>
#include "queue.h"
#include "pcb.h"
#include "clock.h"
#include "utility.h"

Queue readyQueue0, readyQueue1, readyQueue2, blockedQueue;

void initializeQueues() {
    Queue *qs[] = {&readyQueue0, &readyQueue1, &readyQueue2, &blockedQueue};
    for (int i = 0; i < 4; i++) {
        qs[i]->front = 0;
        qs[i]->rear = -1;
        qs[i]->count = 0;
    }
}

int enqueue(Queue *q, int val) {
    if (q->count >= MAX_QUEUE_SIZE) return -1;
    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->queue[q->rear] = val;
    q->count++;
    return 0;
}

int dequeue(Queue *q) {
    if (q->count <= 0) return -1;
    int val = q->queue[q->front];
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->count--;
    return val;
}

int isEmpty(Queue *q) {
    return q->count == 0;
}

int peek(Queue *q) {
    return q->queue[q->front];
}

int getNextReadyProcess(int *usage) {
    if (!isEmpty(&readyQueue0)) { usage[0]++; return dequeue(&readyQueue0); }
    if (!isEmpty(&readyQueue1)) { usage[1]++; return dequeue(&readyQueue1); }
    if (!isEmpty(&readyQueue2)) { usage[2]++; return dequeue(&readyQueue2); }
    return -1;
}

int getQueueLevel(int index) {
    if (!isEmpty(&readyQueue0) && peek(&readyQueue0) == index) return 0;
    if (!isEmpty(&readyQueue1) && peek(&readyQueue1) == index) return 1;
    return 2;
}

void checkBlocked(SystemClock *clk, PCB *table, Queue *blocked, FILE *logFile) {
    int count = blocked->count;
    for (int i = 0; i < count; i++) {
        int idx = dequeue(blocked);
        if (compareTime(clk, table[idx].eventWaitSec, table[idx].eventWaitNano) >= 0) {
            table[idx].blocked = 0;
            enqueue(&readyQueue0, idx);
            fprintf(logFile, "OSS: Unblocking P%d into queue 0\n", idx);
        } else enqueue(blocked, idx);
    }
}

void printQueues(FILE *logFile) {
    fprintf(logFile, "Q0: ");
    for (int i = 0; i < readyQueue0.count; i++)
        fprintf(logFile, "P%d ", readyQueue0.queue[(readyQueue0.front + i) % MAX_QUEUE_SIZE]);
    fprintf(logFile, "\nQ1: ");
    for (int i = 0; i < readyQueue1.count; i++)
        fprintf(logFile, "P%d ", readyQueue1.queue[(readyQueue1.front + i) % MAX_QUEUE_SIZE]);
    fprintf(logFile, "\nQ2: ");
    for (int i = 0; i < readyQueue2.count; i++)
        fprintf(logFile, "P%d ", readyQueue2.queue[(readyQueue2.front + i) % MAX_QUEUE_SIZE]);
    fprintf(logFile, "\nBlocked: ");
    for (int i = 0; i < blockedQueue.count; i++)
        fprintf(logFile, "P%d ", blockedQueue.queue[(blockedQueue.front + i) % MAX_QUEUE_SIZE]);
    fprintf(logFile, "\n");
}
