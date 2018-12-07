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

# define NRES_TYPES 10;
# define NTASKS 25;

typedef enum {IDLE; RUN; WAIT} STATE;

typedef struct {
    char name[32];
    int value;
    RESOURCE *next;
}  RESOURCE;

RESOURCE *resList[NRES_TYPES];

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
    RESOURCE reqRes[NRES_TYPES];
    int taskID;
} TASK;

TASK *taskList[];
int NITER;
pthread_mutex_t monitorMutex;
pthread_mutex_t resourceMutex;

void millsleep(int millsecond);
void setResList(char *line);
void readFile(char filename);
void mutex_unlock(pthread_mutex_t* mutex);
void mutex_lock(pthread_mutex_t* mutex);
void mutex_init(pthread_mutex_t* mutex);
void addValueRes(char key, int value);
void taskExecute(void* index)
int findValueRes(char key);
int checkResources(RESOURCE *reqRes);
void takeRescources(RESOURCE *reqRes);
void returnResources(RESOURCE *reqRes);
void monitorExecute(void *time)


int main(int argc, char *argv[]) {
    int monitorTime;

    if (argc != 4) {
        printf("Number of arguments is wrong\n");
        return -1;
    }

    monitorTime = atoi(argv[2]);
    NITER = atoi(argv[3]);

    // read the file
    readFile(argv[1]);

    /**
     * READ FILE PART TEST CODE
    int i=0;
    while (resList[i]) {
        printf("&s:&d", resList[i].name, resList[i].value);
        if (resList[i].next == NULL) {
            break;
        }
        i++;
    }
    **/

    int err;
    pthread_t ntid;

    // create monitor thread.
    err = pthread_create(&ntid, NULL, monitorExecute, (void*)monitorTime);
    if (err != 0) {
        printf("Fail to create monitor thread.\n");
        err_exit(err, "Fail to create monitor thread.\n");
    }

    // create task thread 
    for (int i=0; taskList[i].name!=NULL; i++) {
        err = pthread_create(&ntid, NULL, taskExecute, (void *)i);
        if (err != 0) {
            printf("Fail to create task thread\n");
            err_exit(err, "Fail to create task thread\n");
        }
    }

    // wait all task thread to end
    for (int i=0; taskList[i].name!=NULL;i++) {
        err = pthread_join(taskList[i].taskID, NULL);
        if (err != 0) {
            printf("Fail to wait task thread to end\n");
            err_exit(err, "Fail to wait task thread to end\n");
        }
    }
    
    // print the termination information and exit
    // all other thread (monitor thread) will end 
    printf("All tasks are finished\n");

    // print system rescources
    printf("\nSystem Resources:\n");
    int i = 0;
    do {
        printf("\t%s: (maxAvail= %d, held= 0)\n", resList[i].name, resList[i].value);
    } while(resList[i++].next != NULL);

    // print task info
    printf("\nSystem Tasks:\n");
    for (int i=0; taskList[i].name!=NULL; i++) {
        char states;
        // get the state value
        if (taskList[i].state == WAIT) {
            strcpy(states, "WAIT");
        } else if (taskList[i].state == RUN) {
            strcpy(states, "RUN");
        } else {
            strcpy(states, "IDLE");
        }

        printf("%s (%s, runTime= %d msec, idleTime= %d msec):\n", taskList[i].name, states, taskList[i].totalBusyTime, taskList[i].totalIdleTime);
        printf("\t(tid= %d)\n", taskList[i].taskID);
        do {
            printf("\t%s: (needed= %d, held= %d)\n", taskList[i].reqRes[i].name, taskList[i].reqRes[i].value, 0);
        } while(taskList[i].reqRes[i++].next != NULL);
    }
    return 0;
}

void taskExecute(void* index) {
    TASK *task;
    task = taskList[(int) index];
    task.taskID = pthread_self();

    clock_t startTime, waitTime;
    struct tms tmswaitstart, tmswaitend;
    
    mutex_lock(&monitorMutex);
    task.state = WAIT;
    mutex_unlock(&monitorMutex);

    startTime = times(&tmswaitstart);
    while (1) {
        // go into the loop 
        // startTime = times(&tmswaitstart); change since may not have rescource at first time

        // check the rescources and if avilbe than start run
        mutex_lock(&resourceMutex);

        if (checkResources(task.reqRes)==0) {
            mutex_unlock(&resourceMutex);
            nanosleep(200);
            continue;
        }

        waitTime = times(&tmswaitend);
        task.totalWaitTime += (waitTime - startTime)/clktck*1000
        takeRescources(task.reqRes);
        mutex_unlock(&resourceMutex);

        // as all value need has been take, task states tend to busy
        mutex_lock(&monitorMutex);
        task.state = RUN;
        mutex_unlock(&monitorMutex);
        // "run" for task.busyTime
        millsleep(task.busyTime);
        task.totalRunTime += task.busyTime;
        // return the rescources
        mutex_lock(&resourceMutex);
        returnResources(task.reqRes);
        mutex_unlock(&resourceMutex);

        // into idle time
        mutex_lock(&monitorMutex);
        task.state = IDLE;
        mutex_unlock(&monitorMutex);
        // "idle" for task.idleTime
        millsleep(task.idleTime);
        task.iteration++;
        waitTime = times(&tmswaitend);
        int iterTime = (waitTime - startTime)/clktck*1000
        printf("task: %s (tid= %d, iter= %d, time= %dmsec)\n", (task.name, task.taskID, task.iteration, iterTime);
        if (task.iteration >= NITER) {
            pthread_exit("Final");
        }

        //reset the states to wait
        mutex_lock(&monitorMutex);
        task.state = WAIT;
        mutex_unlock(&monitorMutex);

        startTime = times(&tmswaitstart);
    }
    return;
}

void monitorExecute(void *time) {
    // moitor all task states after a fix time
    int t = (int) time;

    while(1) {
        // sleep for amount time than lock the task states
        millsleep(time);
        mutex_lock(&monitorMutex);

        // MAX 25 task each with max 32 char name
        // no need to move out since the complier will manage the space when complie
        char waitString[850];
        char runString[850];
        char idleString[850];

        // check all tasks
        for (int i=0; taskList[i].name!=NULL;i++) {
            if (taskList[i].state == WAIT) {
                strcat(waitString, " ");
                strcat(waitString, taskList[i].name);
            } else if ((taskList[i].state == RUN) {
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

void returnResources(RESOURCE *reqRes) {
    // return the rescource the task need
    if (reqRes == NULL) {
        // no need for rescource auto return
        return;
    }

    int i =0;
    
    do {
        addValueRes(resList[i].name, resList[i].value);
    } while(reqRes[i++].next != NULL);

    return;
}

void takeRescources(RESOURCE *reqRes) {
    // take the rescource the task need
    if (reqRes == NULL) {
        // no need for rescource auto return
        return;
    }

    int i =0;
    
    do {
        addValueRes(resList[i].name, -resList[i].value);
    } while(reqRes[i++].next != NULL);

    return;
}

int checkResources(RESOURCE *reqRes) {
    // check if there are enough resource
    // return 0 as fail and 1 as correct
    if (reqRes == NULL) {
        return 1;
    }

    int i =0;
    
    do {
        // if remain resource is less than task need return 0 as fail
        if (findValueRes(resList[i].name) < resList[i].value) {
            return 0;
        }
    } while(reqRes[i++].next != NULL);

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

void readFile(char filename) {
    // function to read the file and set up the information
    FILE *filefp;
    filefp = fopen(filename, "r");
    int itask = 0;

    if (filefp == NULL) {
        printf("Fail to read file\n");
        exit(-1);
    }

    char line[200];
    while () {
        // read one line from the file
        if (strcmp(&line[0], "#")==0 || line[0] == '\0'||line[0] == '\r'||line[0] == '\n') {
            continue;
        } else {
            char *temp;
            temp = strtok(line, " ");
            if (strcmp(temp, "resources")==0) {
                setResList(line);
            } else if (strcmp(temp, "task")==0) {
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
                taskList[itask].reqRes = NULL;
                while ((temp = strtok(NULL, " "))!=NULL) {
                    char *name;
                    name = strtok(temp, ":")
                    strcpy(taskList[itask].reqRes[i].name, name);
                    name = strtok(NULL, ":");
                    taskList[itask].reqRes[i].value = atoi(name);
                    taskList[itask].reqRes[i].next = NULL;
                    if (i>0) {
                        taskList[itask].reqRes[i].next =  taskList[itask].reqRes[i-1];
                    }
                    i++;
                }
                itask++;
            }
        }
    }
    taskList[itask].name = NULL;
    return;
}

void setResList(char *line) {
    // function to read the line of resource
    char *temp;
    int i = 0;
    while ((temp = strtok(NULL, " "))!=NULL) {
        char *name;
        name = strtok(temp, ":")
        strcpy(resList[i].name, name);
        name = strtok(NULL, ":");
        resList[i].reqRes[i].value = atoi(name);
        resList[i].reqRes[i].next = NULL;
        if (i>0) {
            resList[i].next =  resList[i-1];
        }
        i++;
    }
    return;
}

int findValueRes(char key) {
    // find the value by the key like map or dictionary
    int i = 0;
    do {
        if (strcmp(key, resList[i].name)==0) {
            return resList[i].value;
        }
    } while(resList[i++].next != NULL);
    return -255;
}

void addValueRes(char key, int value) {
    // find the value by the key like map or dictionary and modify it
    int i = 0;
    do {
        if (strcmp(key, resList[i].name)==0) {
            resList[i].value + value;
        }
    } while(resList[i++].next != NULL);
    return;
}