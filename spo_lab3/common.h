#pragma once

#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>
#define MAX 80
#define PORT 25080
#define SA struct sockaddr

typedef int RequestType;
#define REQUEST_TYPE_ADD 0
#define REQUEST_TYPE_CHANGE 1
#define REQUEST_TYPE_DELETE 2
#define REQUEST_TYPE_CHANGE_LIST 3
#define REQUEST_TYPE_DELETE_LIST 4
#define REQUEST_TYPE_SERVICE 5

//common data structures
//задача
typedef struct{
    char owner[16]; //владелец
    char listName[32];
    char title[32];
    char description[256];
    long id;
    time_t creation_time;
    time_t deadline;
}Task;

//пакет задач
typedef struct{
    RequestType type;
    Task task;
}TaskPackage;

//список листов
typedef struct ListItem{
    Task task;
    struct ListItem* next;
}ListItem;

void processRequest(TaskPackage taskPackage, ListItem* listRoot);
void add(Task task, ListItem* listRoot);
void change(Task task, ListItem* listRoot);
void delete(Task task, ListItem* currentItem);
void changeList(Task task, ListItem* listRoot);
void deleteList(Task task, ListItem* currentItem);