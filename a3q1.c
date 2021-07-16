#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

#define NANOS_PER_USEC 1000
#define USEC_PER_SEC   1000000
#define MAX_CHAR 100//Maximum characters of a single task
#define TASK_LIMIT 100//The file will not have more than 100 tasks
#define TIME_SLICE 50
#define TIME_ALLOTMENT 200

//////////////////////////////////
////Data Structures
//////////////////////////////////
struct TASK
{
    char* name;
    int type;
    int length;
    int odds_of_IO;
    struct timespec start;
    struct timespec end;
    struct timespec firstAccess;
    int accessed;
    int runTime;
};
typedef struct TASK task;
struct NODE
{
    task* data;
    struct NODE* next;
};
typedef struct NODE Node;
struct QUEUE
{
    Node* front;
    Node* end;
    int size;
};

////Global Variables//////

typedef struct QUEUE Queue;
task* doneTasks[TASK_LIMIT];
int doneCount=0;
int sjf;
int numCPU;
int sjfTasksDone=0;
int taskAvailable=0;
int printed=0;
///////////////////////
Queue* sjfQ;
pthread_mutex_t sjfMutex;
pthread_mutex_t m0;
pthread_mutex_t m1;
pthread_mutex_t m2;
pthread_cond_t sjfCond;
///////////////////////
Queue* mlfq1;
Queue* mlfq2;
Queue* mlfq3;
pthread_mutex_t mlfqMutex;
pthread_cond_t mlfqCond;
int mlfqTasksDone=0;
struct timespec timeCheck;


void printReport();
//Given function for working
static void microsleep(unsigned int usecs)
{
    long seconds = usecs / USEC_PER_SEC;
    long nanos   = (usecs % USEC_PER_SEC) * NANOS_PER_USEC;
    struct timespec t = { .tv_sec = seconds, .tv_nsec = nanos };
    int ret;
    do
    {
        ret = nanosleep( &t, &t );
        // need to loop, `nanosleep` might return before sleeping
        // for the complete time (see `man nanosleep` for details)
    } while (ret == -1 && (t.tv_sec || t.tv_nsec));
}

//Copied from Guy Rutenbergs blog post
//This function is to calculate the difference in 2 different timespec structs
struct timespec diff(struct timespec start,struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

//////////////////////////////////
////Helper functions of Queue
//////////////////////////////////

//Create a queue
Queue* createQueue()
{
    Queue* newQ=(Queue*)malloc(sizeof(Queue));
    newQ->front=NULL;
    newQ->end=NULL;
    newQ->size=0;
    return newQ;
}

//Create a new Node
Node* createNode(task* nTask)
{
    Node* newNode=(Node*)malloc(sizeof(Node));
    newNode->data=nTask;
    newNode->next=NULL;
    return newNode;
}

//Add an element to the queue
void enqueue(Queue* myQ,task* nTask)
{
    Node* newNode=createNode(nTask);
    if(myQ->size==0)
    {
        myQ->front=newNode;
        myQ->end=newNode;
    }
    else
    {
        myQ->end->next=newNode;
        myQ->end=newNode;
    }
    myQ->size++;
}

//Add an element to the queue sorted by the length of the job
void sortEnqueue(Queue* myQ,task* nTask)
{
    Node* newNode=createNode(nTask);
    //If the queue is empty
    if(myQ->size==0)
    {
        myQ->front=newNode;
        myQ->end=newNode;
    }
    //If the new Node is smaller than first element of the queue
    else if(myQ->front->data->length > nTask->length)
    {
        newNode->next=myQ->front;
        myQ->front=newNode;
    }
    else
    {
        Node* curr=myQ->front;
        int found=1;
        //Find the position
        while(curr->next !=NULL && curr->next->data->length < nTask->length)
        {
            curr=curr->next;
            if(curr->next ==NULL)
            {
                found=0;
            }
        }
        //If it is the last element
        if(found==0)
        {
            myQ->end->next=newNode;
            myQ->end=newNode;
        }
        //If it is in the middle of the queue
        else
        {
            newNode->next=curr->next;
            curr->next=newNode;
        }
        
    }
    myQ->size++;
}

//Remove an element from the queue and return it
task* dequeue(Queue* myQ)
{
    task* result=NULL;
    if(myQ->size==0)
    {
        //Do nothing Queue is Empty
    }
    else
    {
        if(myQ->front->next!=NULL)
        {
            Node* temp=myQ->front;
            myQ->front=myQ->front->next;
            result=temp->data;
            myQ->size--;
        }
        else
        {
            result=myQ->front->data;
            myQ->size=0;
        }
    }
    return result;
}
//////////////////////////////////
////Fucntions used for sjf scheduler
//////////////////////////////////

//This function executes the current task and reschedules the task or sends it to the done area
void run_sjf_task(task* currTask)
{
    int io_result;
    int pre_io;
    int workTime;
    srand(time(0));
    //Check if the task is getting its first access
    if(currTask->accessed==0)
    {
        //Record the time of first access
        clock_gettime(CLOCK_REALTIME, &(currTask->firstAccess));
        currTask->accessed=1;
    }
    //Check if the task will do I/O
    io_result=rand() % 101;
    //If the task is not doing I/O
    if(io_result > currTask->odds_of_IO)
    {
        //amount of time the task should work
        workTime=currTask->length;
        currTask->length=0;
    }
    //If it is doing I/O
    else
    {
        pre_io=rand() % (TIME_SLICE+1);
        //work for the pre_io duration or length of the task (which ever is lower)
        if(pre_io >= currTask->length)
        {
            workTime=currTask->length;
            currTask->length=0;
        }
        else
        {
            workTime=pre_io;
            currTask->length= currTask->length - pre_io;
        }
    }
    //Work for the time calculated
    microsleep(workTime);
    pthread_mutex_lock(&m0);
    if(currTask->length==0)
    {
        clock_gettime(CLOCK_REALTIME, &(currTask->end));
        doneTasks[doneCount]=currTask;
        doneCount++;
    }
    else
    {
        sortEnqueue(sjfQ,currTask);
        sjfTasksDone--;
    }
    pthread_mutex_unlock(&m0);
}

//This function returns the next task that needs to be run
task* sjfScheduler()
{
    task* result=NULL;
    pthread_mutex_lock(&m1);
    if(taskAvailable==0)
    {
        taskAvailable=1;
        pthread_cond_signal(&sjfCond);
    }
    else
    {
        if(sjfTasksDone<TASK_LIMIT)
        {
            result=dequeue(sjfQ);
        }
    }
    pthread_mutex_unlock(&m1);
    return result;
}

//This fucntion gets the next task from the scheduler and executes it
void* sjfDispatcher(void* argc)
{
    task* sjfTask;
   // pthread_mutex_lock(&sjfMutex);
    while(sjfTasksDone<TASK_LIMIT)
    {
        pthread_mutex_lock(&sjfMutex);
        while(taskAvailable==0)
        {
            pthread_cond_wait(&sjfCond,&sjfMutex);
        }
        sjfTasksDone++;
        if(sjfTasksDone==TASK_LIMIT)
        {
            printReport();
        }
        sjfTask=sjfScheduler();
        if(sjfTask!=NULL)
        {
            run_sjf_task(sjfTask);
        }
       pthread_mutex_unlock(&sjfMutex);
    }
    //pthread_mutex_unlock(&sjfMutex);
    return argc;
}



//////////////////////////////////
////Fucntions used for mlfq scheduler
//////////////////////////////////

//This function is used to change the priority of all the tasks to high priority
void changePriority()
{
    task* currTask;
    //Check the medium priority queue
    if(mlfq2->size != 0)
    {
        //Put everything from medium queue to high priority queue
        currTask=dequeue(mlfq2);
        while(currTask!=NULL)
        {
            currTask->runTime=0;
            enqueue(mlfq1,currTask);
            currTask=dequeue(mlfq2);
        }
    }
    //Check the low priority queue
    if(mlfq3->size != 0)
    {
        //Put everything from low queue to high priority queue
        currTask=dequeue(mlfq3);
        while(currTask!=NULL)
        {
            currTask->runTime=0;
            enqueue(mlfq1,currTask);
            currTask=dequeue(mlfq3);
        }
    }
}

//This fucntion is used to check if the program has been running for 5000 usecs and change the priority of the tasks if it did
void timeUpdate(struct timespec initTime)
{
    struct timespec currTime;
    struct timespec difference;
    int microsec;
    clock_gettime(CLOCK_REALTIME,&currTime);
    difference=diff(initTime,currTime);
    microsec=(difference.tv_sec/1000000)+(difference.tv_nsec*0.001);
    if(microsec >= 5000)
    {
        changePriority();
        clock_gettime(CLOCK_REALTIME, &timeCheck);
    }
    
}
//This fucntion is used to execute the task and reschedule it
void run_mlfq_task(task* currTask)
{
    int io_result;
    int pre_io;
    int workTime;
    srand(time(0));
    //Check if the task is getting its first access
    if(currTask->accessed==0)
    {
        //Record the time of first access
        clock_gettime(CLOCK_REALTIME, &(currTask->firstAccess));
        currTask->accessed=1;
    }
    //Check if the task will do I/O
    io_result=rand() % 101;
    //If the task is not doing I/O
    if(io_result > currTask->odds_of_IO)
    {
        //If the length of the task is smaller than the time slice
        if(TIME_SLICE >= currTask->length)
        {
            workTime=currTask->length;
            currTask->length=0;
        }
        else
        {
            //Run the task according to the time slice
            workTime=TIME_SLICE;
            currTask->length=currTask->length - TIME_SLICE;
        }
    }
    //If it is doing I/O
    else
    {
        pre_io=rand() % (TIME_SLICE+1);
        //work for the pre_io duration or length of the task (which ever is lower)
        if(pre_io >= currTask->length)
        {
            workTime=currTask->length;
            currTask->length=0;
        }
        else
        {
            workTime=pre_io;
            currTask->length= currTask->length - pre_io;
        }
    }
    currTask->runTime+=workTime;
    microsleep(workTime);
    pthread_mutex_lock(&m1);
    //If the task is finished put it in the done area
    if(currTask->length==0)
    {
        clock_gettime(CLOCK_REALTIME, &(currTask->end));
        doneTasks[doneCount]=currTask;
        doneCount++;
    }
    //Reschedule
    else
    {
        //If the priority needs to be changed to medium
        if(currTask->runTime > TIME_ALLOTMENT)
        {
            //if the priority needs to be changed to low
            if(currTask->runTime > (2 * TIME_ALLOTMENT))
            {
                enqueue(mlfq3,currTask);
            }
            else
            {
                enqueue(mlfq2,currTask);
            }
        }
        else
        {
            enqueue(mlfq1,currTask);
        }
        mlfqTasksDone--;
    }
    pthread_mutex_unlock(&m1);
    
}

//This function returns the next task that needs to be executed
task* mlfqScheduler()
{
    task* result=NULL;
    pthread_mutex_lock(&m0);
    if(taskAvailable==0)
    {
        taskAvailable=1;
        pthread_cond_signal(&mlfqCond);
    }
    else
    {
        //Check if 5000 usecs passed
        timeUpdate(timeCheck);
        //If there is no task in queue 1 (most priority)
        if(mlfq1->size==0)
        {
            //If there is no task in queue 2 (medium priority)
            if(mlfq2->size==0)
            {
                //Get a task from mlfq3(least priority)
                {
                    result=dequeue(mlfq3);
                }
            }
            //Get a task from mlfq2 (medium priority)
            else
            {
                result=dequeue(mlfq2);
            }
        }
        //Get a task from mlfq1 (most priority)
        else
        {
            result=dequeue(mlfq1);
        }
        
    }
    pthread_mutex_unlock(&m0);
    return result;
}
//This function gets the next task from the scheduler and executes it
void* mlfqDispatcher(void* argc)
{
    task* curr;
    //Change with doneCount
    while(mlfqTasksDone<TASK_LIMIT)
    {
        pthread_mutex_lock(&mlfqMutex);
        while(taskAvailable==0)
        {
            pthread_cond_wait(&mlfqCond,&mlfqMutex);
        }
        mlfqTasksDone++;
        curr=mlfqScheduler();
        if(curr!=NULL)
        {
            run_mlfq_task(curr);
        }
        if(mlfqTasksDone==TASK_LIMIT)
        {
            printReport();
        }
        pthread_mutex_unlock(&mlfqMutex);
    }
    return argc;
}

//////////////////////////////////
////Fucntions for reading and printing
//////////////////////////////////

//This fucntion is used to create a task
task* createTask(char* name, int type, int length, int odds_of_IO)
{
    task* result=(task*)malloc(sizeof(task));
    result->name=name;
    result->type=type;
    result->length=length;
    result->odds_of_IO=odds_of_IO;
    result->accessed=0;
    result->runTime=0;
    //Get the arrival time
    clock_gettime(CLOCK_REALTIME, &(result->start));
    return result;
}

//Break down a line and create a task object to add to the appropriate queue
void parseLine(char* line)
{
    char* name=strdup(strtok(line," "));
    int type=atoi(strtok(NULL," "));
    int length=atoi(strtok(NULL," "));
    int odds=atoi(strtok(NULL," "));
    task* newTask=createTask(name,type,length,odds);
    if(sjf==1)
    {
        //Use the sorted enqueue function to add the new task to the queue
        sortEnqueue(sjfQ, newTask);
    }
    else
    {
        //All new tasks have the highest priority so they go into mlfq1
        enqueue(mlfq1, newTask);
    }
}


//Read the tasks.txt file
int readTasks()
{
    int result=0;
    FILE *fp=fopen("tasks.txt","r");
    char line[MAX_CHAR];
    //Check if the file exists
    if(fp==NULL)
    {
        printf("\nFile tasks.txt does not exist\n");
    }
    else
    {
        while(fgets(line,MAX_CHAR,fp)!=NULL)
        {
            //check for new line character and remove it
            if(line[strlen(line)-1]=='\n')
            {
                line[strlen(line)-1]='\0';
            }
            parseLine(line);
        }
        //Close the file
        fclose(fp);
        result=1;
    }
    return result;
}


//Print the report
void printReport()
{
    printed=1;
    int tt_typ0=0,tt_typ1=0,tt_typ2=0,tt_typ3=0;
    int rr_typ0=0,rr_typ1=0,rr_typ2=0,rr_typ3=0;
    int type0=0,type1=0,type2=0,type3=0;
    struct timespec difference;
    if(sjf==1)
    {
        printf("\nUsing sjf with %d CPUs.\n",numCPU);
    }
    else
    {
        printf("\nUsing mlfq with %d CPUs.\n",numCPU);
    }
    int i;
    for(i=0;i<doneCount;i++)
    {
        if(doneTasks[i]->type==0)
        {
            difference=diff(doneTasks[i]->start,doneTasks[i]->end);
            tt_typ0+=(difference.tv_sec/1000000)+(difference.tv_nsec*0.001);
            difference=diff(doneTasks[i]->start,doneTasks[i]->firstAccess);
            rr_typ0+=(difference.tv_sec/1000000)+(difference.tv_nsec*0.001);
            type0++;
        }
        else if(doneTasks[i]->type==1)
        {
            difference=diff(doneTasks[i]->start,doneTasks[i]->end);
            tt_typ1+=(difference.tv_sec/1000000)+(difference.tv_nsec*0.001);
            difference=diff(doneTasks[i]->start,doneTasks[i]->firstAccess);
            rr_typ1+=(difference.tv_sec/1000000)+(difference.tv_nsec*0.001);
            type1++;
        }
        else if(doneTasks[i]->type==2)
        {
            difference=diff(doneTasks[i]->start,doneTasks[i]->end);
            tt_typ2+=(difference.tv_sec/1000000)+(difference.tv_nsec*0.001);
            difference=diff(doneTasks[i]->start,doneTasks[i]->firstAccess);
            rr_typ2+=(difference.tv_sec/1000000)+(difference.tv_nsec*0.001);
            type2++;
        }
        else
        {
            difference=diff(doneTasks[i]->start,doneTasks[i]->end);
            tt_typ3+=(difference.tv_sec/1000000)+(difference.tv_nsec*0.001);
            difference=diff(doneTasks[i]->start,doneTasks[i]->firstAccess);
            rr_typ3+=(difference.tv_sec/1000000)+(difference.tv_nsec*0.001);
            type3++;
        }
    }
    printf("\nAverage Turnaround time: \n");
    printf("\n\tType 0: %d usec\n",tt_typ0/type0);
    printf("\tType 1: %d usec\n",tt_typ1/type1);
    printf("\tType 2: %d usec\n",tt_typ2/type2);
    printf("\tType 3: %d usec\n",tt_typ3/type3);
    printf("\nAverage Response time: \n");
    printf("\n\tType 0: %d usec\n",rr_typ0/type0);
    printf("\tType 1: %d usec\n",rr_typ1/type1);
    printf("\tType 2: %d usec\n",rr_typ2/type2);
    printf("\tType 3: %d usec\n",rr_typ3/type3);
}

int main(int argc, char* argv[])
{
    assert(argc>0);
    int i;
    numCPU=atoi(argv[1]);
    //If we have to do sjf policy, initialize the sjf Queue
    if(strcmp(argv[2],"sjf")==0)
    {
        sjf=1;
        sjfQ=createQueue();
    }
    //Else initialize all the queues needed for mlfq policy
    else
    {
        mlfq1=createQueue();
        mlfq2=createQueue();
        mlfq3=createQueue();
    }
    if(readTasks()==1)
    {
        //Get the number of threads needed
        pthread_t threads[numCPU];
        //If we are running the sjf policy
        if(sjf==1)
        {
            //Initialize thread, mutex and condition
            pthread_mutex_init(&sjfMutex,NULL);
            pthread_mutex_init(&m0,NULL);
            pthread_mutex_init(&m1,NULL);
            pthread_cond_init(&sjfCond,NULL);
            for(i=0;i<numCPU;i++)
            {
                if(pthread_create(&threads[i],NULL,&sjfDispatcher,NULL)!=0)
                {
                    printf("\nFailed to make threads.\n");
                }
            }
            //Start
            sjfScheduler();
        }
        //Running the mlfq policy
        else
        {
            pthread_mutex_init(&mlfqMutex,NULL);
            pthread_mutex_init(&m0,NULL);
            pthread_mutex_init(&m1,NULL);
            pthread_cond_init(&mlfqCond,NULL);
            for(i=0;i<numCPU;i++)
            {
                if(pthread_create(&threads[i],NULL,&mlfqDispatcher,NULL)!=0)
                {
                    printf("\nFailed to make threads.\n");
                }
            }
            //Get the time when the scheduler is starting
            clock_gettime(CLOCK_REALTIME, &timeCheck);
            //Start
            mlfqScheduler();
        }
        
        //Join the threads
        for(i=0;i<numCPU;i++)
        {
            if(pthread_join(threads[i],NULL)!=0)
            {
                printf("\nThreads failed to join.\n");
            }
        }
        
    }
    //Destroy the mutex's used
    if(sjf==1)
    {
        pthread_mutex_destroy(&sjfMutex);
        pthread_mutex_destroy(&m0);
        pthread_mutex_destroy(&m1);
        pthread_cond_destroy(&sjfCond);
    }
    else
    {
        pthread_mutex_destroy(&mlfqMutex);
        pthread_mutex_destroy(&m0);
        pthread_mutex_destroy(&m1);
        pthread_cond_destroy(&mlfqCond);
    }
    //If the report is not printed already
    if(printed==0)
    {
       printReport();
    }
    return EXIT_SUCCESS;
}
