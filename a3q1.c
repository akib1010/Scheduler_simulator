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
typedef struct QUEUE Queue;
Queue* sjfQ;
task* doneTasks[TASK_LIMIT];
int doneCount=0;
int sjf;
int numCPU;
int numTasks=0;
int taskAvailable=0;
task* sjfTask;
pthread_mutex_t sjfMutex;
pthread_mutex_t m0;
pthread_mutex_t m1;
pthread_mutex_t m2;
pthread_cond_t sjfCond;

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
        printf("\nQueue is Empty!\n");
    }
    else
    {
        Node* temp=myQ->front;
        if(myQ->front!=NULL)
        {
            myQ->front=myQ->front->next;
            myQ->size--;
            result=temp->data;
        }
    }
    return result;
}
//////////////////////////////////
////Fucntions used for sjf scheduler
//////////////////////////////////
void run_sjf_task(task* currTask)
{
//    pthread_mutex_lock(&m0);
//    printf("\nName: %s , Length: %d ---- Name: %s, Length: %d",sjfTask->name,sjfTask->length,currTask->name,currTask->length);
//    pthread_mutex_unlock(&m0);
//    printf("\nName: %s, Length: %d",sjfTask->name,sjfTask->length);
    int io_result;
    int pre_io;
    int workTime;
    srand(time(0));
    pthread_mutex_lock(&m0);
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
            currTask->length=currTask->length - pre_io;
        }
    }
    pthread_mutex_unlock(&m0);
    microsleep(currTask->length);
    pthread_mutex_lock(&m2);
    if(currTask->length==0)
    {
        doneTasks[doneCount]=currTask;
        doneCount++;
    }
    else
    {
        sortEnqueue(sjfQ,currTask);
        numTasks--;
    }
    pthread_mutex_unlock(&m2);
}

void sjfScheduler()
{
    pthread_mutex_lock(&m1);
    if(doneCount<TASK_LIMIT)
    {
        taskAvailable=1;
        sjfTask=dequeue(sjfQ);
        pthread_cond_signal(&sjfCond);
    }
    pthread_mutex_unlock(&m1);
}

void* sjfDispatcher(void* argc)
{
    while(numTasks<TASK_LIMIT-1)
    {
        pthread_mutex_lock(&sjfMutex);
        while(taskAvailable==0)
        {
            pthread_cond_wait(&sjfCond,&sjfMutex);
        }
        numTasks++;
        run_sjf_task(sjfTask);
        pthread_mutex_unlock(&sjfMutex);
        sjfScheduler();
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
        sortEnqueue(sjfQ, newTask);
    }
    else
    {
        
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

//Print the report
void printReport()
{
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
    if(strcmp(argv[2],"sjf")==0)
    {
        sjf=1;
        sjfQ=createQueue();
    }
    //////////
    //////////
    if(readTasks()==1)
    {
        //Get the number of threads needed
        pthread_t threads[numCPU];
        if(sjf==1)
        {
            //Initialize thread, mutex and condition
            pthread_mutex_init(&sjfMutex,NULL);
            pthread_mutex_init(&m0,NULL);
            pthread_mutex_init(&m1,NULL);
            pthread_mutex_init(&m2,NULL);
            pthread_cond_init(&sjfCond,NULL);
            for(i=0;i<numCPU;i++)
            {
                if(pthread_create(&threads[i],NULL,&sjfDispatcher,NULL)!=0)
                {
                    printf("\nFailed to make threads.\n");
                }
            }
            sjfScheduler();
            for(i=0;i<numCPU;i++)
            {
                if(pthread_join(threads[i],NULL)!=0)
                {
                    printf("\nThreads failed to join.\n");
                }
            }
        }
        
    }
    printReport();
    return EXIT_SUCCESS;
}

//            task* curr=dequeue(sjfQ);
//            int count=0;
//            while(curr!=NULL)
//            {
//                printf("Name: %s, type: %d, length: %d, odds: %d\n",curr->name,curr->type,curr->length,curr->odds_of_IO);
//                if(count==0)
//                {
//                    task* temp=createTask("Test", 4, 20, 50);
//                    sortEnqueue(sjfQ,temp);
//                }
//                curr=dequeue(sjfQ);
//                count++;
//            }
//            printf("Total NumTasks: %d\n",numTasks);
//            printf("Queue Size: %d\n",sjfQ->size);
