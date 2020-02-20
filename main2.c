#include <mpi.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

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
int sendmsg[3];
int recvmsg[3];

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

void sendTo(int sendTid, int msg){
    if(msg==MSG_LOCKED){
        sendmsg[1] = lockedPriority;
    } else {
        sendmsg[1] = sendmsg[2];
    }
    sendmsg[2]++;
    MPI_Send( sendmsg, 3, MPI_INT, sendTid, msg, MPI_COMM_WORLD );
}

void recvAll(){
    MPI_Recv(recvmsg, 3, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status); 
    if(recvmsg[2]>sendmsg[2]){
        sendmsg[2] = recvmsg[2] + 1;
    } else {
        sendmsg[2]++;
    }
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
        if(queuePriority[i]<procPrioity){
            newQueue[newSizeQueue] = queue[i];
            newQueuePriority[newSizeQueue] = queuePriority[i];
            newSizeQueue++;
        } else if(!pushed) {
            pushed = true;
            
            newQueue[newSizeQueue] = procTid;
            newQueuePriority[newSizeQueue] = procPrioity;  
            newSizeQueue++;          
            newQueue[newSizeQueue] = queue[i];
            newQueuePriority[newSizeQueue] = queuePriority[i];
            newSizeQueue++;
        } else {
            newQueue[newSizeQueue] = queue[i];
            newQueuePriority[newSizeQueue] = queuePriority[i];
            newSizeQueue++;
        }
    }
    
    if(!pushed){
        newQueue[newSizeQueue] = procTid;
        newQueuePriority[newSizeQueue] = procPrioity;       
        newSizeQueue++;
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
    bool isInProc = false;
    for(int i=0;i<sizeMyProc; i++){
        if(myProc[i]==procTid)
            isInProc = true;
    }
    if(!isInProc){
        myProc[sizeMyProc]          = procTid;
        myProcPriority[sizeMyProc]   = procPrioity;
        sizeMyProc++;
    }
}

void removeFromMyProc(int procTid){
    int newMyProc[MAX_QUEUE];
    int newMyProcPriority[MAX_QUEUE];
    int newSizeMyProc = 0;

    for(int i=0;i<MAX_QUEUE;i++){
        newMyProc[i]=-1;
        newMyProcPriority[i]=-1;
    }
    
    for(int i=0;i<sizeMyProc;i++){
        if(myProc[i]!=procTid){
            newMyProc[newSizeMyProc]=myProc[i];
            newMyProcPriority[newSizeMyProc]=myProcPriority[i];
            newSizeMyProc++;
        }       
    }  
    
    for(int i=0;i<MAX_QUEUE;i++){
        myProc[i] = newMyProc[i];
        myProcPriority[i] = newMyProcPriority[i];
    }
    sizeMyProc = newSizeMyProc;
}

void addToInquires(int procTid, int procPrioity){
    bool isInInq = false;
    for(int i=0;i<sizeInquires; i++){
        if(inquires[i]==procTid)
            isInInq = true;
    }
    if(!isInInq){
        inquires[sizeInquires]          = procTid;
        inquiresPriority[sizeInquires]  = procPrioity;
        sizeInquires++;
    }
    
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
    if(sizeInquires==SIZE_GROUP)failedReceived=true;
    if(failedReceived && !canEnter()){
        for(int i=0; i<sizeInquires; i++){
            removeFromMyProc(inquires[i]);
            sendTo(inquires[i], MSG_RELINQUISH);
        }
        //failedReceived=false;
        clearInquires();
    }
}

void onRecv(){
    recvAll();
    switch(status.MPI_TAG){
        case MSG_REQUEST:
            if(DEBUG) printf("%d: Jest REQUEST od %d (%d)!!\n", tid, recvmsg[0], recvmsg[1]);	
            if(!locked){
                locked = true;
                lockedTid = recvmsg[0];
                lockedPriority = recvmsg[1];
                
                sendTo(recvmsg[0], MSG_LOCKED); 
            } else {
                addToQueue(recvmsg[0], recvmsg[1]);
                
                bool isMoreImportant = false;
                for(int i=0; i<sizeMyProc; i++) {
                    if(recvmsg[1] < myProcPriority[i]) {
                        isMoreImportant=true;
                    }
                }
                if(lockedPriority>recvmsg[1]){
                    isMoreImportant = true;
                }
                
                /*if(lockedPriority>recvmsg[1]){
                    isMoreImportant = true;
                } else if(queuePriority[0]!=recvmsg[1]){
                    isMoreImportant=true;
                }*/
                
                if(isMoreImportant){
                    sendTo(lockedTid, MSG_INQUIRE);
                } else {
                    sendTo(recvmsg[0], MSG_FAILED);
                }
            }
            break;
        case MSG_INQUIRE:
            if(DEBUG) printf("%d: Jest INQUIRE od %d (%d)!!\n", tid, recvmsg[0], recvmsg[1]);	
            addToInquires(recvmsg[0], recvmsg[1]);
            check();
            break;
        case MSG_FAILED:
            if(DEBUG) printf("%d: Jest FAILED od %d (%d)!!\n", tid, recvmsg[0], recvmsg[1]);	
            failedReceived = true;
            check();
            break;
        case MSG_LOCKED:
            if(DEBUG) printf("%d: Jest LOCKED od %d (%d)!!\n", tid, recvmsg[0], recvmsg[1]);
            addToMyProc(recvmsg[0], recvmsg[1]);
            break;
        case MSG_RELEASE: {
            if(DEBUG) printf("%d: Jest RELEASE od %d (%d)!!\n", tid, recvmsg[0], recvmsg[1]);                  
            locked = false;
            lockedTid = -1;
            lockedPriority = -1;
            if(sizeQueue>0){
                locked = true;
                lockedTid = queue[0];
                lockedPriority = queuePriority[0];
                sendTo(queue[0], MSG_LOCKED); 
                removeFirstFromQueue();
            }
            break;
        }
        case MSG_RELINQUISH:
            if(DEBUG) printf("%d: Jest RELINQUISH od %d (%d)!!\n", tid, recvmsg[0], recvmsg[1]);
            addToQueue(lockedTid, lockedPriority);
            if(sizeQueue>0){
                locked = true;
                lockedTid = queue[0];
                lockedPriority = queuePriority[0];
                sendTo(queue[0], MSG_LOCKED); 
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
            sendTo(i, MSG_REQUEST);
    }
    
    while(!canEnter()){
        onRecv();
    }
    
    printf("%d: ==== SEKCJA KRYTYCZNA ====  \n", tid);
    
    failedReceived = false;
    clearMyProc();
    sleep(2);
    for(int i=0;i<size;i++){
        if(i == myGroup[0] || i == myGroup[1] || i == myGroup[2])
            sendTo(i, MSG_RELEASE);
    }    
}

int k;
int grupy[255][255];
int ileRazyTab[255][2];
void bubblesort(int table[][2], int size2)
{
	int i, j;
	int temp[2];
	for (i = 0; i<size2-1; i++)
    {
		for (j=0; j<size2-1-i; j++)
		{
			if (table[j][1] < table[j+1][1])
			{
				temp[0] = table[j+1][0];
				temp[1] = table[j+1][1];
				table[j+1][0] = table[j][0];
				table[j+1][1] = table[j][1];
				table[j][0] = temp[0];
				table[j][1] = temp[1];
			}
		}
    }
}

int getLiczba(int without[], int withoutsize){
    int liczba=-1;
    for(int i=0;i<size;i++){
        bool jest = false;
        for(int j=0;j<withoutsize;j++){
            if(ileRazyTab[i][0]==without[j]){
                jest = true;
            }
            
        }
        if(!jest && ileRazyTab[i][1]>0){
            ileRazyTab[i][1]--;
            liczba = ileRazyTab[i][0];
            bubblesort(ileRazyTab, size);
            return liczba;
        }
    }
    printf("cos jest nie tak!");
    return -1;
}

void generateGroup(){
    for(int i=0;i<size;i++){
        ileRazyTab[i][0]=i;
        ileRazyTab[i][1]=k-1;
    }

    bubblesort(ileRazyTab, size);
    
    for(int i=0;i<size;i++){
        grupy[i][0] = i;
        for(int j=1;j<k;j++){
            grupy[i][j] = getLiczba(grupy[i], j);
        }
    }
    
}

int main(int argc, char **argv)
{
	MPI_Init(&argc, &argv); //Musi być w każdym programie na początku

	if(DEBUG) printf("Checking!\n");
	MPI_Comm_size( MPI_COMM_WORLD, &size );
	MPI_Comm_rank( MPI_COMM_WORLD, &tid );
	if(DEBUG) printf("My id is %d from %d\n",tid, size);

    
    float k2 = (1.0+sqrt(4.0*size-3.0))/2.0;
    k = k2;
    float roznica = k-k2;
    if(roznica!=0){
        printf("Nie można użyc dla tylu procesów dla algorytmu maekawa. Spróbuj np 3, 7, 13, 21, 31, 43, 57, 73, 91, 111 procesów.");
        return 0;
    }
    
    generateGroup();
    
    for(int i=0;i<size;i++){
        for(int j=0;j<k;j++){
           printf("%d ", grupy[i][j] );
        }
        printf("\n");
    }
    
    myGroup[0] = grupy[tid][0];
    myGroup[1] = grupy[tid][1];
    myGroup[2] = grupy[tid][2];
        
    
	sendmsg[0] = tid;
	sendmsg[1] = tid;
	sendmsg[2] = tid;
    
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
