#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

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
    struct task* result=temp->data;
    free(temp);
    return result;
}




void parseLine(char* line)
{
    
    
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
    int numCPU=atoi(argv[1]);
    if(readTasks())
    {
        //For sjf policy
        if(strcmp(argv[2],"sjf")==0)
        {
            
        }
        //For mlfq policy
        if(strcmp(argv[1],"mlfq")==0)
        {
            
        }
    }
    else
    {
        printf("\nCould not read the file tasks.txt\n");
    }
    return EXIT_SUCCESS;
}
