#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <sys/wait.h>
#include "pcb.h"

const int SHM_KEY = 1234;
const int MSG_KEY = 5432;
#define CLOCK_INCREMENT 1000 // Nanoseconds increment for each OSS action

typedef enum {
    GOOD = 0,
    INTERRUPT = 1,
    TERMINATE = 2
} State;

// Shared memory structure for clock
typedef struct {
    long seconds;
    long nanoseconds;
} SharedClock;

// Message structure
typedef struct {
    long mtype;
    pid_t pid;
    State programState;
    long seconds;
    long nanoseconds;
} Message;

int shm_id, msg_id;
SharedClock *shared_clock;
PCB processTable[MAX_PROCESSES];
int blocked_queue[MAX_PROCESSES];
float priority_queue[MAX_PROCESSES];

void cleanup(int);
pid_t launch_process(int, int);
void increment_clock(unsigned int, unsigned int);

void message_send(pid_t worker);
Message message_receive();

void block_process(pid_t worker, long seconds, long nanoseconds);
int is_blocked(pid_t worker);
void calculate_priorities();
pid_t determine_next_child();
void terminate_all_children();

int main(int argc, char *argv[]) {
    int max_processes = 18;
    int total_processes = 0;
    int max_simultaneous = 5;
    int time_to_launch = 1000000;
    char *logfile = "log.txt";
    int childrenToRun = 0;

    int opt;
    while ((opt = getopt(argc, argv, "hn:s:t:f:")) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: %s [-h] [-n proc] [-s simul] [-t timeToLaunchNewChild] [-f logfile]\n", argv[0]);
                exit(0);
            case 'n':
                max_processes = strtol(optarg, NULL, 10);
                break;
            case 's':
                max_simultaneous = strtol(optarg, NULL, 10);
                break;
            case 't':
                time_to_launch = strtol(optarg, NULL, 10);
                break;
            case 'f':
                logfile = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s [-h] [-n proc] [-s simul] [-t timeToLaunchNewChild] [-f logfile]\n", argv[0]);
                exit(1);
        }
    }

    // Set up shared memory for clock
    shm_id = shmget(SHM_KEY, sizeof(SharedClock), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }
    shared_clock = (SharedClock *)shmat(shm_id, NULL, 0);
    if (shared_clock == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    // Remove any existing message queue with the same key
    msg_id = msgget(MSG_KEY, 0666);
    if (msg_id != -1) {
        msgctl(msg_id, IPC_RMID, NULL);
    }

    // Set up message queue
    msg_id = msgget(MSG_KEY, IPC_CREAT | 0644);
    if (msg_id < 0) {
        perror("msgget");
        exit(1);
    }

    // Initialize shared clock
    shared_clock->seconds = 0;
    shared_clock->nanoseconds = 0;

    // Set up signal handler for cleanup
    signal(SIGINT, cleanup); // ctrl + c
    signal(SIGALRM, cleanup); // 60 second timeout
    alarm(60);

    // Main loop for launching and scheduling processes
    while (total_processes < max_processes || childrenToRun > 0) {
        if (total_processes < max_simultaneous && total_processes < max_processes) {
            launch_process(total_processes, msg_id);
            total_processes++;
            childrenToRun++;
        }

        pid_t nextChild = determine_next_child();
        if (nextChild < 0) {
            increment_clock(0, 750);
            continue;
        }

        if (is_blocked(nextChild)) {
            increment_clock(0, 750);
            continue;
        }

        message_send(nextChild);
        printf("Message Received. \n");
        Message msg = message_receive();

        if (msg.programState == GOOD) {
            increment_clock(msg.seconds, msg.nanoseconds);
        } else if (msg.programState == INTERRUPT) {
            block_process(nextChild, msg.seconds, msg.nanoseconds);
        } else if (msg.programState == TERMINATE) {
            increment_clock(msg.seconds, msg.nanoseconds);
            childrenToRun--;
        }

        printf("Seconds: %ld\n", shared_clock->seconds);
        printf("Nanoseconds: %ld\n", shared_clock->nanoseconds);
    }

    terminate_all_children();
    cleanup(0);
    return 0;
}

pid_t launch_process(int index, int msg_id) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execl("./user", "./user", (char *)NULL);
        perror("execl");
        exit(1);
    } else if (pid > 0) {
        // Parent process
        processTable[index].occupied = 1;
        processTable[index].pid = pid;
        processTable[index].startSeconds = shared_clock->seconds;
        processTable[index].startNano = shared_clock->nanoseconds;
        printf("OSS: Launched process with PID %d\n", pid);
    } else {
        perror("fork");
    }

    return pid;
}

void increment_clock(unsigned int sec, unsigned int nano) {
    shared_clock->nanoseconds += nano;
    if (shared_clock->nanoseconds >= 1000000000) {
        shared_clock->seconds += 1;
        shared_clock->nanoseconds -= 1000000000;
    }
    shared_clock->seconds += sec;
}

void cleanup(int signo) {
    printf("Cleaning up...\n");
    terminate_all_children();
    // Detach and remove shared memory
    shmdt(shared_clock);
    shmctl(shm_id, IPC_RMID, NULL);
    // Remove message queue
    msgctl(msg_id, IPC_RMID, NULL);
    exit(0);
}

void message_send(pid_t worker) {
    Message msg;

    msg.mtype = worker;
    msg.pid = getpid();
    msg.seconds = 0;
    msg.nanoseconds = 50000000;
    msg.programState = GOOD; // Default state for testing

    if (msgsnd(msg_id, &msg, sizeof(Message), 0) == -1) {
        perror("OSS: Error: msgsnd.\n");
        cleanup(0);
        exit(1);
    }
}

Message message_receive() {
    Message msg;

    if (msgrcv(msg_id, &msg, sizeof(Message), getpid(), 0) == -1) {
        perror("OSS: Error: msgrcv. \n");
    }

    return msg;
}

void block_process(pid_t worker_pid, long secondsToBlock, long nanosecondsToBlock) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i].pid == worker_pid) {
            processTable[i].blocked = 1;
            processTable[i].eventWaitSec = secondsToBlock;
            processTable[i].eventWaitNano = nanosecondsToBlock;

            blocked_queue[i] = 1;
        }
    }
}

int is_blocked(pid_t worker_pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (blocked_queue[i] == 1) {
            return 1;
        }
    }
    return 0;
}

void calculate_priorities() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i].occupied) {
            float totalTimeInSystem = (processTable[i].serviceTimeSeconds * 1e9) + processTable[i].serviceTimeNano;
            float totalServiceTime = (shared_clock->seconds - processTable[i].startSeconds) * 1e9
                                     + (shared_clock->nanoseconds - processTable[i].startNano);
            if (totalTimeInSystem > 0) {
                priority_queue[i] = totalServiceTime / totalTimeInSystem;
            } else {
                priority_queue[i] = 0; // Assign highest priority to new processes
            }
        }
    }
}

pid_t determine_next_child() {
    pid_t child = -1;
    float highestPriority = 1.0;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i].occupied && priority_queue[i] < highestPriority) {
            highestPriority = priority_queue[i];
            child = processTable[i].pid;
        }
    }

    return child;
}

void terminate_all_children() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i].occupied) {
            kill(processTable[i].pid, SIGTERM);
            waitpid(processTable[i].pid, NULL, 0);
        }
    }
}
