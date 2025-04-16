// user.c - Simulated User Process
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include "clock.h"
#include "pcb.h"
#include "message.h"

#define TERMINATE_PROB 5     // 5% chance to terminate when scheduled
#define IO_INTERRUPT_PROB 25 // 25% chance to get blocked

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: ./user [index] [shmClockID] [shmPCBID] [msqid]\n");
        exit(1);
    }

    int index = atoi(argv[1]);
    int shmClockID = atoi(argv[2]);
    int shmPCBID = atoi(argv[3]);
    int msqid = atoi(argv[4]);

    // Seed RNG based on PID to ensure unique behavior
    srand(getpid() * time(NULL));

    // Attach to shared memory
    SystemClock *clock = (SystemClock *)shmat(shmClockID, NULL, 0);
    PCB *pcbTable = (PCB *)shmat(shmPCBID, NULL, 0);

    MessageBuffer msg;

    while (1) {
        // Wait for message from OSS
        if (msgrcv(msqid, &msg, sizeof(msg.mtext), getpid(), 0) == -1) {
            perror("user: msgrcv");
            break;
        }

        int quantum = atoi(msg.mtext);

        int terminateChance = rand() % 100;
        if (terminateChance < TERMINATE_PROB) {
            // Process decides to terminate after using part of its timeslice
            int usedTime = (rand() % (quantum - 1)) + 1;
            msg.mtype = 1;
            sprintf(msg.mtext, "-%d", usedTime); // Negative means terminate
            msgsnd(msqid, &msg, sizeof(msg.mtext), 0);
            break;
        }

        int ioChance = rand() % 100;
        if (ioChance < IO_INTERRUPT_PROB) {
            // Process uses part of quantum and blocks
            int usedTime = (rand() % (quantum - 1)) + 1;
            msg.mtype = 1;
            sprintf(msg.mtext, "%d", usedTime); // Positive < quantum
            msgsnd(msqid, &msg, sizeof(msg.mtext), 0);
        } else {
            // Process uses entire timeslice
            msg.mtype = 1;
            sprintf(msg.mtext, "%d", quantum);
            msgsnd(msqid, &msg, sizeof(msg.mtext), 0);
        }
    }

    return 0;
}
