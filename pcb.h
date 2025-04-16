#ifndef PCB_H
#define PCB_H

#include <sys/types.h>

typedef struct {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
    int serviceTimeSeconds;
    int serviceTimeNano;
    int eventWaitSec;
    int eventWaitNano;
    int blocked;
} PCB;

#endif
