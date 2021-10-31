#ifndef LAB1FINAL_HFS_STRUCTURES_H
#define LAB1FINAL_HFS_STRUCTURES_H
#include <byteswap.h>
#include <stdint.h>

#define HFS_PLUS_SIGNATURE 0x482b
#define HFS_PLUS_VERSION 4

typedef uint16_t UniChar;

enum BTreeType {
    typeAllocation = 0,
    typeExtent = 1,
    typeCatalog = 2,
    typeAttributes = 3,
    typeStartup = 4
};

enum {
    kHFSRootParentID = 1,
    kHFSRootFolderID = 2,
    kHFSExtentsFileID = 3,
    kHFSCatalogFileID = 4,
    kHFSBadBlockFileID = 5,
    kHFSAllocationFileID = 6,
    kHFSStartupFileID = 7,
    kHFSAttributesFileID = 8,
    kHFSRepairCatalogFileID = 14,
    kHFSBogusExtentFileID = 15,
    kHFSFirstUserCatalogNodeID = 16
};

enum {
    kHFSPlusFolderRecord = 0x0001,
    kHFSPlusFileRecord = 0x0002,
    kHFSPlusFolderThreadRecord = 0x0003,
    kHFSPlusFileThreadRecord = 0x0004
};

//HFS+ data structures
struct HFSUniStr255 {
    uint16_t length;
    UniChar unicode[255];
}__attribute__((packed));
typedef struct HFSUniStr255 HFSUniStr255;
typedef const HFSUniStr255 *ConstHFSUniStr255Param;

void reverseHFSUniStr255(HFSUniStr255 *s);

struct HFSPlusBSDInfo {
    uint32_t ownerID;
    uint32_t groupID;
    uint8_t adminFlags;
    uint8_t ownerFlags;
    uint16_t fileMode;
    union {
        uint32_t iNodeNum;
        uint32_t linkCount;
        uint32_t rawDevice;
    } special;
}__attribute__((packed));
typedef struct HFSPlusBSDInfo HFSPlusBSDInfo;

void reverseHFSPlusBSDInfo(HFSPlusBSDInfo *s);

struct HFSPlusExtentDescriptor {
    uint32_t startBlock;
    uint32_t blockCount;
}__attribute__((packed));
typedef struct HFSPlusExtentDescriptor HFSPlusExtentDescriptor;
typedef HFSPlusExtentDescriptor HFSPlusExtentRecord[8];

void reverseHFSPlusExtentDescriptor(HFSPlusExtentDescriptor *s);

//a fork is a set of data associated with a file-system object
//HFS Plus сохраняет информацию о содержимом файла, используя HFSPlusForkDataструктуру.
// Две такие структуры - одна для ресурса и одна для ответвления данных - хранятся в записи каталога для каждого пользовательского файла.
struct HFSPlusForkData {
    uint64_t logicalSize; //Размер в байтах допустимых данных в вилке
    uint32_t clumpSize; //размер скопления(размер группы)
    uint32_t totalBlocks; //Общее количество блоков распределения, используемых всеми экстентами в этой вилке.
    // Массив дескрипторов экстентов для вилки. Этот массив содержит первые восемь дескрипторов экстентов.
    // Если требуется больше дескрипторов экстентов , они сохраняются в файле переполнения экстентов .
    HFSPlusExtentRecord extents;//Структура используется для хранения информации об определенном объеме.
}__attribute__((packed));
typedef struct HFSPlusForkData HFSPlusForkData;
typedef uint32_t HFSCatalogNodeID;

void reverseHFSPlusForkData(HFSPlusForkData *s);

//Каждый том HFS Plus содержит заголовок тома размером 1024 байта от начала тома.
// Заголовок тома - аналог главного блока каталога (MDB) для HFS - содержит информацию о томе в целом, включая расположение других ключевых структур в томе.
struct HFSPlusVolumeHeader {
    uint16_t signature;//Подпись тома, которая должна быть kHFSPlusSigWord( 'H+')
    uint16_t version;//Версия формата тома, которая в настоящее время составляет 4 ( kHFSPlusVersion) для томов HFS Plus

    uint32_t attributes;
    uint32_t lastMountedVersion;
    uint32_t journalInfoBlock;//Номер блока распределения для блока распределения, который содержит JournalInfoBlock
    uint32_t createDate;
    uint32_t modifyDate;
    uint32_t backupDate;//Дата и время последнего резервного копирования тома
    uint32_t checkedDate;//Дата и время последней проверки тома на соответствие

    uint32_t fileCount;//Общее количество файлов на томе. В fileCount не входят специальные файлы. Оно должно равняться количеству файловых записей, найденных в файле каталога.
    uint32_t folderCount;//Общее количество папок на томе. В folderCount не указана корневая папка

    uint32_t blockSize;
    uint32_t totalBlocks;
    uint32_t freeBlocks;

    uint32_t nextAllocation;//Начало поиска следующего размещения(свободных блоков)
    uint32_t rsrcClumpSize;
    uint32_t dataClumpSize;

    HFSCatalogNodeID nextCatalogID;//Следующий неиспользованный идентификатор каталога

    uint32_t writeCount;//увеличивается каждый раз при монтировании тома
    uint64_t encodingsBitmap;//отслеживаются кодировки текста, используемые в именах файлов и папок на томе
    uint32_t finderInfo[8]; //Этот массив 32-битных элементов содержит информацию, используемую Mac OS Finder, и процесс загрузки системного программного обеспечения.

    HFSPlusForkData allocationFile;//Информация о расположении и размере файла распределения
    HFSPlusForkData extentsFile; //Информация о расположении и размере файла экстентов
    HFSPlusForkData catalogFile;//Информация о расположении и размере файла каталога.
    HFSPlusForkData attributesFile;//Информация о расположении и размере файла атрибутов
    HFSPlusForkData startupFile;//Информация о расположении и размере файла запуска.
}__attribute__((packed));
typedef struct HFSPlusVolumeHeader HFSPlusVolumeHeader;

void reverseHFSPlusVolumeHeader(HFSPlusVolumeHeader *s);

//Дескриптор узла содержит основную информацию об узле, а также вперед и назад ссылки на другие узлы.
// Тип BTNodeDescriptor данных описывает эту структуру.
//дескриптор узла всегда имеет длину 14 байтов, поэтому список записей, содержащихся в узле, всегда начинается с 14 байтов от начала узла.
struct BTNodeDescriptor {
    uint32_t fLink;//forward. Номер следующего узла этого типа или 0, если это последний узел.
    uint32_t bLink;//backward. Номер предыдущего узла этого типа или 0, если это первый узел.
    int8_t kind; //Тип этого узла. Существует четыре типа узлов, определяемых константами, перечисленными ниже.
    uint8_t height;//Уровень или глубина этого узла в иерархии B-дерева. Для узла заголовка это поле должно быть нулевым. Для конечных узлов это поле должно быть равным единице. Для узлов индекса это поле на единицу больше высоты дочерних узлов, на которые оно указывает. Высота узла карты равна нулю, как и для узла заголовка.
    uint16_t numRecords;//Количество записей, содержащихся в этом узле.
    uint16_t reserved; //Реализация должна рассматривать это как зарезервированное поле.
}__attribute__((packed));
typedef struct BTNodeDescriptor BTNodeDescriptor;

void reverseBTNodeDescriptor(BTNodeDescriptor *s);

struct BTHeaderRec {
    uint16_t    treeDepth; //Текущая глубина B-дерева. Всегда равно heightполю корневого узла.
    uint32_t    rootNode; //Номер узла корневого узла, индексного узла, который действует как корень B-дерева.
    uint32_t    leafRecords;//Общее количество записей, содержащихся во всех листовых узлах.
    uint32_t    firstLeafNode; //Номер первого листового узла. Это может быть ноль, если нет конечных узлов.
    uint32_t    lastLeafNode;
    uint16_t    nodeSize; //Размер узла в байтах. Это степень двойки от 512 до 32 768 включительно.
    uint16_t    maxKeyLength;//Максимальная длина ключа в индексном или листовом узле.
    uint32_t    totalNodes; //Общее количество узлов (свободных или используемых) в B-дереве. Длина файла B-дерева равна этому значению, умноженному на nodeSize.
    uint32_t    freeNodes;//Количество неиспользуемых узлов в B-дереве.
    uint16_t    reserved1; //Реализация должна рассматривать это как зарезервированное поле.
    uint32_t    clumpSize; // смещение
    uint8_t     btreeType;
    uint8_t     keyCompareType;//Для томов HFSX это поле в заголовке B-дерева каталога определяет порядок ключей (независимо от того, учитывается ли регистр или регистр в томе). Во всех остальных случаях реализация должна рассматривать это как зарезервированное поле
    uint32_t    attributes;//Набор битов, используемых для описания различных атрибутов B-дерева. Значение этих битов дано ниже.
    uint32_t    reserved3[16]; //Реализация должна рассматривать это как зарезервированное поле.
}__attribute__((packed));
typedef struct BTHeaderRec BTHeaderRec;

void reverseBTHeaderRec(BTHeaderRec *s);

//Для данной записи файла, папки или потока ключ файла каталога состоит из CNID родительской папки и имени файла или папки
struct HFSPlusCatalogKey {
    uint16_t keyLength; //требуется для всех ключевых записей в B-дереве. Файл каталога, как и все B-деревья HFS Plus, использует большую длину ключа
    HFSCatalogNodeID parentID;
    HFSUniStr255 nodeName;
}__attribute__((packed));
typedef struct HFSPlusCatalogKey HFSPlusCatalogKey;

void reverseHFSPlusCatalogKey(HFSPlusCatalogKey *s);


struct HFSPlusCatalogFolder {
    int16_t recordType; //Тип записи данных каталога
    uint16_t flags; //Это поле содержит битовые флаги папки. Если в настоящее время для записей папок не определены биты, реализация должна рассматривать это как зарезервированное поле.
    //Количество файлов и папок, непосредственно содержащихся в этой папке.
    //Это равно количеству записей файлов и папок, ключ parentID которых совпадает с ключом этой папки folderID.
    uint32_t valence;
    uint32_t folderID;
    uint32_t createDate;//Дата и время создания папки.
    uint32_t contentModDate;//Дата и время последнего изменения содержимого папки.
    uint32_t attributeModDate;//Дата и время последнего изменения любого поля в записи каталога папки.
    uint32_t accessDate;//Дата и время последнего чтения содержимого папки.
    uint32_t backupDate;//Дата и время последнего резервного копирования папки.
    uint32_t textEncoding;//Подсказка относительно кодировки текста, на основе которого было получено имя папки.
    uint32_t reserved;//Реализация должна рассматривать это как зарезервированное поле.
    HFSPlusBSDInfo permissions;
    uint32_t userInfo[4];
    uint32_t finderInfo[4];
}__attribute__((packed));
typedef struct HFSPlusCatalogFolder HFSPlusCatalogFolder;

void reverseHFSPlusCatalogFolder(HFSPlusCatalogFolder *s);

//используется в файле B-дерева каталога для хранения информации о конкретном файле на томе.
struct HFSPlusCatalogFile {
    int16_t recordType;//Тип записи данных каталога.
    uint16_t flags;
    uint32_t reserved1;//Реализация должна рассматривать это как зарезервированное поле.
    HFSCatalogNodeID fileID;
    uint32_t createDate;
    uint32_t contentModDate;
    uint32_t attributeModDate;
    uint32_t accessDate;
    uint32_t backupDate;
    HFSPlusBSDInfo permissions;
    uint32_t userInfo[4];
    uint32_t finderInfo[4];
    uint32_t textEncoding;
    uint32_t reserved2;

    HFSPlusForkData dataFork; //Информация о расположении и размере вилки данных.
    HFSPlusForkData resourceFork;
}__attribute__((packed));
typedef struct HFSPlusCatalogFile HFSPlusCatalogFile;

void reverseHFSPlusCatalogFile(HFSPlusCatalogFile *s);
#endif //LAB1FINAL_HFS_STRUCTURES_H
