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

//////////////////////////////////
////Data Structures
//////////////////////////////////
struct TASK
{
    char* name;
    int type;
    int length;
    int odds_of_IO;
}task;

struct NODE
{
    struct task* data;
    struct NODE* next;
}Node;

struct QUEUE
{
    struct Node* front;
    struct Node* end;
    int size;
}Queue;

////Global variables
struct Queue* sjfQ;
struct Queue* mlfqP1;
struct Queue* mlfqP2;
struct Queue* mlfqP3;
int sjf;
int numCPU;

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
struct Queue* createQueue()
{
    struct Queue* newQ=(struct Queue*)malloc(sizeof(struct Queue));
    newQ->front=NULL;
    newQ->end=NULL;
    newQ->size=0;
    return newQ;
}

//Create a new Node
struct Node* createNode(struct task* nTask)
{
    struct Node* newNode=(struct Node*)malloc(sizeof(struct Node));
    newNode->data=nTask;
    newNode->next=NULL;
    return newNode;
}

//Add an element to the queue
void enqueue(struct Queue* myQ, struct task* nTask)
{
    struct Node* newNode=createNode(nTask);
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
void sortEnqueue(struct Queue* myQ, struct task* nTask)
{
    struct Node* newNode=createNode(nTask);
    //If the queue is empty
    if(myQ->size==0)
    {
        myQ->front=newNode;
        myQ->end=newNode;
    }
    //If the new Node is smaller than first element of the queue
    else if(myQ->front->data->length > nTask->length)
    {
        myQ->front=newNode;
        myQ->end=newNode;
    }
    else
    {
        struct Node* curr=myQ->front;
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
struct task* dequeue(struct Queue* myQ)
{
    if(myQ->size==0)
    {
        printf("\nQueue is Empty!\n");
        return NULL;
    }
    struct Node* temp=myQ->front;
    myQ->front=myQ->front->next;
    myQ->size--;
    struct task* result=temp->data;
    free(temp);
    return result;
}

//////////////////////////////////
////Fucntions used for sjf scheduler
//////////////////////////////////

void* sjfScheduler(void* args)
{
    
}

//////////////////////////////////
////Fucntions for reading the tasks
//////////////////////////////////

//This fucntion is used to create a task
struct task* createTask(char* name, int type, int length, int odds_of_IO)
{
    struct task* result=(struct task*)malloc(sizeof(struct task));
    result->name=name;
    result->type=type;
    result->length=length;
    result->odds_of_IO=odds_of_IO;
    return result;
}

//Break down a line and create a task object to add to the appropriate queue
void parseLine(char* line)
{
    char* name=strtok(line," ");
    int type=atoi(strtok(NULL," "));
    int length=atoi(strtok(NULL," "));
    int odds=atoi(strtok(NULL," "));
    struct task* newTask=createTask(name,type,length,odds);
    if(sjf==1)
    {
        sortEnqueue(sjfQ, newTask);
    }
    else
    {
        enqueue(mlfqP1, newTask);
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
            //check for new line character
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

int main(int argc, char* argv)
{
    assert(argc>0);
    numCPU=atoi(argv[1]);
    
    //For sjf policy
    if(strcmp(argv[2],"sjf")==0)
    {
        sjf=1;
        sjfQ=createQueue();
    }
    //For mlfq policy
    if(strcmp(argv[2],"mlfq")==0)
    {
        sjf=0;
        mlfqP1=createQueue();
        mlfqP2=createQueue();
        mlfqP3=createQueue();
    }
    
    if(readTasks())
    {
        
    }
    else
    {
        printf("\nCould not read the file tasks.txt\n");
    }
    
    pthread_t threads[numCPU];
    int i;
    for(i=0;i<numCPU;i++)
    {
        if(pthread_create(&threads[i],NULL,sjfScheduler,NULL))
        {
            printf("\nFailed to make threads.\n");
        }
    }
    return EXIT_SUCCESS;
}
