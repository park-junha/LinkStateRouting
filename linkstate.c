//  Lab 7
//  Junha Park
//  Link State routing lab
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>

#define N 4
const int MAXTHREAD = 3;

typedef struct {
    char name[50];
    char ip[50];
    int port;
} Machines;

Machines machines[N];
int routerId, routerPort;
int costs[N][N];
int distances[N];
pthread_mutex_t mutexLock;

//  Socket initialization
int sockfd;
struct sockaddr_in addr;
socklen_t addrLen = sizeof(struct sockaddr);

//  Print costs to console
void printCosts(void) {
    int i, j;
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            pthread_mutex_lock(&mutexLock);
            printf("%d ", costs[i][j]);
            pthread_mutex_unlock(&mutexLock);
        }
        printf("\n");
    }
}

//  Thread 1: receive message from other nodes in the network
void rcvMsg(void* arg) {
    printf("Thread 1: Receive messages.\n");
    int infoRecv[3];
    int hostNode, nextNode, newCost;

    while (1) {
        recvfrom(sockfd, infoRecv, sizeof(infoRecv), 0, NULL, NULL);
        hostNode = infoRecv[0];
        nextNode = infoRecv[1];
        newCost = infoRecv[2];

        printf("New update received, updating now.\n");

        pthread_mutex_lock(&mutexLock);
        costs[hostNode][nextNode] = newCost;
        costs[nextNode][hostNode] = newCost;
        pthread_mutex_unlock(&mutexLock);

        printf("Cost update completed, printing the new cost below:\n");
        printCosts();
    }
}

//  Thread 2: update costs
void updateCosts(void* arg) {
    printf("Thread 2: Update costs.\n");
    int neighborId;
    int newCost;

    while (1) {
        printf("Please enter the neighbor ID & new cost to update: \n");
        scanf("%d %d", &neighborId, &newCost);

        pthread_mutex_lock(&mutexLock);
        costs[routerId][neighborId] = newCost;
        costs[neighborId][routerId] = newCost;
        pthread_mutex_unlock(&mutexLock);

        printf("\nNew cost updated, the new cost table is: \n");
        printCosts();

        struct sockaddr_in destAddr;
        destAddr.sin_family = AF_INET;

        int i;
        for (i = 0; i < N; i++) {
            if (i == routerId) {
                printf("Own router, do not need to notify.\n");
                continue;
            }

            destAddr.sin_port = htons(machines[i].port);
            inet_pton(AF_INET, machines[i].ip, &destAddr.sin_addr.s_addr);

            int info[3] = {routerId, neighborId, newCost};

            printf("Sending update information to %s \t %d \n", machines[i].ip, machines[i].port);
            sendto(sockfd, info, sizeof(info), 0, (struct sockaddr*) &destAddr, addrLen);
            printf("Update information sent.\n");
        }
    }
}

//  Thread 3: LS algorithm
void Dijkstra(void* arg) {
    printf("Thread 3: Dijkstra's Algorithm.\n");

    int leastDistArr[4];
    int visited[4];
    int count;
    int minCost;
    int minIndex;
    int addTemp;
    int i = 0;
    int j = 0;
    int k = 0;

    while (1) {
        for (i = 0; i < N; i++) {
            for (j = 0; j < N; j++) {
                visited[j] = 0;
                leastDistArr[j] = costs[i][j];
            }
            visited[j] = 1;
            count = N-1;

            while (count > 0) {
                minCost = 9999;
                minIndex = -1;

                for (j = 0; j < N; j++) {
                    if (leastDistArr[j] < minCost && visited[j] == 0) {
                        minCost = leastDistArr[j];
                        minIndex = j;
                    }
                }

                visited[minIndex] = 1;

                for (k = 0; k < N; k++) {
                    if (visited[k] == 0) {
                        addTemp = leastDistArr[minIndex] + costs[minIndex][k];
                        if (addTemp < leastDistArr[k]) {
                            leastDistArr[k] = addTemp;
                        }
                    }
                }

                count--;
            }

            for (j = 0; j < N; j++) {
                printf("%d ", leastDistArr[j]);
            }

            printf("\n");
        }

        sleep(rand()%3+5);
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Argument error.\n");
        return 1;
    }

    pthread_t threads[MAXTHREAD];

    routerId = atoi(argv[1]);

    FILE *fdin, *mcin;
    fdin = fopen(argv[2], "rb");
    mcin = fopen(argv[3], "rb");

    int row = 0;
    for(row = 0; row < N; row++) {
        fscanf(fdin, "%d %d %d %d", &costs[row][0], &costs[row][1], &costs[row][2], &costs[row][3]);
        fscanf(mcin, "%s %s %d", &machines[row].name, &machines[row].ip, &machines[row].port);
        if (routerId == row) {
            routerPort = machines[row].port;
            printf("The port number of given router ID is %d.\n", routerPort);
        }
    }

    printCosts();

    fclose(fdin);
    fclose(mcin);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(routerPort);
    addr.sin_addr.s_addr = INADDR_ANY;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Failure to setup an endpoint socket");
        exit(1);
    }

    if ((bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr))) < 0){
        perror("Failure to bind server address to the endpoint socket");
        exit(1);
    }

    //  Create threads
    int i = 0;

    for (i = 0; i < MAXTHREAD; i++) {
        if (i == 0) {
            pthread_create(&threads[i], NULL, rcvMsg, (void*)(i));
        }
        if (i == 1) {
            pthread_create(&threads[i], NULL, updateCosts, (void*)(i));
        }
        if (i == 2) {
            pthread_create(&threads[i], NULL, Dijkstra, (void*)(i));
        }
    }
    
    for(i = 0; i < MAXTHREAD; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
