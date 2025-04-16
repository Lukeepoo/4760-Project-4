#ifndef UTILITY_H
#define UTILITY_H

#include "clock.h"
#include "pcb.h"

void advanceClock(SystemClock *clk, int sec, int nano);
int compareTime(SystemClock *clk, int sec, int nano);
void printProcessTable(FILE *logFile, PCB *table);

#endif
