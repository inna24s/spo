#pragma once

#include "client.h"

int endFlagClient = 0;
ListItem clientListRoot;
//поток, который считывает данные от сервера
void* clientReaderThread(void * voidParams){
    int sockfd = *(int*) voidParams;
    TaskPackage taskPackage; //создаем пакет задач
    while (!endFlagClient){
        read(sockfd, &taskPackage, sizeof(TaskPackage));
        processRequest(taskPackage, &clientListRoot);
        ListItem* currentItem = clientListRoot.next;
        while (currentItem != NULL){
            printf("From server:\n\t ID: %li\n\t Title : %s\n\t List: %s", currentItem->task.id, currentItem->task.title, currentItem->task.listName);
            printf("\t Description: %s", currentItem->task.description);
            struct tm tm = *localtime(&currentItem->task.creation_time);
            printf("\t Created: %d-%02d-%02d %02d:%02d:%02d\n ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
            currentItem = currentItem->next;
        }
    }

    return NULL;
}

//создание задач
void cFunc(int sockfd, char * username)
{
    pthread_t receiver;
    pthread_create(&receiver, NULL, clientReaderThread, (void *) &sockfd);

    char buff[1024];
    TaskPackage taskPackage;

    //handshake
    strcpy(taskPackage.task.owner, username);
    taskPackage.type = REQUEST_TYPE_SERVICE;
    write(sockfd, &taskPackage, sizeof(TaskPackage));

    int command = -2;
    printf("Command list: \n \t0: ADD\n \t1: CHANGE\n \t2: DELETE\n \t3: CHANGE_LIST\n \t4: DELETE_LIST\n Enter command: ");
    while (!endFlagClient){
        fgets(buff, 1024, stdin);
        //Функция atoi() конвертирует начальную часть строки, на которую указывает nptr, в целое число.
        command = atoi(buff);
        if(command == -1){
            endFlagClient = 1;
            break;
        }
        //add
        else if(command == REQUEST_TYPE_ADD){
            printf("Enter title : ");
            fgets(buff, 1024, stdin);
            strcpy(taskPackage.task.title, buff);
            printf("Enter description : ");
            fgets(buff, 1024, stdin);
            strcpy(taskPackage.task.description, buff);
            printf("Enter list name : ");
            fgets(buff, 1024, stdin);
            strcpy(taskPackage.task.listName, buff);
            taskPackage.type = REQUEST_TYPE_ADD;
            write(sockfd, &taskPackage, sizeof(TaskPackage));
        }
        //change
        else if(command == REQUEST_TYPE_CHANGE){
            printf("Enter id: ");
            fgets(buff, 1024, stdin);
            taskPackage.task.id = atol(buff);
            printf("Enter title : ");
            fgets(buff, 1024, stdin);
            strcpy(taskPackage.task.title, buff);
            printf("Enter description : ");
            fgets(buff, 1024, stdin);
            strcpy(taskPackage.task.description, buff);
            printf("Enter list name : ");
            fgets(buff, 1024, stdin);
            strcpy(taskPackage.task.listName, buff);
            taskPackage.type = REQUEST_TYPE_CHANGE;
            write(sockfd, &taskPackage, sizeof(TaskPackage));
        }
        //delete
        else if(command == REQUEST_TYPE_DELETE){
            printf("Enter id: ");
            fgets(buff, 1024, stdin);
            taskPackage.task.id = atol(buff);
            taskPackage.type = REQUEST_TYPE_DELETE;
            write(sockfd, &taskPackage, sizeof(TaskPackage));
        }
        //delete list
        else if(command == REQUEST_TYPE_DELETE_LIST){
            printf("Enter list name : ");
            fgets(buff, 1024, stdin);
            strcpy(taskPackage.task.listName, buff);
            taskPackage.type = REQUEST_TYPE_DELETE_LIST;
            write(sockfd, &taskPackage, sizeof(TaskPackage));
        }
        //change list
        else if(command == REQUEST_TYPE_CHANGE_LIST){
            printf("Enter list name : ");
            fgets(buff, 1024, stdin);
            strcpy(taskPackage.task.listName, buff);
            printf("Enter new list name : ");
            fgets(buff, 1024, stdin);
            strcpy(taskPackage.task.title, buff);
            taskPackage.type = REQUEST_TYPE_CHANGE_LIST;
            write(sockfd, &taskPackage, sizeof(TaskPackage));
        }
        else{
            printf("Enter number of command");
            printf("Command list: \n \t0: ADD\n \t1: CHANGE\n \t2: DELETE\n \t3: CHANGE_LIST\n \t4: DELETE_LIST\n");
        }
    }
    //отправить запрос на отмену в поток
    pthread_cancel(receiver);


}

//создание сокета и вызов функции создания задач
int clientMode(char *username, char* host){
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;
    clientListRoot.next = NULL;

    // socket create and varification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(host);
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");

    cFunc(sockfd, username);

    // close the socket
    close(sockfd);
    return 0;
}