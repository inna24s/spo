#include "server.h"

int serverEndFlag = 0;
int serverNumConnections = 0;
atomic_int serverNumMessages = 0;
atomic_long serverTaskId = 0;
pthread_mutex_t serverSocketLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t serverBufferLock = PTHREAD_MUTEX_INITIALIZER;
//pthread_cond_t, которая обеспечивает блокирование одного или нескольких потоков до тех пор, пока не придёт сигнал, или не пройдёт максимально
//установленное время ожидания
pthread_cond_t serverWriterCV = PTHREAD_COND_INITIALIZER;
pthread_cond_t serverReaderCV = PTHREAD_COND_INITIALIZER;
TaskPackage serverBuffer;
ListItem serverListRoot;

struct WriterThreadData{
    int fd;
    char user[16];
    int endFlag;
};


void* serverWriterThread(void * voidParams){
    //инициализация мьютекса
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    struct WriterThreadData* threadData = (struct WriterThreadData*) voidParams;
    // ждем, пока файл с таким именем будет создан
    while (!serverEndFlag){
        //используется, чтобы атомарно освободить мьютекс и заставить вызывающий поток блокироваться по переменной состояния.
        // Функция pthread_cond_wait() возвращает 0 после успешного завершения.
        pthread_cond_wait(&serverWriterCV, &lock);
        if(strcmp(threadData->user, serverBuffer.task.owner) == 0 && !threadData->endFlag){
            //блокируем мьютекс
            pthread_mutex_lock(&serverSocketLock);
            write(threadData->fd, &serverBuffer, sizeof(TaskPackage));
            //разблокируем мьютекс
            pthread_mutex_unlock(&serverSocketLock);
        }
        serverNumMessages++;
        if(serverNumMessages == serverNumConnections){
            //пробуждаем поток
            pthread_cond_signal(&serverReaderCV);
        }
    }
    return NULL;
}

//поток, который следит за подключением к нему клиентов, принимает данные от клиентов
void* serverReaderThread(void * voidParams){
    int sockfd = *(int*) voidParams;
    char user[16];
    TaskPackage taskPackage;
    //инициализация мьютекса с помощью макроса
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    //handshake сервер и клиент идентифицируют друг друга и начинают общение
    read(sockfd, &taskPackage, sizeof(TaskPackage));
    strcpy(user, taskPackage.task.owner);

    pthread_mutex_lock(&serverBufferLock);
    //send all user's tasks
    ListItem* currentItem = serverListRoot.next;
    while (currentItem != NULL){
        if(strcmp(currentItem->task.owner, user) == 0){
            taskPackage.task = currentItem->task;
            taskPackage.type = REQUEST_TYPE_ADD;
            pthread_mutex_lock(&serverSocketLock);
            write(sockfd, &taskPackage, sizeof(TaskPackage));
            pthread_mutex_unlock(&serverSocketLock);
        }
        currentItem = currentItem->next;
    }
    //увеличиваем количество подключений
    serverNumConnections++;
    printf("Server accepted the client. Username is %s. Now %i connections\n", user, serverNumConnections);
    pthread_t writer;
    struct WriterThreadData data;
    data.fd = sockfd;
    data.endFlag = 0;
    strcpy(data.user, user);
    pthread_create(&writer, NULL, serverWriterThread, &data);
    //освобождает занятый мьютекс
    pthread_mutex_unlock(&serverBufferLock);
    //Функция recv служит для чтения данных из сокета
    //Первый аргумент - сокет-дескриптор, из которого читаются данные.
    // Второй и третий аргументы - соответственно, адрес и длина буфера для записи читаемых данных.
    // Четвертый параметр - это комбинация битовых флагов, управляющих режимами чтения.
    // Если аргумент flags равен нулю, то считанные данные удаляются из сокета.
    while(recv(sockfd, &taskPackage, sizeof(TaskPackage), 0) > 0) {
        printf("From client:\n\t Title : %s", taskPackage.task.title);
        printf("\t Description: %s", taskPackage.task.description);
        struct tm tm = *localtime(&taskPackage.task.creation_time);
        printf("\t Created: %d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900 , tm.tm_mon + 1 , tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        strcpy(taskPackage.task.owner, user);
        if(taskPackage.type == REQUEST_TYPE_ADD){
            taskPackage.task.creation_time = time(NULL);
            taskPackage.task.id = serverTaskId++;
        }
        //блокируем мьютекс
        pthread_mutex_lock(&serverBufferLock);
        processRequest(taskPackage, &serverListRoot);
        serverNumMessages = 0;
        serverBuffer = taskPackage;
        pthread_cond_broadcast(&serverWriterCV); //разблокирует все потоки, блокированные переменной состояния, на которую указывает cv, определенная pthread_cond_wait()
        pthread_cond_wait(&serverReaderCV, &lock); //используется, чтобы атомарно освободить мьютекс и заставить вызывающий поток блокироваться по переменной состояния
        pthread_mutex_unlock(&serverBufferLock);
    }
    data.endFlag = 1;
    pthread_mutex_lock(&serverBufferLock);
    serverNumConnections--;//уменьшаем колиество подключений
    printf("User %s disconnected. Now %i connections\n", user, serverNumConnections);
    pthread_cancel(writer);//посылает нити thread запрос отмены
    pthread_mutex_unlock(&serverBufferLock);
    return NULL;
}

void* listenerThread(void * voidParams){
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;
    //поток прослушивания клиента
    pthread_t clientReader;

    serverListRoot.next = NULL;

    // socket create and verification
    //domain AF_INET - IPv4
    //type SOCK_STREAM (надёжная потокоориентированная служба (сервис) или потоковый сокет)
    // protocol 0 - не указан
    sockfd = socket(AF_INET, SOCK_STREAM, 0); //возвращает файловый дескриптор
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        return NULL;
    }
    else
        printf("Socket successfully created..\n");
    //Функция bzero() устанавливает первые n байтов области, начинающейся с s в нули (пустые байты).
    bzero(&servaddr, sizeof(servaddr));

    // Далее нам необходимо связать сокет с портом и адресом.
    //На Linux мы можем так же забиндиться на INADDR_ANY, но в этом случае мы будем получать все дейтаграммы пришедшие на забинденный нами порт.
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    // Binding newly created socket to given IP and verification
    //bind привязывает к сокету sockfd локальный адрес my_addr длиной addrlen.
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed...\n");
        return NULL;
    }
    else
        printf("Socket successfully binded..\n");

    // Now server is ready to listen and verification
    //Создаём очередь запросов на соединение и ждём запроса клиента
    //Второй аргумент этой функции задает максимальное число соединений, которые ядро может помещать в очередь этого сокета
    if ((listen(sockfd, 5)) != 0) {
        printf("Listen failed...\n");
        return NULL;
    }
    else
        printf("Server listening..\n");

    len = sizeof(cli);

    while (!serverEndFlag){
        //Он извлекает первый запрос на подключение к очереди ожидающих подключений для
        //прослушивающий сокет, sockfd , создает новый подключенный сокет и
        // возвращает новый файловый дескриптор, ссылающийся на этот сокет.
        //Вновь созданный сокет не находится в состоянии прослушивания.
        connfd = accept(sockfd, (SA*)&cli, &len);
        if (connfd < 0) {
            printf("server acccept failed...\n");
            return NULL;
        }
        else{
            pthread_create(&clientReader, NULL, serverReaderThread, (void *) &connfd);
        }
    }

    close(sockfd);
    return NULL;
}

int serverMode(){
    pthread_t listener;
    pthread_create(&listener, NULL, listenerThread, NULL);
    while (!serverEndFlag){
        char c = getchar();
        if(c=='Q'){
            serverEndFlag = 1;
        }
    }
    return 0;
}