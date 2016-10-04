#define DEBUG
#ifndef _FS_BACKEND_
#define _FS_BACKEND_
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#endif // !_FS_BACKEND_
#ifdef DEBUG
#define DBG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DBG_PRINTF(...)
#endif // DEBUG

#define FS_MAX_PATH 255
#define FS_MASK_EOC 0x0FFFFFFF 
#define SECTOR_SIZE 512
#define SECTOR_ALREADY_IN_MEMORY 0x00
#define NO_ERROR 0x00
#define ERROR_OPEN_VHD 0x01
#define ERROR_FS_TYPE 0x02
#define ERROR_SECTOR_SIZE 0x04
#define ERROR_INVALID_SEEK 0x08
#define ERROR_UNKOWN 0xFFFFFFFF
#define CURRENT_FLAG_DIRTY 0x01

#define ENRTRY_FLAG_DIRTY 0x01
#define ENRTRY_FLAG_OPEN 0x02
#define ENRTRY_FLAG_SIZECHANGED 0x04
#define ENRTRY_FLAG_ROOT 0x08

#define ENRTRY_MODE_READ 0x01
#define ENRTRY_MODE_WRITE 0x02
#define ENRTRY_MODE_APPEND 0x04
#define ENRTRY_MODE_OVERWRITE 0x08

#define ENRTRY_ATTR_READ_ONLY 0x01
#define ENRTRY_ATTR_HIDDEN 0x02
#define ENRTRY_ATTR_SYSTEM 0x04
#define ENRTRY_ATTR_VOLUME_LABEL 0x08
#define ENRTRY_ATTR_DIRECTORY 0x10
#define ENRTRY_ATTR_ARCHIVE 0x20
#define ENRTRY_ATTR_DEVICE 0x40
#define ENRTRY_ATTR_UNUSED 0x80
//Structures

typedef struct struct_MBR_PartitionTable_struct {
	uint8_t MBR_BootIndicator;
	uint8_t MBR_StartHead;
	uint8_t MBR_StartSector;
	uint8_t MBR_StartCylinder;
	uint8_t MBR_PartionType;
	uint8_t MBR_EndHead;
	uint8_t MBR_EndSector;
	uint8_t MBR_EndCylinder;
	uint32_t MBR_SectorsPreceding;
	uint32_t MBR_SectorsInPartition;
} MBR_PartitionTable_struct;

typedef struct struct_MBR_struct {
	uint8_t MBR_BootRecord[446];
	MBR_PartitionTable_struct MBR_PartitionTable[4];
	uint16_t MBR_Signature;
};
typedef struct struct_MBR_Info_struct {
	bool Active;
	int StartCylinder;
	int StartHead;
	int StartSector;
	int ReservedSector;
	std::string FS_Type;
};
typedef struct struct_BPB_struct {
	uint8_t     BS_JumpBoot[3];
	uint8_t     BS_OEMName[8];
	uint16_t    BytesPerSector;
	uint8_t     SectorsPerCluster;
	uint16_t    ReservedSectorCount;
	uint8_t     NumFATs;
	uint16_t    RootEntryCount;
	uint16_t    TotalSectors16;
	uint8_t     Media;
	uint16_t    FATSize16;
	uint16_t    SectorsPerTrack;
	uint16_t    NumberOfHeads;
	uint32_t    HiddenSectors;
	uint32_t    TotalSectors32;
	uint32_t    FATSize;
	uint16_t    ExtFlags;
	uint16_t    FSVersion;
	uint32_t    RootCluster;
	uint16_t    FSInfo;
	uint16_t    BkBootSec;
	uint8_t     Reserved[12];
	uint8_t     BS_DriveNumber;
	uint8_t     BS_Reserved1;
	uint8_t     BS_BootSig;
	uint32_t    BS_VolumeID;
	uint8_t     BS_VolumeLabel[11];
	uint8_t     BS_FileSystemType[8];
} BPB_struct;
typedef struct struct_DirectoryEntry_struct {
	uint8_t filename[8];
	uint8_t extension[3];
	uint8_t attributes;
	uint8_t reserved;
	uint8_t creationTimeMs;
	uint16_t creationTime;
	uint16_t creationDate;
	uint16_t lastAccessTime;
	uint16_t eaIndex;
	uint16_t modifiedTime;
	uint16_t modifiedDate;
	uint16_t firstCluster;
	uint32_t fileSize;
} DirectoryEntry_struct;
typedef struct struct_LongFileName_struct {
	uint8_t sequence_number;
	uint16_t name_field_1[5]; 
	uint8_t attributes;
	uint8_t reserved;
	uint8_t checksum;
	uint16_t name2[6];
	uint16_t firstCluster;
	uint16_t name3[2];
} LongFileName_struct;
typedef struct struct_FileInfo_struct {
	uint32_t ParentStartCluster;
	uint32_t StartCluster;
	uint32_t CurrentClusterIdx;
	uint32_t CurrentCluster;
	short CurrentSector;
	short CurrentByte;
	uint32_t pos;
	uint8_t flags;
	uint8_t attributes;
	uint8_t mode;
	uint32_t size;
	uint8_t filename[FS_MAX_PATH];
} FileInfo;
typedef struct struct_FS_Info_struct {
	uint8_t SectorsPerCluster;
	uint32_t ReservedSectors;
	uint32_t DataStartSector;
	uint32_t DataSectors;
	uint32_t DataClusters;
	uint32_t TotalSectors;
	uint32_t FatSize;
	uint32_t RootDirSectors;
} FS_Info_struct;
typedef struct struct_CurrentData_struct {
	uint32_t CurrentSector;
	uint8_t SectorFlags;
	uint8_t buffer[512];
} CurrentData_struct;
typedef struct struct_PWD_struct {
	char DirectoryName[FS_MAX_PATH];
	uint32_t cluster;
};
//Structures end

//Low Level Functions
int read_sector(const char *DiskFileName, uint8_t * buffer, uint32_t sector);
int write_sector(const char *DiskFileName, uint8_t *buffer, uint32_t sector);
int MBR_read(uint8_t *buffer);
int BPB_read(uint8_t *buffer);
int FS_read_sector(uint8_t * buffer, uint32_t sector);//Read a sector from the beginning of FS
int FS_fetch(uint32_t sector); //Fetch a sector
int FS_store(); //Write buffer to current sector
uint32_t FS_get_FAT_entry(uint32_t cluster);//Get FAT entry of specified cluster
int FS_set_FAT_entry(uint32_t cluster, uint32_t value);
int FS_free_clusterchain(uint32_t cluster);
uint32_t FS_find_free_cluster();
uint32_t FS_find_free_cluster_from(uint32_t c);
uint32_t FS_first_sector(uint32_t cluster);
//High Level Functions
int FS_intial();
FileInfo *FS_fopen(char *filename, const char *mode);
int FS_fclose(FileInfo *fp);
int FS_fread(uint8_t *dest, int size, FileInfo *fp);
int FS_fseek(FileInfo * fp, uint32_t offset, int origin);
int FS_compare_filename(FileInfo *fp, uint8_t *name);
bool FS_compare_filename_segment(DirectoryEntry_struct *entry, uint8_t *name);
int FILE_read_info(char *filename, FileInfo *fp);

//Golbal Variables
extern char DiskFileName[FS_MAX_PATH];
extern struct_CurrentData_struct Current;
extern struct_MBR_Info_struct MBR_Info;
extern struct_FS_Info_struct FS_Info;
extern struct_PWD_struct PWD;
