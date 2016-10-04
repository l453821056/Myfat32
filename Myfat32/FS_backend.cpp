#include "FS_backend.h"
struct_CurrentData_struct Current;
int FS_Start_Sector;
struct_MBR_Info_struct MBR_Info;
int read_sector(const char *DiskFileName,uint8_t * buffer, uint32_t sector)
{
	FILE *fp;
	fp = fopen(DiskFileName, "r+b");
	fseek(fp, sector*SECTOR_SIZE, 0);
	fread(buffer, 1, 512, fp);
	fclose(fp);
	return 0;
}

int MBR_read(uint8_t * buffer)
{
	read_sector(DiskFileName, Current.buffer, 0);
	struct_MBR_struct *MBR;
	MBR = (struct_MBR_struct *)Current.buffer;
	MBR_Info.Active = (MBR->MBR_PartitionTable->MBR_BootIndicator) ?1 : 0;
	MBR_Info.FS_Type = (MBR->MBR_PartitionTable->MBR_PartionType == 0x0c) ? "FAT32" : "UNKNOWN";
	MBR_Info.ReservedSector = MBR->MBR_PartitionTable->MBR_SectorsPreceding;
	MBR_Info.StartCylinder = MBR->MBR_PartitionTable->MBR_StartCylinder;
	MBR_Info.StartHead = MBR->MBR_PartitionTable->MBR_StartHead;
	MBR_Info.StartSector = MBR->MBR_PartitionTable->MBR_StartSector;
	FS_Start_Sector = MBR_Info.ReservedSector;
	return 0;
}

int FS_read_sector(uint8_t * buffer, uint32_t sector)
{

	return 0;
}
