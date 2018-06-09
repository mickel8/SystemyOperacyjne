#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <endian.h>
#include <pthread.h>
#include <errno.h>
#include <arpa/inet.h>

#include "utils.h"

struct task 
{
    char op;
    int a;
    int b;
};

struct nameBox
{
    int busy;
    int sockfd;
    char *name;
};

// ======== USER CONSTANTS ======== //
#define maxClients 20
// ================================ //


// ======== USER VARIABLES ======== //
static int port;
static char path[30];
static int networkSocket;
static int localSocket;
static pthread_t terminalThread;
static pthread_t networkThread;
static struct nameBox clientsNames[maxClients];
// ================================ //


// ======= FUNCTIONS ======== //
void *terminal(void *);
void *network(void *);
int create_network_socket(void);
int create_local_socket(void);
void start_listen(int);
int create_new_epoll(void);
void add_socket_to_epoll(int, int);
int accept_new_connection(struct epoll_event);
void receive_from_client(int, int);
void send_info(char *, int);
void close_socket(int);
void int_action(int);
void set_sigint_handler(void);
int register_name(char *, int);
void unregister_name(int);
// ========================== //


int main(int argc, char **argv)
{
    
    if(argc != 3)
    {
        printf("Bad arg number");
        exit(EXIT_FAILURE);
    }
    
    port = atoi(argv[1]);
    strcpy(path, argv[2]);  

    for(int i = 0; i < maxClients; i++) clientsNames[i].busy = 0;

    int res;

    res = pthread_create(&terminalThread, NULL, &terminal, NULL);
    if(res != 0) perror("pthread_create");
    res = pthread_create(&networkThread, NULL, &network, NULL);
    if(res != 0) perror("pthread_create");

    res = pthread_join(terminalThread, NULL);
    if(res != 0) perror("pthread_join");
    res = pthread_join(networkThread, NULL);
    if(res != 0) perror("pthread_join");

    return 0;
}


void *terminal(void *args)
{
    while(1)
    {
        struct task newTask;
        scanf("%d %c %d", &newTask.a, &newTask.op, &newTask.b);
        printf("Przyjęto działanie w Terminalu: %c %d %d\n", newTask.op, newTask.a, newTask.b);
    } 
    return NULL;
}

void *network(void *args)
{

    int res;
    int epollfd;

    set_sigint_handler();

    networkSocket = create_network_socket();
    localSocket = create_local_socket();

    printf("\e[96mNetwork socket fd: %d\n", networkSocket);
    printf("Local socket fd: %d \e[39m\n", localSocket);

    epollfd = create_new_epoll();

    add_socket_to_epoll(epollfd, networkSocket);
    add_socket_to_epoll(epollfd, localSocket);

    start_listen(networkSocket);
    start_listen(localSocket);

    struct epoll_event events[maxClients]; 
    int mess;
    int socketfd; 
    int clientfd;
    
    while(1)
    {
        mess = epoll_wait(epollfd, events, 20, -1);
        if(mess == -1) perror("network -> epoll_wait");

        for(int i = 0; i < mess; i++)
        {
            
            if(events[i].data.fd == networkSocket || events[i].data.fd == localSocket)
            {
                clientfd = accept_new_connection(events[i]);
                add_socket_to_epoll(epollfd, clientfd);
            }
            else 
            {
                receive_from_client(events[i].data.fd, epollfd);
            }

        }

    }
    
    return NULL;
}

int create_network_socket()
{
    int res;
    int sockfd;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) perror("create_network_socket -> socket");
    else printf("Utworzono gniazdo sieciowe\n");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htobe16(port);
    addr.sin_addr.s_addr = INADDR_ANY; 

    res = bind(sockfd, (struct sockaddr *) &addr, sizeof(addr));
    if(res == -1) perror("create_network_socket -> bind");
    else printf("Przypisano nazwę do gniazda sieciowego\n");

    return sockfd;
   
}

int create_local_socket()
{
    int res;
    int sockfd;

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(sockfd == -1) perror("create_local_socket -> socket");
    else printf("Utworzono gniazdo lokalne\n");

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);

    res = bind(sockfd, (struct sockaddr *) &addr, sizeof(addr));
    if(res == -1) perror("create_local_socket -> bind");
    else printf("Przypisano nazwę do gniazda lokalnego\n");

    return sockfd;
}

int create_new_epoll()
{
    int epollfd;

    epollfd = epoll_create1(0);
    if(epollfd == -1) perror("network -> epoll_create1");

    return epollfd;
}

void add_socket_to_epoll(int epollfd, int socketfd)
{
    int res;

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = socketfd;

    res = epoll_ctl(epollfd, EPOLL_CTL_ADD, socketfd, &event);
    if(res == -1) perror("network -> epoll_ctl");

}

void start_listen(int socketfd)
{
    int res;

    res = listen(socketfd, 100); 
    if(res == -1) perror("start_listen -> listen");

}

int accept_new_connection(struct epoll_event event)
{
    int socketfd;
    int clientfd;

    struct sockaddr_in addr_client;
    socklen_t addr_client_len;
    int c = sizeof(struct sockaddr_in);

    if(event.data.fd == networkSocket) socketfd = networkSocket;
    else socketfd = localSocket;
    
    clientfd = accept(socketfd, (struct sockaddr *) &addr_client, (socklen_t*)&c);
    if(clientfd == -1) perror("accept_new_connection -> accept");

    return clientfd;
}

void receive_from_client(int clientfd, int epollfd)
{
    int res;

    ssize_t readBytes;
    ssize_t sentBytes;
    char *line;
    char *mess_type = calloc(strlen(NAME)+ 1, sizeof(char));
    long mess_len;

    readBytes = recv(clientfd, mess_type, 2, 0);
    if(readBytes == -1) 
    {
        perror("receive_from_client -> recv");
        return;
    }
    else if (readBytes == 0)
    {
        printf("Klient zamknął połączenie\n");

        res = epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, NULL);
        if(res == -1) perror("Nie udało się wyrejestrować klienta z epoll'a\n");
        else printf("Wyrejestrowano klienta z epoll'a\n");
        unregister_name(clientfd);

        return;
    }
    else
    {
        if(strcmp(mess_type, NAME) == 0)
        {
    
            readBytes = recv(clientfd, &mess_len, sizeof(mess_len), 0);
            if(readBytes == -1) perror("NAME -> recv");
            else if(readBytes == 0) 
            {
                printf("NAME -> klient zamknął połączenie");
                unregister_name(clientfd);
            }

            line = calloc(ntohl(mess_len), sizeof(char));

            readBytes = recv(clientfd, line, ntohl(mess_len), MSG_WAITALL);
            if(readBytes == -1) perror("NAME -> recv");
            else if(readBytes == 0)
            {
                printf("NAME -> klient zamknął połączenie");
                unregister_name(clientfd);
            }

            res = register_name(line, clientfd);

            if(res == 0) send_info(FREE, clientfd);
            else if(res == -1) send_info(BUSY, clientfd);
            else if(res == -2) send_info(MAXC, clientfd);

            free(line);
            free(mess_type);
        }
        else if(strcmp(mess_type, RES) == 0)
        {

        }
        else
        {
            printf("Nieznany rodzaj wiadomości\n");       
        }
    }
    
    
}


void send_info(char *INFO, int sockfd)
{
    ssize_t sentBytes;
    sentBytes = send(sockfd, INFO, 2, 0); 
    if(sentBytes != 2) perror("send_info -> send");

}

void close_socket(int sockfd)
{
    int res;

    res = shutdown(sockfd, SHUT_RDWR);
    if(res == -1) perror("close_socket -> shutdown");

    res = close(sockfd);
    if(res == -1) perror("close_socket -> close");
    else printf("\nZamknięto gniazdo");
}

void int_action(int signo)
{
    pthread_cancel(terminalThread);
    close_socket(networkSocket);
    close_socket(localSocket);
    unlink(path);
    exit(EXIT_SUCCESS);
}

void set_sigint_handler()
{
    int res;

    struct sigaction intAct;
    intAct.sa_handler = int_action;
    sigemptyset(&intAct.sa_mask);
    intAct.sa_flags = 0;

    res = sigaction(SIGINT, &intAct, NULL);
    if(res == -1) perror("set_sigint_handler -> sigaction");
}

int register_name(char *name, int sockfd)
{
    int freeIndex = -1;

    for(int i = 0; i < maxClients; i++)
    {
        if(clientsNames[i].busy == 1)
        {
            if(strcmp(clientsNames[i].name, name) == 0) return -1;
        }
        else freeIndex = i;

    }

    if(freeIndex == -1)
    {
        printf("Przyjęto maksymalną ilosc połączeń\n");
        return -2;
    }
    else 
    {
        clientsNames[freeIndex].name = calloc(strlen(name) + 1, sizeof(char));
        strcpy(clientsNames[freeIndex].name, name);
        clientsNames[freeIndex].busy = 1;
        clientsNames[freeIndex].sockfd = sockfd;
        printf("\e[92m** Zarejestrowano nowego klienta: %s/%d **\e[39m\n", name, sockfd);
        return 0;
    }


}

void unregister_name(int sockfd)
{
    for(int i = 0; i < maxClients; i++)
    {
        if(clientsNames[i].sockfd == sockfd)
        {
            printf("\e[91m** Wyrejestrowano nazwę klienta %s/%d **\e[39m\n", clientsNames[i].name, sockfd);
            clientsNames[i].busy = 0;
            free(clientsNames[i].name);
        }
    }
}