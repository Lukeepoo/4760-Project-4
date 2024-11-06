#ifndef PCB_H
#define PCB_H

#include <sys/types.h>

typedef struct PCB {
    int occupied;          // either true (1) or false (0)
    pid_t pid;             // process id of this child
    int startSeconds;      // time when it was created (seconds)
    int startNano;         // time when it was created (nanoseconds)
    int serviceTimeSeconds; // total seconds it has been scheduled
    int serviceTimeNano;   // total nanoseconds it has been scheduled
    int eventWaitSec;      // seconds until event completion
    int eventWaitNano;     // nanoseconds until event completion
    int blocked;           // is this process waiting on an event (1 if blocked, 0 otherwise)
} PCB;

#define MAX_PROCESSES 18

#endif