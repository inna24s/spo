#ifndef SOFTWARE1LIB_PARTITION_H
#define SOFTWARE1LIB_PARTITION_H

#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
    char *name;
    uint32_t major;
    uint32_t minor;
    uint64_t blocks;
    uint64_t size_mb;
} partition;

uint64_t load_partitions(partition *buffer, uint64_t max_length);

void free_partition(partition *ptr, uint64_t partitions_size);

#endif //SOFTWARE1LIB_PARTITION_H
