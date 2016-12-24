#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
//constant
const int MAX_FLOOR=20;
const float DEFAULT_RATE=0.004; //new users per millisecond
//semaphores
sem_t elevatorCall;
sem_t waitingListSem,passengerListSem;
//shared variables
typedef struct waitingNodePrototype{
    int atFloor;
    int direction;
    sem_t* elevatorIsHere;
    struct waitingNodePrototype* next;
}waitingNode;
waitingNode* waitingListHead=NULL;
waitingNode* waitingListTail=NULL;
typedef struct passengerNodePrototype{
    pthread_t tid;
    int toFloor;
    sem_t* elevatorIsHere;
    struct passengerNodePrototype* next;
}passengerNode;
passengerNode* passengerListHead=NULL;
passengerNode* passengerListTail=NULL;
//threads
pthread_attr_t threadAttribute;

inline float cumulativeProbability(float,float);
void CreateElevatorThread();
void CreateNewUserThread();
void Elevator();
void User();

int main(int argc,char* argv[]){
    float rate;
    if(argc<=1)
        rate=DEFAULT_RATE;
    else if(argc>2){
        printf("Wrong Usage\n");
        printf("Argument: ([rate])");
        return 0;
    }
    else
        rate=atof(argv[1]);
    srand(time(NULL));

    //initialize
    sem_init(&elevatorCall,0,0);
    sem_init(&waitingListSem,0,1);
    sem_init(&passengerListSem,0,1);
    pthread_attr_init(&threadAttribute);

    CreateElevatorThread();

    clock_t last=clock();
    while(1){
        clock_t now=clock();
        if(now-last<100) //millisecond=0.1s
            usleep(35000); //microsecond=0.035s
        else if( (float)rand()/RAND_MAX >= cumulativeProbability((float)(now-last),rate) ){
            last=clock();
            CreateNewUserThread();
        }
        else
            usleep(1000); //microsecond=0.001s
    }
    return 0;
}
inline float cumulativeProbability(float elapsedTime,float rate){
    return 1-exp(-1*rate*elapsedTime);
}
void CreateElevatorThread(){
    pthread_t elevator;
    pthread_create(&elevator,&threadAttribute,NULL,Elevator);
    return;
}
void CreateNewUserThread(){
    pthread_t newUser;
    pthread_create(&newUser,&threadAttribute,NULL,User);
    return;
}

void Elevator(){
    int atFloor=rand()%MAX_FLOOR+1,toFloor=atFloor;
    while(1){
        //wait for any user
        if(waitingListHead==NULL)
            sem_wait(&elevatorCall);

        //choose destination
        int lowestFloor=atFloor,highestFloor=atFloor;
        int lowerUsers=0,upperUsers=0;
        //scan waitingList
        sem_wait(&waitingListSem); //critical section-waitingList
            waitingNode* aWaitingNode=waitingListHead;
            while(aWaitingNode!=NULL){
                if(aWaitingNode->atFloor<lowestFloor)
                    lowestFloor=aWaitingNode->atFloor;
                if(aWaitingNode->atFloor>highestFloor)
                    highestFloor=aWaitingNode->atFloor;
                if(aWaitingNode->atFloor<atFloor)
                    ++lowerUsers;
                if(aWaitingNode->atFloor>atFloor)
                    ++upperUsers;
                aWaitingNode=aWaitingNode->next;
            }
        sem_post(&waitingListSem);
        //choose the nearer one, same then more users, same then random one
        if(lowestFloor!=atFloor || highestFloor!=atFloor){
            if(lowestFloor==atFloor)
                toFloor=highestFloor;
            else if(highestFloor==atFloor)
                toFloor=lowestFloor;
            else{
                if(atFloor-lowestFloor<highestFloor-atFloor)
                    toFloor=lowestFloor;
                else if(atFloor-lowestFloor>highestFloor-atFloor)
                    toFloor=highestFloor;
                else{
                    if(lowerUsers>upperUsers)
                        toFloor=lowestFloor;
                    else if(lowerUsers<upperUsers)
                        toFloor=highestFloor;
                    else
                        toFloor=(rand()%2==0)?lowestFloor:highestFloor;
                }
            }
        }

        //go to destination by 1 floor
        while(1){
            //let out users leaving at this floor
            //scan passengerList
            sem_wait(&passengerListSem); //critical section-passengerList
                passengerNode* aPassengerNode=passengerListHead;
                while(aPassengerNode!=NULL){
                    passengerNode* nextPassengerNode=aPassengerNode->next;
                    if(aPassengerNode->toFloor==atFloor){
                        //signal user to leave
                        sem_post(aPassengerNode->elevatorIsHere);
                        //wait for user to successfully leave
                        pthread_t leavingPassengerThread=aPassengerNode->tid;
                        sem_post(&passengerListSem);
                            pthread_join(leavingPassengerThread,NULL);
                        sem_wait(&passengerListSem);
                    }
                    aPassengerNode=nextPassengerNode;
                }
            sem_post(&passengerListSem);
            //let in users taking at this floor with same direction
            //scan waitingList
            sem_wait(&waitingListSem); //critical section-waitingList
                waitingNode* aWaitingNode=waitingListHead;
                while(aWaitingNode!=NULL){
                    waitingNode* nextWaitingNode=aWaitingNode->next;
                    if(aWaitingNode->atFloor==atFloor
                       && (toFloor-atFloor)*aWaitingNode->direction>=0){
                        //signal user to enter
                        sem_post(aWaitingNode->elevatorIsHere);
                        //wait for user to successfully enter
                        sem_t* elevatorIsHere=aWaitingNode->elevatorIsHere;
                        sem_post(&waitingListSem);
                            while(sem_getvalue(elevatorIsHere,NULL)>=0)
                                usleep(10000); //microsecond=0.01s
                        sem_wait(&waitingListSem);

                        //overwrite destination if farther
                        sem_wait(&passengerListSem); //critical section-passengerList
                            if(toFloor>atFloor){
                                if(passengerListTail->toFloor>toFloor)
                                    toFloor=passengerListTail->toFloor;
                            }
                            else if(toFloor<atFloor){
                                if(passengerListTail->toFloor<toFloor)
                                    toFloor=passengerListTail->toFloor;
                            }
                            else
                                toFloor=passengerListTail->toFloor;
                        sem_post(&passengerListSem);
                    }
                    aWaitingNode=nextWaitingNode;
                }
            sem_post(&waitingListSem);

            if(toFloor==atFloor)
                break;
            if(toFloor>atFloor)
                ++atFloor;
            if(toFloor<atFloor)
                --atFloor;
        }
    }
}
void User(){
    //set random atFloor and toFloor
    int atFloor=rand()%MAX_FLOOR+1,toFloor=rand()%MAX_FLOOR+1;
    while(toFloor==atFloor)
        toFloor=rand()%MAX_FLOOR+1;
    //get direction
    int direction;
    if(toFloor>atFloor)
        direction=1;
    if(toFloor<atFloor)
        direction=-1;
    //create a semaphore that waits for elevator to arrive
    sem_t elevatorIsHere;
    sem_init(&elevatorIsHere,0,0);

    //add to waitingList
    sem_wait(&waitingListSem); //critical section-waitingList
        waitingNode* previousWaitingNode=waitingListTail;
        if(waitingListTail==NULL){
            waitingListHead=malloc(sizeof(waitingNode));
            waitingListTail=waitingListHead;
        }
        else{
            waitingListTail->next=malloc(sizeof(waitingNode));
            waitingListTail=waitingListTail->next;
        }
        waitingListTail->atFloor=atFloor;
        waitingListTail->direction=direction;
        waitingListTail->elevatorIsHere=&elevatorIsHere;
        waitingListTail->next=NULL;
    sem_post(&waitingListSem);

    //signal elevator to operate
    if(sem_getvalue(&elevatorCall,NULL)<0)
        sem_post(&elevatorCall);
    //wait for elevator to arrive
    sem_wait(&elevatorIsHere);

    //remove from waitingList
    sem_wait(&waitingListSem); //critical section-waitingList
        waitingNode* nextWaitingNode;
        if(previousWaitingNode==NULL){
            nextWaitingNode=waitingListHead->next;
            free(waitingListHead);
            waitingListHead=nextWaitingNode;
        }
        else{
            nextWaitingNode=previousWaitingNode->next->next;
            free(previousWaitingNode->next);
            previousWaitingNode->next=nextWaitingNode;
        }
        if(nextWaitingNode==NULL)
            waitingListTail=previousWaitingNode;
    sem_post(&waitingListSem);
    //add to passengerList
    sem_wait(&passengerListSem); //critical section-passengerList
        passengerNode* previousPassengerNode=passengerListTail;
        if(passengerListTail==NULL){
            passengerListHead=malloc(sizeof(passengerNode));
            passengerListTail=passengerListHead;
        }
        else{
            passengerListTail->next=malloc(sizeof(passengerNode));
            passengerListTail=passengerListTail->next;
        }
        passengerListTail->tid=pthread_self();
        passengerListTail->toFloor=toFloor;
        passengerListTail->elevatorIsHere=&elevatorIsHere;
        passengerListTail->next=NULL;
    sem_post(&passengerListSem);

    //wait for elevator to arrive
    sem_wait(&elevatorIsHere);

    //remove from passengerList
    sem_wait(&passengerListSem); //critical section-passengerList
        passengerNode* nextPassengerNode;
        if(previousPassengerNode==NULL){
            nextPassengerNode=passengerListHead->next;
            free(passengerListHead);
            passengerListHead=nextPassengerNode;
        }
        else{
            nextPassengerNode=previousPassengerNode->next->next;
            free(previousPassengerNode->next);
            previousPassengerNode->next=nextPassengerNode;
        }
        if(nextPassengerNode==NULL)
            passengerListTail=previousPassengerNode;
    sem_post(&passengerListSem);

    sem_destroy(&elevatorIsHere);
    return;
}
