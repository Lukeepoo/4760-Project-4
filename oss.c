#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include "pcb.h"
#include "clock.h"
#include "message.h"
#include "queue.h"
#include "utility.h"

#define MAX_PROCESSES 18
#define MAX_TOTAL_PROCESSES 100
#define BASE_QUANTUM 10000000     // 10 ms in ns
#define DISPATCH_OVERHEAD 1000
#define LOGFILE "log.txt"

SystemClock *sysClock;
PCB *processTable;
int shmClockID, shmPCBID, msqid;
FILE *logFile;

int totalCreated = 0;
int totalActive = 0;
int blockedTotal = 0;
unsigned int cpuIdle = 0;
int queueUsage[3] = {0};

void cleanup(int sig) {
    fprintf(stderr, "Cleaning up shared memory and message queues...\n");

    // Safely kill child processes BEFORE detaching
    if (processTable) {
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (processTable[i].occupied) {
                kill(processTable[i].pid, SIGTERM);
            }
        }
    }

    // Wait for any terminated children
    while (wait(NULL) > 0);

    // Detach shared memory
    if (sysClock) shmdt(sysClock);
    if (processTable) shmdt(processTable);

    // Remove shared memory segments
    if (shmClockID > 0) shmctl(shmClockID, IPC_RMID, NULL);
    if (shmPCBID > 0) shmctl(shmPCBID, IPC_RMID, NULL);

    // Remove message queue
    if (msqid > 0) msgctl(msqid, IPC_RMID, NULL);

    // Close log file
    if (logFile) fclose(logFile);

    exit(0);
}


void forkChild(int index) {
    pid_t pid = fork();
    if (pid == 0) {
        char args[4][10];
        sprintf(args[0], "%d", index);
        sprintf(args[1], "%d", shmClockID);
        sprintf(args[2], "%d", shmPCBID);
        sprintf(args[3], "%d", msqid);
        execl("./user", "./user", args[0], args[1], args[2], args[3], NULL);
        perror("execl");
        exit(1);
    } else {
        processTable[index].occupied = 1;
        processTable[index].pid = pid;
        processTable[index].startSeconds = sysClock->seconds;
        processTable[index].startNano = sysClock->nanoseconds;
        totalCreated++;
        totalActive++;
        fprintf(logFile, "OSS: Generating process with PID %d and putting it in queue 0 at time %u:%u\n",
                pid, sysClock->seconds, sysClock->nanoseconds);
        enqueue(&readyQueue0, index);
    }
}

int main() {
    srand(time(NULL));
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    shmClockID = shmget(IPC_PRIVATE, sizeof(SystemClock), IPC_CREAT | 0666);
    shmPCBID = shmget(IPC_PRIVATE, sizeof(PCB) * MAX_PROCESSES, IPC_CREAT | 0666);
    sysClock = shmat(shmClockID, NULL, 0);
    processTable = shmat(shmPCBID, NULL, 0);
    memset(sysClock, 0, sizeof(SystemClock));
    memset(processTable, 0, sizeof(PCB) * MAX_PROCESSES);

    msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    logFile = fopen(LOGFILE, "w");

    initializeQueues();

    unsigned int nextForkSec = 0, nextForkNS = 0;
    unsigned int logLimit = 0;
    time_t startTime = time(NULL);

    while ((totalCreated < MAX_TOTAL_PROCESSES || totalActive > 0) &&
           (time(NULL) - startTime < 3)) {

        advanceClock(sysClock, 0, 10000);
        checkBlocked(sysClock, processTable, &blockedQueue, logFile);

        if (compareTime(sysClock, nextForkSec, nextForkNS) >= 0 &&
            totalCreated < MAX_TOTAL_PROCESSES) {
            for (int i = 0; i < MAX_PROCESSES; i++) {
                if (!processTable[i].occupied) {
                    forkChild(i);
                    int rns = rand() % 1000000000;
                    nextForkNS = sysClock->nanoseconds + rns;
                    nextForkSec = sysClock->seconds + (nextForkNS / 1000000000);
                    nextForkNS %= 1000000000;
                    break;
                }
            }
        }

        int index = getNextReadyProcess(&queueUsage[0]);
        if (index == -1) {
            advanceClock(sysClock, 0, 100000000); // idle
            cpuIdle += 100000000;
            continue;
        }

        int level = getQueueLevel(index);
        int quantum = BASE_QUANTUM * (1 << level);

        MessageBuffer msg;
        msg.mtype = processTable[index].pid;
        sprintf(msg.mtext, "%d", quantum);
        msgsnd(msqid, &msg, sizeof(msg.mtext), 0);

        advanceClock(sysClock, 0, DISPATCH_OVERHEAD);
        fprintf(logFile, "OSS: Dispatching process with PID %d from queue %d at time %u:%u\n",
                processTable[index].pid, level, sysClock->seconds, sysClock->nanoseconds);

        msgrcv(msqid, &msg, sizeof(msg.mtext), 1, 0);
        int result = atoi(msg.mtext);
        int used = abs(result);
        advanceClock(sysClock, 0, used);

        processTable[index].serviceTimeNano += used;
        processTable[index].serviceTimeSeconds += processTable[index].serviceTimeNano / 1000000000;
        processTable[index].serviceTimeNano %= 1000000000;

        if (result < 0) {
            fprintf(logFile, "OSS: Receiving that process with PID %d terminated after using %d ns\n",
                    processTable[index].pid, used);
            processTable[index].occupied = 0;
            totalActive--;
        } else if (result < quantum) {
            processTable[index].blocked = 1;
            blockedTotal++;
            int wait = rand() % 500000000;
            processTable[index].eventWaitNano = sysClock->nanoseconds + wait;
            processTable[index].eventWaitSec = sysClock->seconds + processTable[index].eventWaitNano / 1000000000;
            processTable[index].eventWaitNano %= 1000000000;
            enqueue(&blockedQueue, index);
            fprintf(logFile, "OSS: Receiving that process with PID %d ran for %d ns, not using full quantum\n",
                    processTable[index].pid, used);
            fprintf(logFile, "OSS: Putting process with PID %d into blocked queue\n", processTable[index].pid);
        } else {
            int newLevel = (level < 2) ? level + 1 : 2;
            enqueue((newLevel == 1) ? &readyQueue1 : &readyQueue2, index);
            fprintf(logFile, "OSS: Receiving that process with PID %d ran full quantum of %d ns\n",
                    processTable[index].pid, used);
            fprintf(logFile, "OSS: Putting process with PID %d into queue %d\n", processTable[index].pid, newLevel);
        }

        logLimit++;
        if (logLimit % 50 == 0) {
            fprintf(logFile, "OSS: Outputting queues:\n");
            printQueues(logFile);
            fprintf(logFile, "OSS: Outputting process table:\n");
            printProcessTable(logFile, processTable);
        }

        if (logLimit >= 10000) break;
    }

    fprintf(logFile, "\n=== Statistics ===\n");
    fprintf(logFile, "CPU Idle Time: %u ns\n", cpuIdle);
    fprintf(logFile, "Q0 used: %d times\nQ1 used: %d times\nQ2 used: %d times\n",
            queueUsage[0], queueUsage[1], queueUsage[2]);
    fprintf(logFile, "Processes blocked: %d times\n", blockedTotal);

    cleanup(0);
    return 0;
}
