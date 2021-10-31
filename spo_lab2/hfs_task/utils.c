#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>
#include "utils.h"

#define HEADER_OFFSET 1024

HFSPlus *getHFSPlus(char *name) {
    int fd = open(name, O_RDONLY);
    HFSPlusVolumeHeader *header = malloc(sizeof(struct HFSPlusVolumeHeader));
    //pread() записывает максимум count байтов из описателя файлов fd, начиная со смещения offset (от начала файла), в буфер buf. Текущая позиция файла не изменяется.
    pread(fd, header, sizeof(struct HFSPlusVolumeHeader), HEADER_OFFSET);
    reverseHFSPlusVolumeHeader(header);
    if (header->signature == HFS_PLUS_SIGNATURE && header->version == HFS_PLUS_VERSION) {
        HFSPlus *fileSystem = malloc(sizeof(HFSPlus));
        fileSystem->volumeHeader = header;
        fileSystem->deviceDescriptor = fd;
        BTree *catBtree = malloc(sizeof(BTree));
        catBtree->data = malloc(fileSystem->volumeHeader->catalogFile.totalBlocks * fileSystem->volumeHeader->blockSize);
        fileSystem->catalog = catBtree;
        int read_blocks = 0;
        int ext_counter = 0;
        int block_size = fileSystem->volumeHeader->blockSize;
        while (ext_counter < 8 && read_blocks < fileSystem->volumeHeader->catalogFile.totalBlocks) {
            pread(fileSystem->deviceDescriptor,
                  fileSystem->catalog->data + (read_blocks * block_size),
                  fileSystem->volumeHeader->catalogFile.extents[ext_counter].blockCount * block_size,
                  fileSystem->volumeHeader->catalogFile.extents[ext_counter].startBlock * block_size);
            ext_counter++;
            read_blocks += fileSystem->volumeHeader->catalogFile.extents[ext_counter].blockCount;
        }
        catBtree->header = catBtree->data + sizeof(BTNodeDescriptor);
        reverseBTHeaderRec(catBtree->header);
        fileSystem->pwd = kHFSRootFolderID;
        return fileSystem;
    }
    free(header);
    close(fd);
    return NULL;
}

Node *getNode(int nodeNumber, BTree *tree) {
    if (nodeNumber >= tree->header->totalNodes) {
        return NULL;
    }
    Node *node = malloc(sizeof(Node));
    node->descriptor = malloc(tree->header->nodeSize);
    memcpy(node->descriptor, tree->data + tree->header->nodeSize * nodeNumber, tree->header->nodeSize);
    reverseBTNodeDescriptor(node->descriptor);
    node->record_offsets = malloc(sizeof(uint16_t) * node->descriptor->numRecords);
    void *endOfNode = (void *) node->descriptor + tree->header->nodeSize;
    uint16_t *startOffsets = ((uint16_t *) endOfNode) - node->descriptor->numRecords;
    for (int i = 0; i < node->descriptor->numRecords; i++) {
        node->record_offsets[i] = bswap_16(startOffsets[i]);
    }
    return node;
}

Record *getRecord(int recordNumber, Node *node) {
    if (recordNumber >= node->descriptor->numRecords) {
        return NULL;
    }
    Record *record = malloc(sizeof(Record));
    int offset = node->record_offsets[node->descriptor->numRecords - 1 - recordNumber];
    record->size = node->record_offsets[node->descriptor->numRecords - 2 - recordNumber] - offset;
    record->data = malloc(record->size);
    void *data = ((void *) node->descriptor) + offset;
    memcpy(record->data, data, record->size);
    record->key_length = *((uint16_t *) record->data);
    record->key_length = bswap_16(record->key_length);
    record->id = bswap_32(*(uint32_t *) (record->data +
                                         sizeof(uint16_t) +
                                         record->key_length +
                                         sizeof(int16_t) +
                                         sizeof(uint16_t) +
                                         sizeof(uint32_t)));
    record->type = bswap_16(*(int16_t *) (record->data +
                                          sizeof(uint16_t) +
                                          record->key_length));
    record->key = malloc(sizeof(struct HFSPlusCatalogKey));
    memcpy(record->key, record->data, sizeof(HFSPlusCatalogKey));
    reverseHFSPlusCatalogKey(record->key);
    record->name = calloc(1, sizeof(char) * record->key->nodeName.length);
    uint8_t *key = (uint8_t *) &record->key->nodeName.unicode;
    for (int i = 0; i < record->key->nodeName.length; ++i) {
        char c1 = key[i * 2];
        char c2 = key[i * 2 + 1];
        strncat(&record->name[i], &c1, 1);
        strncat(&record->name[i], &c2, 1);
    }
    return record;
}

Record *getRecordByNameAndParent(HFSPlus *hfs, char *name, uint32_t parent) {
    uint32_t currentNodeID = hfs->catalog->header->firstLeafNode;
    while (currentNodeID > 0) {
        Node *node = getNode(currentNodeID, hfs->catalog);
        for (int i = 0; i < node->descriptor->numRecords; i++) {
            Record *record = getRecord(i, node);
            if (record->key->parentID == parent
                && strcmp(record->name, name) == 0
                && (record->type == kHFSPlusFolderRecord || record->type == kHFSPlusFileRecord))
                return record;
            freeRecord(record);
        }
        currentNodeID = node->descriptor->fLink;
        freeNode(node);
    }
    return NULL;
}

Record *getRecordFolderByID(HFSPlus *hfs, uint32_t findID) {
    uint32_t currentNodeID = hfs->catalog->header->firstLeafNode;
    while (currentNodeID > 0) {
        Node *node = getNode(currentNodeID, hfs->catalog);
        for (int i = 0; i < node->descriptor->numRecords; i++) {
            Record *record = getRecord(i, node);
            if (record->id == findID
                && record->type == kHFSPlusFolderRecord) {
                freeNode(node);
                return record;
            }
            freeRecord(record);
        }
        currentNodeID = node->descriptor->fLink;
        freeNode(node);
    }
    return NULL;
}

Record *getLastRecordFromPath(HFSPlus *hfs, char *path) {

    Record *record = NULL;
    char *token = strtok(path, "/");
    uint32_t bufID = hfs->pwd;
    if (path[0] == '/') {
        record = getRecordFolderByID(hfs, kHFSRootFolderID);
        if (strlen(path) > 1 && record != NULL) {
            bufID = record->id;
            freeRecord(record);
            record = NULL;
        } else {
            return record;
        }
    }
    while (token != NULL) {
        if (record != NULL) {
            freeRecord(record);
            record = NULL;
        }
        if (strcmp(token, ".") == 0) {
            token = strtok(NULL, "/");
            continue;
        }
        if (strcmp(token, "..") == 0) {
            record = getRecordFolderByID(hfs, bufID);
            if (record == NULL)
                return NULL;
            bufID = record->key->parentID;
            record = getRecordFolderByID(hfs, bufID);
        } else
            record = getRecordByNameAndParent(hfs, token, bufID);
        if (record == NULL)
            return NULL;
        bufID = record->id;
        token = strtok(NULL, "/");
    }
    return record;
}

char *f_pwd(HFSPlus *hfs) {
    Record *record;
    char *message = calloc(1, sizeof(char));
    if (hfs->pwd == kHFSRootFolderID) {
        message = realloc(message, strlen("/\n") + 1);
        sprintf(message, "/\n");
        return message;
    }
    HFSCatalogNodeID current = hfs->pwd;
    do {
        record = getRecordFolderByID(hfs, current);
        current = record->key->parentID;
        message = realloc(message, strlen(message) + strlen(record->name) + 1);
        char *buffer = calloc(1, strlen(message) +
                                 strlen(record->name) + strlen("/") + 1);
        sprintf(buffer, "/%s", record->name);
        strcat(buffer, message);
        sprintf(message, "%s", buffer);
        free(buffer);
        freeRecord(record);
    } while (current != kHFSRootFolderID);
    message = realloc(message, strlen(message) + strlen("\n") + 1);
    strcat(message, "\n");
    return message;
}


char *cd(HFSPlus *hfs, char *name) {
    Record *record;
    char *message = calloc(1, sizeof(char));
    if (strcmp(name, "..") == 0) {
        record = getRecordFolderByID(hfs, hfs->pwd);
        if (record != NULL) {
            record = getRecordFolderByID(hfs, record->key->parentID);
            if(record == NULL) {
                message = realloc(message, strlen("You are already in top\n") + 1);
                sprintf(message, "You are already in top\n");
                return message;
            }
        }
    } else if (name == NULL) {
        message = realloc(message, strlen("No arguments\n") + 1);
        sprintf(message, "No arguments\n");
        return message;
    } else
    record = getLastRecordFromPath(hfs, name);

    if (record != NULL) {
        if (record->type == kHFSPlusFolderRecord) {
            hfs->pwd = record->id;
            free(message);
            message = NULL;
        } else {
            message = realloc(message, strlen("Its not a directory\n"));
            sprintf(message, "Its not a directory\n");
        }
        freeRecord(record);
    } else {
        message = realloc(message, strlen("Didnt found folder\n"));
        sprintf(message, "Didnt found folder\n");
    }
    return message;
}

char *getChildrens(HFSPlus *hfs, uint32_t parent, char *buffer) {
    uint32_t currentNodeID = hfs->catalog->header->firstLeafNode;
    while (currentNodeID > 0) {
        Node *node = getNode(currentNodeID, hfs->catalog);
        for (int i = 0; i < node->descriptor->numRecords; i++) {
            Record *record = getRecord(i, node);
            if (strcmp(record->name, "") != 0 && record->key->parentID == parent) {
                if (record->type == kHFSPlusFolderRecord) {
                    buffer = realloc(buffer, sizeof(char) + strlen(buffer) + strlen("folder   |   \n") +
                                             record->key->nodeName.length);
                    sprintf(buffer, "%sfolder   |   %s\n", buffer, record->name);
                } else if (record->type == kHFSPlusFileRecord) {
                    buffer = realloc(buffer, sizeof(char) + strlen(buffer) + strlen("data     |   \n") +
                                             record->key->nodeName.length);
                    sprintf(buffer, "%sfile     |   %s\n", buffer, record->name);
                }
            }
            freeRecord(record);
        }
        currentNodeID = node->descriptor->fLink;
        freeNode(node);
    }
    return buffer;
}

char *ls(HFSPlus *hfs, char *path) {
    char *buffer = malloc(sizeof(char));
    buffer[0] = '\0';
    uint32_t id = hfs->pwd;
    if (path != NULL) {
        Record *record = getLastRecordFromPath(hfs,path);
        id = record->id;
        freeRecord(record);
    }
    return getChildrens(hfs, id, buffer);
}

char *recursive_copy(HFSPlus *hfs, Record *record, char *dest, char *message) {
    char *path;
    char *buffer;
    if (record->type == kHFSPlusFileRecord) {
        if (dest[strlen(dest) - 1] != '/') {
            path = calloc(1, strlen(dest) +
                             strlen("/") +
                             strlen(record->name) +
                             1);
            sprintf(path, "%s/%s", dest, record->name);
        } else {
            path = calloc(1, strlen(dest) +
                             strlen(record->name) +
                             1);
            sprintf(path, "%s%s", dest, record->name);
        }
        int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 00666);
        if (fd == -1) {
            buffer = calloc(1, strlen("Not able to create a File by destination: \n") +
                               strlen(path) + 1);
            sprintf(buffer, "Not able to create a data by destination: %s\n", path);
            message = realloc(message, strlen(message) + strlen(buffer) + 1);
            strcat(message, buffer);
            free(buffer);
            free(path);
            return message;
        }
        HFSPlusCatalogFile *file = malloc(sizeof(HFSPlusCatalogFile));
        memcpy(file, record->data + sizeof(int16_t) + record->key_length, sizeof(HFSPlusCatalogFile));
        reverseHFSPlusCatalogFile(file);
        int read_blocks = 0;
        int ext_counter = 0;
        int block_size = hfs->volumeHeader->blockSize;
        void *buffer = malloc(1);
        while (ext_counter < 8 && read_blocks < hfs->volumeHeader->catalogFile.totalBlocks) {
            buffer = realloc(buffer, file->dataFork.extents[ext_counter].blockCount * block_size);
            pread(hfs->deviceDescriptor,
                  buffer,
                  file->dataFork.extents[ext_counter].blockCount * block_size,
                  file->dataFork.extents[ext_counter].startBlock * block_size);
            pwrite(fd, buffer, file->dataFork.extents[ext_counter].blockCount * block_size, read_blocks);
            ext_counter++;
            read_blocks += hfs->volumeHeader->catalogFile.extents[ext_counter].blockCount;
        }
        close(fd);
        free(file);
        buffer = calloc(1, strlen("Successfully copied data  into \n") +
                           strlen(dest) + strlen(record->name) + 1);
        sprintf(buffer, "Successfully copied data %s into %s\n", record->name, dest);
        message = realloc(message, strlen(message) + strlen(buffer) + 1);
        strcat(message, buffer);
        free(buffer);
        free(path);
        return message;
    }
    if (record->type == kHFSPlusFolderRecord) {
        if (dest[strlen(dest) - 1] != '/') {
            path = calloc(1, strlen(dest) +
                             strlen("/") +
                             strlen(record->name) +
                             1);
            sprintf(path, "%s/%s", dest, record->name);
        } else {
            path = calloc(1, strlen(dest) +
                             strlen(record->name) +
                             1);
            sprintf(path, "%s%s", dest, record->name);
        }
        if (mkdir(path, 00777) != 0) {

            buffer = calloc(1, strlen("Not able to create a Folder by destination: \n") +
                               strlen(path) + 1);
            sprintf(buffer, "Not able to create a Folder by destination: %s\n", path);
            message = realloc(message, strlen(message) + strlen(buffer) + 1);
            strcat(message, buffer);
            free(buffer);
            free(path);
            return message;
        }
        buffer = calloc(1, strlen("Successfully created Folder  in \n") +
                           strlen(dest) + strlen(record->name) + 1);
        sprintf(buffer, "Successfully created Folder %s in %s\n", record->name, dest);
        message = realloc(message, strlen(message) + strlen(buffer) + 1);
        strcat(message, buffer);
        free(buffer);
        path = realloc(path, strlen(path) + strlen("/") + 1);
        strcat(path, "/");
        uint32_t currentNodeID = hfs->catalog->header->firstLeafNode;
        while (currentNodeID > 0) {
            Node *node = getNode(currentNodeID, hfs->catalog);
            for (int i = 0; i < node->descriptor->numRecords; i++) {
                Record *childRecord = getRecord(i, node);
                if (strcmp(childRecord->name, "") != 0 && childRecord->key->parentID == record->id) {
                    buffer = calloc(1, strlen(path) + 1);
                    sprintf(buffer, "%s", path);
                    message = recursive_copy(hfs, childRecord, buffer, message);
                    free(buffer);
                }
                freeRecord(childRecord);
            }
            currentNodeID = node->descriptor->fLink;
            freeNode(node);
        }
    }
    return message;
}

char *cp(HFSPlus *hfs, char *source, char *dest) {
    char *message = calloc(1, sizeof(char));
    if (source == NULL || dest == NULL) {
        message = realloc(message, strlen("No arguments\n") + 1);
        sprintf(message, "No arguments\n");
    }
    Record *record;
    record = getLastRecordFromPath(hfs, source);
    if (record == NULL) {
        message = realloc(message, strlen("Couldn't find the source to copy\n") + 1);
        sprintf(message, "Couldn't find the source to copy\n");
    } else
        message = recursive_copy(hfs, record, dest, message);
    return message;
}

void freeHFSPlus(HFSPlus *hfs) {
    freeBTree(hfs->catalog);
    close(hfs->deviceDescriptor);
    free(hfs->volumeHeader);
    free(hfs);
}

void freeBTree(BTree *bTree) {
    free(bTree->data);
    free(bTree);
}

void freeNode(Node *node) {
    free(node->descriptor);
    free(node);
}

void freeRecord(Record *record) {
    free(record->name);
    free(record->key);
    free(record->data);
    free(record);
}
