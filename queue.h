#ifndef QUEUE_H
#define QUEUE_H

#include "clock.h"
#include "pcb.h"

#define MAX_QUEUE_SIZE 20

typedef struct {
    int queue[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
} Queue;

extern Queue readyQueue0, readyQueue1, readyQueue2, blockedQueue;

void initializeQueues();
int enqueue(Queue *q, int val);
int dequeue(Queue *q);
int isEmpty(Queue *q);
int peek(Queue *q);
int getNextReadyProcess(int *usage);
int getQueueLevel(int index);
void checkBlocked(SystemClock *clk, PCB *table, Queue *blocked, FILE *logFile);
void printQueues(FILE *logFile);

#endif
