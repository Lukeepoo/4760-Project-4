#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include "pcb.h"

#define MSG_KEY 5432

typedef enum {
    GOOD = 0,
    INTERRUPT = 1,
    TERMINATE = 2
} State;

typedef struct {
    long mtype;
    pid_t pid;
    State programState;
    long seconds;
    long nanoseconds;
} Message;

int main() {
    int msg_id = msgget(MSG_KEY, 0666);
    if (msg_id < 0) {
        perror("msgget");
        exit(1);
    }

    Message msg;
    // Wait for a message from OSS
    if (msgrcv(msg_id, &msg, sizeof(Message), getpid(), 0) < 0) {
        perror("msgrcv");
        exit(1);
    }

    printf("User process %d received quantum %ld\n", getpid(), msg.seconds);
    // Simulate work
    sleep(1);

    // Send message back to OSS
    msg.mtype = 1;  // Assign appropriate message type
    msg.programState = GOOD; // Or any state relevant to the simulation
    msg.seconds = rand() % 100 + 1; // Random quantum value for the user process

    if (msgsnd(msg_id, &msg, sizeof(Message), 0) < 0) {
        perror("msgsnd");
        exit(1);
    }

    return 0;
}
