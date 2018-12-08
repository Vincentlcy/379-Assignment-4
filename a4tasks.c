# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <stdarg.h>
# include <assert.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <sys/types.h>
# include <signal.h>
# include <string.h>
# include <malloc.h>
# include <pthread.h>
# include <sys/times.h>
# include <errno.h>

# define NRES_TYPES 10
# define NTASKS 25

typedef enum {IDLE, RUN, WAIT} STATE;

struct rescources {
    char name[32];
    int value;
    struct rescources *next;
};

typedef struct rescources RESOURCE;

RESOURCE resList[NRES_TYPES];

typedef struct {
    char name[32];
    int busyTime;
    int idleTime;
    STATE state;
    int totalRunTime;
    int totalIdleTime;
    int totalWaitTime;
    int iteration;
    int ready;
    int numreq;
    RESOURCE reqRes[NRES_TYPES];
    pthread_t taskID;
} TASK;

TASK taskList[NTASKS];
int NITER;
pthread_mutex_t monitorMutex;
pthread_mutex_t resourceMutex;
pthread_mutex_t taskMutex;
int ITASK;
int IRES;

void millsleep(int millsecond);
void setResList(char *line);
void readFile(char *filename);
void mutex_unlock(pthread_mutex_t* mutex);
void mutex_lock(pthread_mutex_t* mutex);
void mutex_init(pthread_mutex_t* mutex);
void addValueRes(char* key, int value);
void *taskExecute(void* index);
int findValueRes(char* key);
int checkResources(RESOURCE *reqRes, int size);
void takeRescources(RESOURCE *reqRes, int size);
void returnResources(RESOURCE *reqRes, int size);
void *monitorExecute(void *time);
void divideRes(char *name, int i);


int main(int argc, char *argv[]) {
    clock_t stime, etime;

    struct tms tmsstart;
    struct tms tmsend;

    stime = times(&tmsstart);

    int monitorTime;

    if (argc != 4) {
        printf("Number of arguments is wrong\n");
        return -1;
    }

    monitorTime = atoi(argv[2]);
    NITER = atoi(argv[3]);

    // read the file
    readFile(argv[1]);

    int err;
    pthread_t ntid;

    printf("Ready to create Thread\n");

    // create monitor thread.
    err = pthread_create(&ntid, NULL, monitorExecute, &monitorTime);
    if (err != 0) {
        printf("Fail to create monitor thread.\n");
    }

    // create task thread 
    mutex_lock(&taskMutex);
    for (int i=0; i<ITASK; i++) {
        err = pthread_create(&ntid, NULL, taskExecute, &i);
        if (err != 0) {
            printf("Fail to create task thread\n");
        }
        mutex_lock(&taskMutex);
    }

    millsleep(500);

    // wait all task thread to end
    for (int i=0; i<ITASK;i++) {
        err = pthread_join(taskList[i].taskID, NULL);
        if (err != 0) {
            printf("Fail to wait task thread to end\n");
        }
    }
    
    // print the termination information and exit
    // all other thread (monitor thread) will end 
    printf("All tasks are finished\n");

    // print system rescources
    printf("\nSystem Resources:\n");
    int i = 0;
    for (i=0; i<IRES; i++) {
        printf("\t%s: (maxAvail= %d, held= 0)\n", resList[i].name, resList[i].value);
    };

    // print task info
    printf("\nSystem Tasks:\n");
    for (int i=0; i<ITASK; i++) {
        char states[10];
        // get the state value
        if (taskList[i].state == WAIT) {
            strcpy(states, "WAIT");
        } else if (taskList[i].state == RUN) {
            strcpy(states, "RUN");
        } else {
            strcpy(states, "IDLE");
        }

        printf("%s (%s, runTime= %d msec, idleTime= %d msec):\n", taskList[i].name, states, taskList[i].busyTime, taskList[i].idleTime);
        printf("\t(tid= %lu)\n", taskList[i].taskID);
        for(int j=0; j<taskList[i].numreq; j++) {
            printf("\t%s: (needed= %d, held= %d)\n", taskList[i].reqRes[j].name, taskList[i].reqRes[j].value, 0);
        };
        printf("\t(RUN: %d times, WAIT: %d msec)\n", taskList[i].iteration, taskList[i].totalWaitTime);
    }
    etime = times(&tmsend);
    clock_t t = etime -stime;
    printf("Running time = %d msec\n", (int)((double) t/CLOCKS_PER_SEC*10000000));
    return 0;
}

void *taskExecute(void* index_p) {
    int index = *(int *)index_p;
    TASK *task;
    task = &taskList[index];
    task->taskID = pthread_self();
    task->ready = 1;
    printf("Task %d: %s Thread Started\n", index, task->name);
    mutex_unlock(&taskMutex);

    clock_t startTime, waitTime;
    struct tms tmswaitstart;
    struct tms tmswaitend;
    
    mutex_lock(&monitorMutex);
    task->state = WAIT;
    mutex_unlock(&monitorMutex);

    startTime = times(&tmswaitstart);
    while (1) {
        // go into the loop 
        // startTime = times(&tmswaitstart); change since may not have rescource at first time

        // check the rescources and if avilbe than start run
        mutex_lock(&resourceMutex);
        //printf("task %s ",task->name);
        if (checkResources(task->reqRes, task->numreq)==0) {
            mutex_unlock(&resourceMutex);
            millsleep(23);
            continue;
        }

        waitTime = times(&tmswaitend);
        task->totalWaitTime += ((double)(waitTime - startTime))*10000000/CLOCKS_PER_SEC;
        takeRescources(task->reqRes, task->numreq);
        mutex_unlock(&resourceMutex);

        // as all value need has been take, task states tend to busy
        mutex_lock(&monitorMutex);
        task->state = RUN;
        mutex_unlock(&monitorMutex);

        // "run" for task.busyTime
        millsleep(task->busyTime);
        task->totalRunTime += task->busyTime;
        // return the rescources
        mutex_lock(&resourceMutex);
        returnResources(task->reqRes, task->numreq);
        mutex_unlock(&resourceMutex);

        // into idle time
        mutex_lock(&monitorMutex);
        task->state = IDLE;
        mutex_unlock(&monitorMutex);
        // "idle" for task.idleTime
        millsleep(task->idleTime);
        task->iteration++;
        waitTime = times(&tmswaitend);
        int iterTime = ((double)(waitTime - startTime))*10000000/CLOCKS_PER_SEC;
        printf("task: %s (tid= %lu, iter= %d, time= %dmsec)\n", task->name, task->taskID, task->iteration, iterTime);
        if (task->iteration >= NITER) {
            printf("task %s finished\n", task->name);
            pthread_exit("Final");
        }

        //reset the states to wait
        mutex_lock(&monitorMutex);
        task->state = WAIT;
        mutex_unlock(&monitorMutex);

        startTime = times(&tmswaitstart);
    }
    return NULL;
}

void *monitorExecute(void *time) {
    // moitor all task states after a fix time
    printf("Monitor Thread Started\n");

    int t = *(int *) time;

    // MAX 25 task each with max 32 char name
    // no need to move out since the complier will manage the space when complie
    char waitString[850];
    char runString[850];
    char idleString[850];

    while(1) {
        // sleep for amount time than lock the task states
        millsleep(t);
        mutex_lock(&monitorMutex);

        strcpy(waitString,"");
        strcpy(runString,"");
        strcpy(idleString,"");

        // check all tasks

        for (int i=0; i<ITASK;i++) {
            printf("%d", taskList[i].state);
            if (taskList[i].state == WAIT) {
                strcat(waitString, " ");
                strcat(waitString, taskList[i].name);
            } else if (taskList[i].state == RUN) {
                strcat(runString, " ");
                strcat(runString, taskList[i].name);
            } else {
                strcat(idleString, " ");
                strcat(idleString, taskList[i].name);
            }
        }

        // print the information just produced
        printf("\nmonitor: [WAIT]%s\n\t [RUN]%s\n\t [IDLE]%s\n", waitString, runString, idleString);

        // lock mutex
        mutex_unlock(&monitorMutex);
    }
}

void returnResources(RESOURCE *reqRes, int size) {
    // return the rescource the task need
    if (size == 0) {
        // no need for rescource auto return
        return;
    }

    for (int i=0; i<size; i++) {
        addValueRes(reqRes[i].name, reqRes[i].value);
    };

    return;
}

void takeRescources(RESOURCE *reqRes, int size) {
    // take the rescource the task need
    if (size == 0) {
        // no need for rescource auto return
        return;
    }

    for (int i=0; i<size; i++) {
        addValueRes(reqRes[i].name, -reqRes[i].value);
    };

    return;
}

int checkResources(RESOURCE *reqRes, int size) {
    // check if there are enough resource
    // return 0 as fail and 1 as correct
    if (reqRes == NULL) {
        return 1;
    }

    for (int i=0; i<size; i++) {
        // if remain resource is less than task need return 0 as fail
        if (findValueRes(reqRes[i].name) < reqRes[i].value) {
            //printf("%d %s \n",findValueRes(reqRes[i].name) ,reqRes[i].name);
            return 0;
        }
    };

    return 1;
}

void millsleep(int millsecond) {
    // sleep for millsecond 
    // modify from lab
    /***
    struct timespec interval;
    interval.tv_sec = (millsecond - millsecond%1000)/1000;
    interval.tv_nsec = millsecond%1000 * 1000000;
    nanosleep($interval, NULL)
    ***/
    struct timespec interval;
	interval.tv_sec = (long) millsecond / 1000;
	interval.tv_nsec = (long) ((millsecond % 1000) * 1000000);
	if (nanosleep(&interval, NULL) < 0)
		printf("warning: delay: %s\n", strerror(errno));
}

void mutex_init(pthread_mutex_t* mutex) {	
    // modify from lab
	int rval = pthread_mutex_init(mutex, NULL);
	if (rval) { fprintf(stderr, "mutex_init: %s\n", strerror(rval)); exit(1); }
}

void mutex_lock(pthread_mutex_t* mutex) {
    // modify from lab
	int rval = pthread_mutex_lock(mutex);
	if (rval) { fprintf(stderr, "mutex_lock: %s\n", strerror(rval)); exit(1); }
}

void mutex_unlock(pthread_mutex_t* mutex) {
    // modify from lab
	int rval = pthread_mutex_unlock(mutex);
	if (rval) { fprintf(stderr, "mutex_unlock: %s\n", strerror(rval)); exit(1); }
}

void readFile(char *filename) {
    // function to read the file and set up the information
    FILE *filefp;
    filefp = fopen(filename, "r");
    int itask = 0;

    if (filefp == NULL) {
        printf("Fail to read file\n");
        exit(-1);
    }

    char line[200];
    while (fgets(line, 200, filefp)!=NULL) {
        // read one line from the file
        if (line[0]=='#'||line[0] == '\r'||line[0] == '\n') {
            continue;
        } else {
            char *temp;
            temp = strtok(line, " ");
            if (strcmp(temp, "resources")==0) {
                setResList(line);
            } else {
                // this line is a task
                temp = strtok(NULL, " ");
                strcpy(taskList[itask].name, temp);
                temp = strtok(NULL, " ");
                taskList[itask].busyTime = atoi(temp);
                temp = strtok(NULL, " ");
                taskList[itask].idleTime = atoi(temp);
                taskList[itask].totalRunTime = 0;
                taskList[itask].totalIdleTime = 0;
                taskList[itask].totalWaitTime = 0;
                taskList[itask].iteration = 0;
                taskList[itask].ready = 0;
                taskList[itask].state = IDLE;

                int i = 0;
                strcpy(taskList[itask].reqRes[i].name, "\n");
                char taskTemp[10][35];
                while ((temp = strtok(NULL, " "))!=NULL) {
                    strcpy(taskTemp[i], temp);
                    taskList[itask].reqRes[i].next = (void*)NULL;
                    if (i>0) {
                        taskList[itask].reqRes[i].next =  &taskList[itask].reqRes[i-1];
                    }
                    i++;
                }
                taskList[itask].numreq = i;
                for (int j=0;j<taskList[itask].numreq;j++) {
                    strcpy(taskList[itask].reqRes[j].name, strtok(taskTemp[j], ":"));
                    taskList[itask].reqRes[j].value = atoi(strtok(NULL, ":"));
                }
                itask++;
            }
        }
    }
    ITASK = itask;
    return;
}

void setResList(char *line) {
    // function to read the line of resource
    char resTemp[10][35];
    char *temp;
    int i = 0;
    temp = strtok(NULL, " ");
    while (temp!=NULL) {
        strcpy(resTemp[i], temp);
        resList[i].next = NULL;
        strcpy(resList[i+1].name, "\n");
        if (i>0) {
            resList[i-1].next = &resList[i];
        }
        temp = strtok(NULL, " ");
        i++;
    }
    IRES = i;
    for (i=0;i<IRES;i++) {
        divideRes(resTemp[i], i);
    }
    return;
}

void divideRes(char *temp, int i) {
    // small funciton to handle key:value
    strcpy(resList[i].name, strtok(temp, ":"));
    resList[i].value = atoi(strtok(NULL, ":"));
    return;
}

int findValueRes(char *key) {
    // find the value by the key like map or dictionary
    for (int i=0; i<IRES; i++) {
        if (strcmp(key, resList[i].name)==0) {
            return resList[i].value;
        }
    };
    return -255;
}

void addValueRes(char *key, int value) {
    // find the value by the key like map or dictionary and modify it
    
    for (int i=0; i<IRES; i++)  {
        if (strcmp(key, resList[i].name)==0) {
            resList[i].value += value;
        }
    };
    return;
}