#include "partition.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
//основными и дополнительными номерами файлов устройств, соответствующих разделам жестких дисков,
// количеством блоков в рамках этих разделов и именами файлов устройств, расположенных в директории /dev.
//Разделы, оканчивающиеся цифрой - файловые системы. Разделы без цифры на конце - реальные физические устройства.
#define PARTITIONS_PATH "/proc/partitions"
#define READ_ONLY "r"
#define DELIMITERS " \n"

partition *parse_partition_string(char *source) {
    partition *result = (partition *) malloc(sizeof(partition));
    //strtoll преобразовывает строку в long int
    result->major = strtoll(strtok(source, DELIMITERS), NULL, 10); //основные разделы
    result->minor = strtoll(strtok(NULL, DELIMITERS), NULL, 10); //доп разделы
    result->blocks = strtoll(strtok(NULL, DELIMITERS), NULL, 10); //количеством блоков
    result->size_mb = result->blocks / 1024;
    char *first = strtok(NULL, DELIMITERS);
    result->name = malloc(strlen(first) * sizeof(char)); //имена устройств
    strcpy(result->name, first);
    return result;
}

uint64_t load_partitions(partition *buffer, uint64_t max_length) {

    FILE *partitions_file = fopen(PARTITIONS_PATH, READ_ONLY);
    char string[100];
    fgets(string, sizeof(string), partitions_file); //считываем заголовки
    //printf("%s", string);
    fgets(string, sizeof(string), partitions_file); //пустая строчка
    char *ptr = fgets(string, sizeof(string), partitions_file); //данные
    uint64_t loaded = 0;

    while (ptr != NULL && loaded < max_length) {
        buffer[loaded++] = *parse_partition_string(string);
        ptr = fgets(string, sizeof(string), partitions_file);
    }
    fclose(partitions_file);
    return loaded;
}

void free_partition(partition *ptr, uint64_t partitions_size) {
    for (int i = 0; i < partitions_size; ++i) {
        free(ptr[i].name);
    }
    free(ptr);
}