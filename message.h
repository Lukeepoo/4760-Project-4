#ifndef MESSAGE_H
#define MESSAGE_H

#define MSGSZ 20

typedef struct {
    long mtype;
    char mtext[MSGSZ];
} MessageBuffer;

#endif
