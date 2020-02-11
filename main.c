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

int main(int argc, char **argv)
{
	int tid,size;
	MPI_Status status;

	MPI_Init(&argc, &argv); //Musi być w każdym programie na początku

	printf("Checking!\n");
	MPI_Comm_size( MPI_COMM_WORLD, &size );
	MPI_Comm_rank( MPI_COMM_WORLD, &tid );
	printf("My id is %d from %d\n",tid, size);

    int myGroup[SIZE_GROUP];
    myGroup[0] = tid; // ja
    
    if(tid+1<size)
        myGroup[1] = tid+1; // nastepny
    else
        myGroup[1] = 0;
    
    if(tid-1>=0)
        myGroup[2] = tid-1; // poprzedni
    else
        myGroup[2] = size-1;
        
    
	int sendmsg[2];
    int recvmsg[2];

	bool locked = false;
    int lockedTid;
	int priority = tid;
	int myProc[MAX_QUEUE];
	int myProcPriority[MAX_QUEUE];
    int sizeMyProc = 0;
	int queue[MAX_QUEUE];
	int queuePriority[MAX_QUEUE];
    int sizeQueue = 0;
    int failedCount = 0;
    bool isInquire = false;
    int inquireFrom;
    
	for(int i=0;i<MAX_QUEUE;i++){
        myProc[MAX_QUEUE]=-1;
        myProcPriority[MAX_QUEUE]=-1;
        queue[MAX_QUEUE]=-1;
        queuePriority[MAX_QUEUE]=-1;
    }
    

	sendmsg[0] = tid;
	sendmsg[1] = priority;
	
     
    while(true){
       for(int i=0;i<size;i++){
            if(i == myGroup[0] || i == myGroup[1] || i == myGroup[2])
                MPI_Send( sendmsg, 2, MPI_INT, i, MSG_REQUEST, MPI_COMM_WORLD );
        }
        
        while(sizeMyProc!=SIZE_GROUP){
            
            MPI_Recv(recvmsg, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);  
        
            switch(status.MPI_TAG){
                case MSG_REQUEST:
                    if(DEBUG) printf("%d: Jest request od %d !!\n", tid, recvmsg[0]);	
                    if(!locked){
                        locked = true;
                        lockedTid = recvmsg[0];
                        
                        MPI_Send( sendmsg, 2, MPI_INT, recvmsg[0], MSG_LOCKED, MPI_COMM_WORLD );
                    } else {
                        bool isMoreImportant = true;
                        for(int i=0;i<sizeMyProc;i++){                
                            if(myProcPriority[i]>recvmsg[1]){
                                isMoreImportant = false;
                            }
                        }
                        
                        bool isInQueue = false;
                        for(int i=0; i<sizeQueue; i++){
                            if(queue[i]==recvmsg[0])
                                isInQueue = true;                                
                        }
                        
                        if(!isInQueue){
                            queue[sizeQueue] = recvmsg[0];
                            queuePriority[sizeQueue] = recvmsg[1];
                            sizeQueue++;
                        }
                        
                        if(isMoreImportant){                    
                            MPI_Send( sendmsg, 2, MPI_INT, lockedTid, MSG_INQUIRE, MPI_COMM_WORLD );
                        } else {
                            MPI_Send( sendmsg, 2, MPI_INT, recvmsg[0], MSG_FAILED, MPI_COMM_WORLD );
                        }
                    }
                    break;
                case MSG_INQUIRE:      
                    if(DEBUG) printf("%d: Jest INQUIRE od %d!!\n", tid, recvmsg[0]);	
                    if(failedCount>0){
                        if(DEBUG) printf("%d: wysylkam RELINQUISH!!\n", tid);	
                        locked = false;
                        lockedTid=-1;
                        
                        
                        int maxPrio = -1;
                        int maxPrioTid = -1;
                        for(int i=0;i<sizeQueue;i++){                
                            if(queuePriority[i]>maxPrio){
                                maxPrio = queuePriority[i];
                                maxPrioTid = queue[i];
                            }
                        }
                        if(maxPrio>-1){      
                            locked = true;
                            lockedTid=maxPrioTid;
                             
                            
                            int newQueue[MAX_QUEUE];
                            int newQueuePriority[MAX_QUEUE];
                            int newSizeQueue = 0;
                            
                            for(int i=0;i<MAX_QUEUE;i++){
                                newQueue[i]=-1;
                                newQueuePriority[i]=-1;
                            }
                            
                            for(int i=0;i<sizeQueue;i++){     
                                if(queue[i]!=maxPrioTid){
                                    newQueue[newSizeQueue]         = queue[i];
                                    newQueuePriority[newSizeQueue] = queuePriority[i];
                                    newSizeQueue++;
                                }
                            }
                            for(int i=0;i<MAX_QUEUE;i++){
                                queue[i] = newQueue[i];
                                queuePriority[i] = newQueuePriority[i];
                            }
                            sizeQueue = newSizeQueue;
                            
                            MPI_Send( sendmsg, 2, MPI_INT, maxPrioTid, MSG_LOCKED, MPI_COMM_WORLD ); 
                        
                        }
                        
                        MPI_Send( sendmsg, 2, MPI_INT, recvmsg[0], MSG_RELINQUISH, MPI_COMM_WORLD );   
                    } else {
                        isInquire = true;
                        inquireFrom = recvmsg[0];
                    }
                    break;
                case MSG_FAILED:
                    if(DEBUG) printf("%d: Jest FAILED!!\n", tid);	
                    failedCount++;
                    if(isInquire){
                        locked = false;
                        lockedTid=-1;
                        if(DEBUG) printf("%d: wysylkam RELINQUISH!!\n", tid);	
                        MPI_Send( sendmsg, 2, MPI_INT, inquireFrom, MSG_RELINQUISH, MPI_COMM_WORLD );
                                            
                        if(sizeQueue>0){                                
                            
                                int maxPrio = -1;
                                int maxPrioTid = -1;
                                for(int i=0;i<sizeQueue;i++){                
                                    if(queuePriority[i]>maxPrio){
                                        maxPrio = queuePriority[i];
                                        maxPrioTid = queue[i];
                                    }
                                }
                                if(maxPrio>-1){      
                                    locked = true;
                                    lockedTid=maxPrioTid; 
                                    
                                    int newQueue[MAX_QUEUE];
                                    int newQueuePriority[MAX_QUEUE];
                                    int newSizeQueue = 0;
                                    
                                    for(int i=0;i<MAX_QUEUE;i++){
                                        newQueue[i]=-1;
                                        newQueuePriority[i]=-1;
                                    }
                                    
                                    for(int i=0;i<sizeQueue;i++){     
                                        if(queue[i]!=maxPrioTid){
                                            newQueue[newSizeQueue]         = queue[i];
                                            newQueuePriority[newSizeQueue] = queuePriority[i];
                                            newSizeQueue++;
                                        }
                                    }
                                    for(int i=0;i<MAX_QUEUE;i++){
                                        queue[i] = newQueue[i];
                                        queuePriority[i] = newQueuePriority[i];
                                    }
                                    sizeQueue = newSizeQueue;
                                    
                                    MPI_Send( sendmsg, 2, MPI_INT, maxPrioTid, MSG_LOCKED, MPI_COMM_WORLD );  
                                
                                }
                    
                        }
                    }
                    break;
                case MSG_LOCKED:
                    if(DEBUG) printf("%d: Jest LOCKED od %d !!\n", tid, recvmsg[0]);	
                    myProc[sizeMyProc]           = recvmsg[0];
                    myProcPriority[sizeMyProc]   = recvmsg[1];
                    sizeMyProc++;
                    break;
                case MSG_RELEASE: {
                    if(DEBUG) printf("%d: Jest RELEASE!!\n", tid);	                    
                    int maxPrio = -1;
                    int maxPrioTid = -1;
                    for(int i=0;i<sizeQueue;i++){                
                        if(queuePriority[i]>maxPrio){
                            maxPrio = queuePriority[i];
                            maxPrioTid = queue[i];
                        }
                    }
                    if(maxPrio>-1){
                        locked = true;
                        lockedTid=maxPrioTid;
                        MPI_Send( sendmsg, 2, MPI_INT, maxPrioTid, MSG_LOCKED, MPI_COMM_WORLD );   
                        
                        int newQueue[MAX_QUEUE];
                        int newQueuePriority[MAX_QUEUE];
                        int newSizeQueue = 0;
                        
                        for(int i=0;i<MAX_QUEUE;i++){
                            newQueue[i]=-1;
                            newQueuePriority[i]=-1;
                        }
                        
                        for(int i=0;i<sizeQueue;i++){
                            if(queue[i]!=maxPrioTid){
                                newQueue[newSizeQueue]         = queue[i];
                                newQueuePriority[newSizeQueue] = queuePriority[i];
                                newSizeQueue++;
                            }
                        }
                        for(int i=0;i<MAX_QUEUE;i++){
                            queue[i] = newQueue[i];
                            queuePriority[i] = newQueuePriority[i];
                        }
                        sizeQueue = newSizeQueue;
                    
                    } else {
                        locked = false;
                        lockedTid = -1;
                    }
                    break;
                }               
                case MSG_RELINQUISH:
                    if(DEBUG) printf("%d: Jest RELINQUISH od %d!!\n", tid, recvmsg[0]);	
                    
                    bool isInQueue = false;
                    for(int i=0; i<sizeQueue; i++){
                        if(queue[i]==recvmsg[0])
                            isInQueue = true;                                
                    }
                    if(!isInQueue){
                        queue[sizeQueue] = recvmsg[0];
                        queuePriority[sizeQueue] = recvmsg[1];
                        sizeQueue++;
                    }
                    
                                        
                    int newMyProc[MAX_QUEUE];
                    int newMyProcPriority[MAX_QUEUE];
                    int newSizeMyProc=0;
                    for(int i=0;i<MAX_QUEUE;i++){
                        newMyProc[i]=-1;
                        newMyProcPriority[i]=-1;
                    }
                    for(int i=0;i<sizeMyProc;i++){     
                        if(myProc[i]!=recvmsg[0]){
                            newMyProc[newSizeMyProc]         = myProc[i];
                            newMyProcPriority[newSizeMyProc] = myProcPriority[i];
                            newSizeMyProc++;
                        }
                    }
                    for(int i=0;i<MAX_QUEUE;i++){
                        myProc[i] = newMyProc[i];
                        myProcPriority[i] = newMyProcPriority[i];
                    }
                    sizeMyProc = newSizeMyProc;
                    break;
            }
            
        }
        printf("%d: SEKCJA KRYTYCZNA!!! \n", tid);
        
        
        sleep(1);
        
        for(int i=0;i<MAX_QUEUE;i++){
            myProc[i]=-1;
            myProcPriority[i]=-1;
        }
        sizeMyProc = 0;
        failedCount = 0;
                    
        for(int i=0;i<size;i++){
            if((i == myGroup[0] || i == myGroup[1] || i == myGroup[2]))
                MPI_Send( sendmsg, 2, MPI_INT, i, MSG_RELEASE, MPI_COMM_WORLD );
        }
    }

    

	MPI_Finalize(); // Musi być w każdym programie na końcu
}
