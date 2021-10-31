#include <ncurses.h>
#include "common.h"

int endFlagClient = 0;
int clientSocketFD;
//GUI variables
WINDOW *windows[4];
char* titles[4] = {"Lists", "Tasks", "Details", "Edit"};
int currentWindow = 0;
//list window
int currentListNumber = 0;
int totalLists = 0;
Task currentList;
//task window
int currentTaskNumber = 0;
int totalTasks = 0;
Task currentTask;
//details window
int currentDetailNumber = 0;
//edit window
char editBuffer[256];

//Data
ListItem taskListRoot;
ListItem listListRoot;

//Functions
void refreshEditWindow();
void refreshListWindow();
void refreshTaskWindow();
void refreshDetailsWindow();

void refreshListList(){
    ListItem *currentItem = &listListRoot;
    ListItem *currentTaskItem = &taskListRoot;
    while (currentItem->next != NULL){
        ListItem *item = currentItem->next;
        currentItem->next = currentItem->next->next;
        free(item);
    }
    while (currentTaskItem != NULL){
        currentItem = listListRoot.next;
        int flag = 0;
        while(currentItem != NULL){
            if(strcmp(currentItem->task.listName, currentTaskItem->task.listName) == 0){
                flag = 1;
                break;
            }
            currentItem = currentItem->next;
        }
        if(!flag){
            add(currentTaskItem->task, &listListRoot);
        }
        currentTaskItem = currentTaskItem->next;
    }
}

//считывает данные от клиента
void* clientReaderThread(void * voidParams){
    TaskPackage taskPackage;
    while (!endFlagClient){
        read(clientSocketFD, &taskPackage, sizeof(TaskPackage));
        processRequest(taskPackage, &taskListRoot);
        if(taskPackage.type == REQUEST_TYPE_DELETE_LIST || taskPackage.type == REQUEST_TYPE_DELETE){
            currentTask.id = -1;
            currentTaskNumber = 0;
            currentListNumber = 0;
        }
        refreshListList();
        refreshListWindow();
    }
    return NULL;
}
//соединение с сокетом
void setUpSocket(char* username, char* host){
    struct sockaddr_in servaddr;

    clientSocketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocketFD == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(host);
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    if (connect(clientSocketFD, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }

    pthread_t receiver;
    pthread_create(&receiver, NULL, clientReaderThread, NULL);
    TaskPackage taskPackage;
    //handshake
    strcpy(taskPackage.task.owner, username);
    taskPackage.type = REQUEST_TYPE_SERVICE;
    write(clientSocketFD, &taskPackage, sizeof(TaskPackage));
}

void refreshListWindow(){
    wclear(windows[0]);
    currentWindow == 0 ? box(windows[0], '|', '-') : box(windows[0], 0, 0);
    wprintw(windows[0], titles[0]);//форматированного вывода на окно
    int counter = 0;
    ListItem* item = listListRoot.next;
    while (item != NULL){
        if(counter == currentListNumber){
            mvwprintw(windows[0], counter+2, 1, ">");
            currentList = item->task;
        }
        mvwprintw(windows[0], counter+2, 2, item->task.listName);
        item = item->next;
        counter++;
    }
    totalLists = counter - 1;
    wrefresh(windows[0]);
    refreshTaskWindow();
}

void refreshTaskWindow(){
    wclear(windows[1]);
    currentWindow == 1 ? box(windows[1], '|', '-') : box(windows[1], 0, 0);
    wprintw(windows[1], titles[1]);
    int counter = 0;
    currentTask.id = -1;
    ListItem* item = taskListRoot.next;
    while (item != NULL){
        if(strcmp(item->task.listName, currentList.listName) == 0){
            if(counter == currentTaskNumber){
                mvwprintw(windows[1], counter+2, 1, ">");
                currentTask = item->task;
            }
            mvwprintw(windows[1], counter+2, 2, item->task.title);
            counter++;
        }
        item = item->next;
    }
    totalTasks = counter;
    wrefresh(windows[1]);
    refreshDetailsWindow();
}

void refreshDetailsWindow(){
    wclear(windows[2]);
    currentWindow == 2 ? box(windows[2], '|', '-') : box(windows[2], 0, 0);
    wprintw(windows[2], titles[2]);
    struct tm tm;
    char buff[32];
    if(currentTask.id != -1){
        mvwprintw(windows[2], 2, 2, "Title:");
        mvwprintw(windows[2], 2, 15, currentTask.title);

        mvwprintw(windows[2], 3, 2, "Task list:");
        mvwprintw(windows[2], 3, 15, currentTask.listName);

        mvwprintw(windows[2], 4, 2, "Description:");
        mvwprintw(windows[2], 4, 15, currentTask.description);

        tm = *localtime(&currentTask.creation_time);
        sprintf(buff, "Created:     %d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        mvwprintw(windows[2], 5, 2, buff);

        if(currentTask.deadline != 0){
            tm = *localtime(&currentTask.deadline);
            sprintf(buff, "Deadline:    %d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
            mvwprintw(windows[2], 6, 2, buff);
        } else{
            mvwprintw(windows[2], 6, 2, "Deadline:    -");
        }

        mvwprintw(windows[2], currentDetailNumber + 2, 1, ">");
        if(currentDetailNumber == 0){
            strcpy(editBuffer, currentTask.title);
        }
        if(currentDetailNumber == 1){
            strcpy(editBuffer, currentTask.listName);
        }
        if(currentDetailNumber == 2){
            strcpy(editBuffer, currentTask.description);
        }
    }
    wrefresh(windows[2]);
    refreshEditWindow();
}

void refreshEditWindow(){
    wclear(windows[3]);
    currentWindow == 3 ? box(windows[3], '|', '-') : box(windows[3], 0, 0);
    wprintw(windows[3], titles[3]);
    mvwprintw(windows[3], 2, 2, editBuffer);
    wrefresh(windows[3]);
}


int main(){
    char *username = "inna";
    char *host = "127.0.0.1";

    taskListRoot.next = NULL;
    listListRoot.next = NULL;

    setUpSocket(username, host);

    const int listWW = 20;
    const int editWH = 7;

    initscr(); //инициализирует работу с консолью.
    noecho(); //Выключаем отображение вводимых символов
    raw(); //Берём управление клавиатурой на себя
    curs_set(0);//устанавливает статус курсора
    refresh();

    //Вызов newwin создает и возвращает указатель на новое окно с заданным количеством строк и столбцов.
    windows[0] = newwin(LINES - editWH, listWW, 0, 0);
    windows[1] = newwin(LINES - editWH, listWW, 0, listWW);
    windows[2] = newwin(LINES - editWH, COLS - listWW * 2, 0, listWW * 2);
    windows[3] = newwin(editWH, COLS, LINES - editWH, 0);
    refreshListWindow();

    while (!endFlagClient){
        char c = getch();
        //getch() ожидает нажатия клавиши и возвращает её код. Здесь работа до тех пор, пока не будет нажат Esc.
        if(c == 27){
            endFlagClient = 1;
        }
        //tab
        if(c == 9){
            currentWindow = (currentWindow + 1) % (currentTask.id == -1 ? 2 : 4);
            refreshListWindow();
        }
        if(c == 'w' || c == 'W'){
            if(currentWindow == 0){
                currentListNumber = currentListNumber == 0 ? totalLists - 1: currentListNumber - 1;
                refreshListWindow();
            }
            if(currentWindow == 1){
                currentTaskNumber = currentTaskNumber == 0 ? totalTasks - 1: currentTaskNumber - 1;
                refreshTaskWindow();
            }
            if(currentWindow == 2){
                currentDetailNumber = currentDetailNumber == 0 ? 2 : currentDetailNumber - 1;
                refreshDetailsWindow();
            }
        }
        if(c == 's' || c == 'S'){
            if(currentWindow == 0){
                currentListNumber = (currentListNumber + 1) % totalLists;
                refreshListWindow();
            }
            if(currentWindow == 1){
                currentTaskNumber = (currentTaskNumber + 1) % totalTasks;
                refreshTaskWindow();
            }
            if(currentWindow == 2){
                currentDetailNumber = (currentDetailNumber + 1) % 3;
                refreshDetailsWindow();
            }
        }
    }
    endwin();
}


