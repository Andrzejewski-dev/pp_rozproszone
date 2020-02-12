#include <mpi.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#define ROOT 0
#define MAX_QUEUE 32
#define SIZE_GROUP 3
#define DEBUG true

#define MSG_REQUEST 101
#define MSG_LOCKED 102
#define MSG_RELEASE 103
#define MSG_FAILED 104
#define MSG_INQUIRE 105
#define MSG_RELINQUISH 106

int tid,size;
MPI_Status status;

int myGroup[SIZE_GROUP];
int sendmsg[2];
int recvmsg[2];

bool locked = false;
int lockedTid;
int lockedPriority;
int myProc[MAX_QUEUE];
int myProcPriority[MAX_QUEUE];
int sizeMyProc = 0;
int queue[MAX_QUEUE];
int queuePriority[MAX_QUEUE];
int sizeQueue = 0;
int inquires[MAX_QUEUE];
int inquiresPriority[MAX_QUEUE];
int sizeInquires = 0;

bool failedReceived = false;
     
bool canEnter(){
    return sizeMyProc == SIZE_GROUP;
}

void addToQueue(int procTid, int procPrioity){
    bool pushed = false;
      
    int newQueue[MAX_QUEUE];
    int newQueuePriority[MAX_QUEUE];
    int newSizeQueue = 0;

    for(int i=0;i<MAX_QUEUE;i++){
        newQueue[i]=-1;
        newQueuePriority[i]=-1;
    }
                            
    for(int i=0; i<sizeQueue; i++){
        if(queuePriority[sizeQueue]<procPrioity){
            newQueue[i] = queue[i];
            newQueuePriority[i] = queuePriority[i];
            newSizeQueue++;
        } else if(!pushed) {
            pushed=true;
            
            newQueue[i] = procTid;
            newQueuePriority[i] = procPrioity;            
            newQueue[i+1] = queue[i];
            newQueuePriority[i+1] = queuePriority[i];
            newSizeQueue+=2;
        } else {                  
            newQueue[i+1] = queue[i];
            newQueuePriority[i+1] = queuePriority[i];
            newSizeQueue++;
        }
    }
    
    if(newSizeQueue==0){
        newQueue[0] = procTid;
        newQueuePriority[0] = procPrioity;       
        newSizeQueue=1;
    }
    
    for(int i=0;i<MAX_QUEUE;i++){
        queue[i] = newQueue[i];
        queuePriority[i] = newQueuePriority[i];
    }
    sizeQueue = newSizeQueue;
}

void removeFirstFromQueue(){
    for(int i=0;i<MAX_QUEUE-1;i++){
        queue[i]=queue[i+1];
        queuePriority[i]=queuePriority[i+1];
    }
    sizeQueue--;
    if(sizeQueue<0)sizeQueue=0;
}


void addToMyProc(int procTid, int procPrioity){
    myProc[sizeMyProc]          = procTid;
    myProcPriority[sizeMyProc]   = procPrioity;
    sizeMyProc++;
}

void addToInquires(int procTid, int procPrioity){
    inquires[sizeInquires]          = procTid;
    inquiresPriority[sizeInquires]  = procPrioity;
    sizeInquires++;
}

void clearInquires(){
    for(int i=0;i<MAX_QUEUE;i++){
        inquires[i] = -1;
        inquiresPriority[i] = -1;
    }
    sizeInquires=0;
}

void clearMyProc(){
    for(int i=0;i<MAX_QUEUE;i++){
        myProc[i] = -1;
        myProcPriority[i] = -1;
    }
    sizeMyProc=0;
}

void check(){
    if(failedReceived && canEnter() == false){
        locked = false;
        lockedTid=-1;//nie wiem czy git
        for(int i=0; i<sizeInquires; i++){
            
            MPI_Send( sendmsg, 2, MPI_INT, inquires[i], MSG_RELINQUISH, MPI_COMM_WORLD );
        }
        clearInquires();
    }
}

void onRecv(){
    MPI_Recv(recvmsg, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status); 
      
    switch(status.MPI_TAG){
        case MSG_REQUEST:
            if(DEBUG) printf("%d: Jest REQUEST od %d !!\n", tid, recvmsg[0]);	
            if(!locked){
                locked = true;
                lockedTid = recvmsg[0];
                lockedPriority = recvmsg[1];
                
                MPI_Send( sendmsg, 2, MPI_INT, recvmsg[0], MSG_LOCKED, MPI_COMM_WORLD );
            } else {
                addToQueue(recvmsg[0], recvmsg[1]);
                /*for(int i=0;i<sizeQueue;i++){
                    printf("%d: [%d] =  %d \n", tid, i, queue[i]);
                }
                break;*/
                if(lockedPriority>recvmsg[1] && queuePriority[0]==recvmsg[1]){
                     MPI_Send( sendmsg, 2, MPI_INT, lockedTid, MSG_INQUIRE, MPI_COMM_WORLD );
                } else {
                    MPI_Send( sendmsg, 2, MPI_INT, recvmsg[0], MSG_FAILED, MPI_COMM_WORLD );
                     
                }  
            }
            break;
        case MSG_INQUIRE:
            if(DEBUG) printf("%d: Jest INQUIRE od %d!!\n", tid, recvmsg[0]);	
            addToInquires(recvmsg[0], recvmsg[1]);
            check();
            break;
        case MSG_FAILED:
            if(DEBUG) printf("%d: Jest FAILED od %d!!\n", tid, recvmsg[0]);	
            failedReceived = true;
            check();
            break;
        case MSG_LOCKED:
            if(DEBUG) printf("%d: Jest LOCKED od %d !!\n", tid, recvmsg[0]);	
            addToMyProc(recvmsg[0], recvmsg[1]);
            break;
        case MSG_RELEASE: {
            if(DEBUG) printf("%d: Jest RELEASE od %d!!\n", tid, recvmsg[0]);                  
            locked = false;
            lockedTid = -1;
            lockedPriority = -1;
            if(sizeQueue>0){
                locked = true;
                lockedTid = queue[0];
                lockedPriority = queuePriority[0];
                MPI_Send( sendmsg, 2, MPI_INT, queue[0], MSG_LOCKED, MPI_COMM_WORLD );
                removeFirstFromQueue();                
            }     
            break;
        }               
        case MSG_RELINQUISH:
            if(DEBUG) printf("%d: Jest RELINQUISH od %d!!\n", tid, recvmsg[0]);	
            addToQueue(lockedTid, lockedPriority);
            if(sizeQueue>0){
                locked = true;
                lockedTid = queue[0];
                lockedPriority = queuePriority[0];
                MPI_Send( sendmsg, 2, MPI_INT, queue[0], MSG_LOCKED, MPI_COMM_WORLD );
                removeFirstFromQueue();                
            } else {
                locked = false;
                lockedTid = -1;
                lockedPriority = -1;
            }
            break;
    }
}

void enterCriticalSection(){
    for(int i=0;i<size;i++){
        if(i == myGroup[0] || i == myGroup[1] || i == myGroup[2])
            MPI_Send( sendmsg, 2, MPI_INT, i, MSG_REQUEST, MPI_COMM_WORLD );
    }
    
    while(!canEnter()){
        onRecv();
        sendmsg[1]++;
    }
    
    printf("%d: SEKCJA KRYTYCZNA! ! ! ! \n", tid);
    
    failedReceived = false;
    clearMyProc();
    for(int i=0;i<size;i++){
        if(i == myGroup[0] || i == myGroup[1] || i == myGroup[2])
            MPI_Send( sendmsg, 2, MPI_INT, i, MSG_RELEASE, MPI_COMM_WORLD );
    }    
}


int main(int argc, char **argv)
{
	MPI_Init(&argc, &argv); //Musi być w każdym programie na początku

	printf("Checking!\n");
	MPI_Comm_size( MPI_COMM_WORLD, &size );
	MPI_Comm_rank( MPI_COMM_WORLD, &tid );
	printf("My id is %d from %d\n",tid, size);

   
    myGroup[0] = tid; // ja
    
    if(tid+1<size)
        myGroup[1] = tid+1; // nastepny
    else
        myGroup[1] = 0;
    
    if(tid-1>=0)
        myGroup[2] = tid-1; // poprzedni
    else
        myGroup[2] = size-1;
    
    
	sendmsg[0] = tid;
	sendmsg[1] = tid;
    
    for(int i=0;i<MAX_QUEUE;i++){
        myProc[i]=-1;
        myProcPriority[i]=-1;
        queue[i]=-1;
        queuePriority[i]=-1;
        inquires[i] = -1;
        inquiresPriority[i] = -1;
    }
        
    while(true)
        enterCriticalSection();
	
    
    

	MPI_Finalize(); // Musi być w każdym programie na końcu
}
