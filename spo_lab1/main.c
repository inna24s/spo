#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "hfs_task/utils.h"
#include "disks_partitions_task/partition.h"

int fs(char *filename) {
    HFSPlus *hfs = getHFSPlus(filename);
    if (hfs == NULL) {
        printf("This is not a HFSPlus system");
        return -1;
    }
    bool exit = false;
    char *inputString = malloc(1024);
    while (!exit) {
        printf(">");
        fgets(inputString, 1024, stdin);
        char *command = strtok(inputString, " \n");
        if (command == NULL) {
            continue;
        }
        char *source = strtok(NULL, " \n");
        char *dest = strtok(NULL, " \n");
        if (strcmp(command, "exit") == 0) {
            exit = 1;
        } else if (strcmp(command, "help") == 0) {
            printf("cd [directory] - change working directory\n");
            printf("pwd - print working directory full name\n");
            printf("cp - [directory] [target directory] - copy dir or file from mounted device\n");
            printf("ls - show working directory elements\n");
            printf("exit - terminate program\n");
            printf("help - print help\n");
        }else if (strcmp(command, "ls") == 0) {
            char *message = ls(hfs, source);
            printf("%s", message);
            free(message);
        }else if (strcmp(command, "cd") == 0) {
            char *message = cd(hfs, source);
            if (message != NULL) {
                printf("%s", message);
                free(message);
            }
        } else if (strcmp(command, "pwd") == 0) {
            char *message = f_pwd(hfs);
            printf("%s", message);
            free(message);
        }  else if (strcmp(command, "cp") == 0) {
            char *message = cp(hfs, source, dest);
            printf("%s", message);
            free(message);
        } else {
            printf("Command not found\n");
        }
    }
    freeHFSPlus(hfs);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "parts") == 0) {
        partition parts[50];
        uint64_t size = load_partitions(parts, 50);
        printf("MAJ:MIN| Size(Mb) Name\n");
        for (int i = 0; i < size; ++i)
            printf("%3d:%3d| %8llu %s\n", parts[i].major, parts[i].minor, parts[i].size_mb, parts[i].name);
    }
    if (argc >= 3 && strcmp(argv[1], "fs") == 0) {
        fs(argv[2]);
        return 0;
    }
    printf("Enter arguments!\n");
    puts("spo1 - program to work with filesystem");
    puts("keys:");
    puts("\tparts (to see a list of partitions) ");
    puts("\tfs <path> (to work with HFS+: hfsimg.bin)");
    return 0;
}
