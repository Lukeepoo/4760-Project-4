#include <stdio.h>
#include "utility.h"

void advanceClock(SystemClock *clk, int sec, int nano) {
    clk->nanoseconds += nano;
    clk->seconds += sec + clk->nanoseconds / 1000000000;
    clk->nanoseconds %= 1000000000;
}

int compareTime(SystemClock *clk, int sec, int nano) {
    if (clk->seconds > sec || (clk->seconds == sec && clk->nanoseconds >= nano))
        return 1;
    return 0;
}

void printProcessTable(FILE *logFile, PCB *table) {
    fprintf(logFile, "Idx | PID   | Start       | CPU Time     | Blocked\n");
    for (int i = 0; i < 18; i++) {
        if (table[i].occupied) {
            fprintf(logFile, "%3d | %5d | %u:%09d | %u:%09d | %d\n",
                i, table[i].pid,
                table[i].startSeconds, table[i].startNano,
                table[i].serviceTimeSeconds, table[i].serviceTimeNano,
                table[i].blocked);
        }
    }
}
