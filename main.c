//build with gcc -lm -lrt -lpthread
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
//constant
const int MAX_FLOOR=20;
const float DEFAULT_RATE=2; //new users per second
const int ELEVATOR_SPEED=100000; //in microsecond
const int ELEVATOR_WAIT_FOR_USER=0; //to leave or enter, in microsecond
//semaphores
sem_t elevatorUsage;
sem_t waitingListSem,passengerListSem;
//shared variables
typedef struct waitingNodePrototype{
    int atFloor;
    int direction;
    sem_t* elevatorIsHere;
    struct waitingNodePrototype* previous;
    struct waitingNodePrototype* next;
}waitingNode;
waitingNode* waitingListHead=NULL;
waitingNode* waitingListTail=NULL;
typedef struct passengerNodePrototype{
    pthread_t tid;
    int toFloor;
    sem_t* elevatorIsHere;
    struct passengerNodePrototype* previous;
    struct passengerNodePrototype* next;
}passengerNode;
passengerNode* passengerListHead=NULL;
passengerNode* passengerListTail=NULL;
//thread attribute
pthread_attr_t threadAttribute;

inline float nextTime(float rate);
inline void CreateElevatorThread();
inline void CreateNewUserThread();
void* Elevator();
void* User();

int main(int argc,char* argv[]){
    float rate;
    if(argc<=1)
        rate=DEFAULT_RATE;
    else if(argc>2){
        printf("Wrong Usage\n");
        printf("Argument: [rate=%.2f]\n",DEFAULT_RATE);
        printf("rate - number of users appearing every second\n");
        return 0;
    }
    else
        rate=atof(argv[1]);
    srand(time(NULL));

    //initialize semaphores and thread attribute
    sem_init(&elevatorUsage,0,0);
    sem_init(&waitingListSem,0,1);
    sem_init(&passengerListSem,0,1);
    pthread_attr_init(&threadAttribute);

    CreateElevatorThread();

    while(1){
        float waitTime=nextTime(rate)*1000000;
        if(waitTime/1000000>=1)
            usleep((int)waitTime/1000000*1000000);
        struct timespec last;
        clock_gettime(CLOCK_REALTIME,&last);
        waitTime-=(int)waitTime/1000000*1000000;
        struct timespec now;
        clock_gettime(CLOCK_REALTIME,&now);
        long nanoInterval=(now.tv_nsec-last.tv_nsec)>=0?(now.tv_nsec-last.tv_nsec):(now.tv_nsec-last.tv_nsec+1000000000);
        while(nanoInterval<waitTime){
            usleep(waitTime/3);
            clock_gettime(CLOCK_REALTIME,&now);
            nanoInterval=(now.tv_nsec-last.tv_nsec)>=0?(now.tv_nsec-last.tv_nsec):(now.tv_nsec-last.tv_nsec+1000000000);
        }
        CreateNewUserThread();
    }
    return 0;
}
inline float nextTime(float rate){
    return -logf(1.0-(float)rand()/(RAND_MAX+1))/rate;
}
inline void CreateElevatorThread(){
    pthread_t elevator;
    pthread_create(&elevator,&threadAttribute,Elevator,NULL);
    return;
}
inline void CreateNewUserThread(){
    pthread_t newUser;
    pthread_create(&newUser,&threadAttribute,User,NULL);
    return;
}

void* Elevator(){
    int atFloor=rand()%MAX_FLOOR+1,toFloor=atFloor;
    printf("[電梯] %dF\n",atFloor);
    while(1){
        //wait for any user
        sem_wait(&waitingListSem); //critical section-waitingList
            if(waitingListHead==NULL)
                printf("(電梯閒置)\n");
        sem_post(&waitingListSem);
        sem_wait(&elevatorUsage);
        sem_post(&elevatorUsage);

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
        if(toFloor!=atFloor)
            printf("(電梯前往 %dF)\n",toFloor);

        //go to destination by 1 floor
        while(1){
            //let out users leaving at this floor
            //scan passengerList
            sem_wait(&passengerListSem); //critical section-passengerList
                passengerNode* aPassengerNode=passengerListHead;
                while(aPassengerNode!=NULL){
                    if(aPassengerNode->toFloor==atFloor){
                        pthread_t leavingPassengerThread=aPassengerNode->tid;
                        passengerNode* previousPassengerNode=aPassengerNode->previous;
                        //signal user to leave
                        sem_post(aPassengerNode->elevatorIsHere);
            sem_post(&passengerListSem);
                        //wait for user to successfully leave
                        while(pthread_join(leavingPassengerThread,NULL)!=0){
                            usleep(ELEVATOR_WAIT_FOR_USER);
                        }
            sem_wait(&passengerListSem);
                        if(previousPassengerNode==NULL)
                            aPassengerNode=passengerListHead;
                        else
                            aPassengerNode=previousPassengerNode->next;
                    }
                    else
                        aPassengerNode=aPassengerNode->next;
                }
            sem_post(&passengerListSem);
            //let in users taking at this floor with same direction
            //scan waitingList
            sem_wait(&waitingListSem); //critical section-waitingList
                waitingNode* aWaitingNode=waitingListHead;
                while(aWaitingNode!=NULL){
                    if(aWaitingNode->atFloor==atFloor
                       && (toFloor-atFloor)*aWaitingNode->direction>=0){ //elevator has the same direction as user, or no direction
                        sem_t* elevatorIsHere=aWaitingNode->elevatorIsHere;
                        waitingNode* previousWaitingNode=aWaitingNode->previous;
                        //signal user to enter
                        sem_post(aWaitingNode->elevatorIsHere);
            sem_post(&waitingListSem);
                        //wait for user to successfully enter
                        sem_wait(&passengerListSem); //critical section-passengerList
                            while(passengerListTail==NULL){
                        sem_post(&passengerListSem);
                                usleep(ELEVATOR_WAIT_FOR_USER);
                        sem_wait(&passengerListSem);
                            }
                            while(passengerListTail->elevatorIsHere!=elevatorIsHere){
                        sem_post(&passengerListSem);
                                usleep(ELEVATOR_WAIT_FOR_USER);
                        sem_wait(&passengerListSem);
                            }
                        sem_post(&passengerListSem);

                        //overwrite destination if farther
                        sem_wait(&passengerListSem); //critical section-passengerList
                            if(toFloor>atFloor){
                                if(passengerListTail->toFloor>toFloor){
                                    toFloor=passengerListTail->toFloor;
                                    printf("(電梯改為前往 %dF)\n",toFloor);
                                }
                            }
                            else if(toFloor<atFloor){
                                if(passengerListTail->toFloor<toFloor){
                                    toFloor=passengerListTail->toFloor;
                                    printf("(電梯改為前往 %dF)\n",toFloor);
                                }
                            }
                            else{
                                toFloor=passengerListTail->toFloor;
                                printf("(電梯前往 %dF)\n",toFloor);
                            }
                        sem_post(&passengerListSem);
            sem_wait(&waitingListSem);
                        if(previousWaitingNode==NULL)
                            aWaitingNode=waitingListHead;
                        else
                            aWaitingNode=previousWaitingNode->next;
                    }
                    else
                        aWaitingNode=aWaitingNode->next;
                }
            sem_post(&waitingListSem);

            if(toFloor==atFloor)
                break;
            struct timespec then;
            clock_gettime(CLOCK_REALTIME,&then);
            struct timespec now;
            clock_gettime(CLOCK_REALTIME,&now);
            long nanoInterval=(now.tv_nsec-then.tv_nsec)>=0?(now.tv_nsec-then.tv_nsec):(now.tv_nsec-then.tv_nsec+1000000000);
            while(nanoInterval<ELEVATOR_SPEED*1000){
                usleep(ELEVATOR_SPEED/3);
                clock_gettime(CLOCK_REALTIME,&now);
                nanoInterval=(now.tv_nsec-then.tv_nsec)>=0?(now.tv_nsec-then.tv_nsec):(now.tv_nsec-then.tv_nsec+1000000000);
            }
            if(toFloor>atFloor)
                ++atFloor;
            if(toFloor<atFloor)
                --atFloor;
            printf("[電梯] %dF\n",atFloor);
        }
    }
    return;
}
void* User(){
    //set user number
    static int nextNumber=1;
    int userNumber=nextNumber++;

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

    //stage 1 - wait for elevator
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
        waitingNode* thisWaitingUser=waitingListTail; //saved for removing
        thisWaitingUser->atFloor=atFloor;
        thisWaitingUser->direction=direction;
        thisWaitingUser->elevatorIsHere=&elevatorIsHere;
        thisWaitingUser->previous=previousWaitingNode;
        thisWaitingUser->next=NULL;
        printf("[使用者 %d] %dF,%s\n",userNumber,atFloor,direction>0?"↑":"↓");
    sem_post(&waitingListSem);

    //signal elevator to operate
    sem_post(&elevatorUsage);
    //wait for elevator to arrive
    sem_wait(&elevatorIsHere);

    //stage 2 - take the elevator
    //remove from waitingList
    sem_wait(&waitingListSem); //critical section-waitingList
        if(thisWaitingUser->previous==NULL)
            waitingListHead=thisWaitingUser->next;
        else
            thisWaitingUser->previous->next=thisWaitingUser->next;
        if(thisWaitingUser->next==NULL)
            waitingListTail=thisWaitingUser->previous;
        else
            thisWaitingUser->next->previous=thisWaitingUser->previous;
        free(thisWaitingUser);
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
        passengerNode* thisPassenger=passengerListTail; //saved for removing
        thisPassenger->tid=pthread_self();
        thisPassenger->toFloor=toFloor;
        thisPassenger->elevatorIsHere=&elevatorIsHere;
        thisPassenger->previous=previousPassengerNode;
        thisPassenger->next=NULL;
        printf("(使用者 %d 進入電梯，按了 %dF 鍵)\n",userNumber,toFloor);
    sem_post(&passengerListSem);

    //wait for elevator to arrive
    sem_wait(&elevatorIsHere);

    //stage 3 - leave the elevator
    //remove from passengerList
    sem_wait(&passengerListSem); //critical section-passengerList
        if(thisPassenger->previous==NULL)
            passengerListHead=thisPassenger->next;
        else
            thisPassenger->previous->next=thisPassenger->next;
        if(thisPassenger->next==NULL)
            passengerListTail=thisPassenger->previous;
        else
            thisPassenger->next->previous=thisPassenger->previous;
        free(thisPassenger);
        printf("(使用者 %d 離開電梯)\n",userNumber);
    sem_post(&passengerListSem);

    sem_destroy(&elevatorIsHere);
    sem_wait(&elevatorUsage);
    return;
}
