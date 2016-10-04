#include "FS_backend.h"
struct_CurrentData_struct Current;
int FS_Start_Sector;
struct_MBR_Info_struct MBR_Info;
struct_FS_Info_struct FS_Info;
struct_PWD_struct PWD;
int read_sector(const char *DiskFileName,uint8_t * buffer, uint32_t sector)
{
	FILE *fp;
	fp = fopen(DiskFileName, "r+b");
	if (fp == NULL)
	{
		DBG_PRINTF("\nERROR_OPEN_VHD\n");
		return ERROR_OPEN_VHD;
	}
	fseek(fp, sector*SECTOR_SIZE, 0);
	fread(buffer, 1, 512, fp);
	fclose(fp);
	return 0;
}

int write_sector(const char * DiskFileName, uint8_t * buffer, uint32_t sector)
{
	FILE *fp;
	fp = fopen(DiskFileName, "r+b");
	if (fp == NULL)
	{
		DBG_PRINTF("\nERROR_OPEN_VHD\n");
		return ERROR_OPEN_VHD;
	}
	fseek(fp, sector*SECTOR_SIZE, 0);
	fwrite(buffer, 1, 512, fp);
	fclose(fp);
	return 0;
}

int MBR_read(uint8_t * buffer)
{
	DBG_PRINTF("\nStart reading MBR\n");
	uint32_t ErrorLevel;
	ErrorLevel|=read_sector(DiskFileName, Current.buffer, 0);
	struct_MBR_struct *MBR;
	MBR = (struct_MBR_struct *)Current.buffer;
	MBR_Info.Active = (MBR->MBR_PartitionTable->MBR_BootIndicator) ?1 : 0;
	MBR_Info.FS_Type = (MBR->MBR_PartitionTable->MBR_PartionType == 0x0c) ? "FAT32" : "UNKNOWN";
	MBR_Info.ReservedSector = MBR->MBR_PartitionTable->MBR_SectorsPreceding;
	MBR_Info.StartCylinder = MBR->MBR_PartitionTable->MBR_StartCylinder;
	MBR_Info.StartHead = MBR->MBR_PartitionTable->MBR_StartHead;
	MBR_Info.StartSector = MBR->MBR_PartitionTable->MBR_StartSector;
	FS_Start_Sector = MBR_Info.ReservedSector;
	return ErrorLevel;
}

int BPB_read(uint8_t * buffer)
{
	DBG_PRINTF("\nStart reading BPB\n");
	uint32_t ErrorLevel;
	ErrorLevel |= read_sector(DiskFileName, Current.buffer, 0);
	struct_BPB_struct *BPB;
	BPB = (struct_BPB_struct *)Current.buffer;
	if (BPB->BytesPerSector != 512)
	{
		DBG_PRINTF("\nStart reading BPB(!=512)\n");
		ErrorLevel |= ERROR_SECTOR_SIZE;
	}
	FS_Info.FatSize = BPB->FATSize;
	FS_Info.RootDirSectors = 0;
	FS_Info.TotalSectors =BPB->TotalSectors32;
	FS_Info.DataSectors = FS_Info.TotalSectors - (BPB->ReservedSectorCount + (BPB->NumFATs * FS_Info.FatSize) + FS_Info.RootDirSectors);
	FS_Info.SectorsPerCluster = BPB->SectorsPerCluster;
	FS_Info.DataClusters = FS_Info.DataSectors / FS_Info.SectorsPerCluster;
	FS_Info.ReservedSectors = BPB->ReservedSectorCount;
	FS_Info.DataStartSector = BPB->ReservedSectorCount + (BPB->NumFATs * FS_Info.FatSize) + FS_Info.RootDirSectors;
	return ErrorLevel;
}

int FS_read_sector(uint8_t * buffer, uint32_t sector)
{
	uint32_t ErrorLevel = 0;
	ErrorLevel = read_sector(DiskFileName, Current.buffer, sector + FS_Start_Sector);
	return ErrorLevel;
}

int FS_fetch(uint32_t sector)
{
	uint32_t ErrorLevel = 0;
	if (Current.CurrentSector == sector)
	{
		DBG_PRINTF("\nSECTOR_ALREADY_IN_MEMORY\n");
		return SECTOR_ALREADY_IN_MEMORY;
	}
	if (Current.SectorFlags & CURRENT_FLAG_DIRTY)
	{
		DBG_PRINTF("\nCURRENT_FLAG_DIRTY\n");
		ErrorLevel |= FS_store();
	}
	DBG_PRINTF("\nFS_read_sector %d\n",sector);
	ErrorLevel |= FS_read_sector(Current.buffer, sector);
	return ErrorLevel;
}

int FS_store()
{
	uint32_t ErrorLevel = 0;
	ErrorLevel = write_sector(DiskFileName, Current.buffer, FS_Start_Sector+Current.CurrentSector);
	return ErrorLevel;
}

uint32_t FS_get_FAT_entry(uint32_t cluster)
{
	uint32_t offset = 4 * cluster;
	uint32_t sector = FS_Info.ReservedSectors + (offset/512);
	FS_fetch(sector);
	uint32_t *FAT_entry = ((uint32_t *) &(Current.buffer[offset % 512]));
	DBG_PRINTF("\nGot FAT entry of cluster %d(%x)\n", cluster, *FAT_entry);
	return *FAT_entry;
}

int FS_set_FAT_entry(uint32_t cluster, uint32_t value)
{
	uint32_t ErrorLevel = 0;
	uint32_t offset = 4 * cluster;
	uint32_t sector = FS_Info.ReservedSectors + (offset / 512);
	ErrorLevel |= FS_fetch(sector);
	uint32_t *FAT_entry = ((uint32_t *) &(Current.buffer[offset % 512]));
	DBG_PRINTF("\nGot FAT entry of cluster %d(%x)\n", cluster, *FAT_entry);
	if (*FAT_entry != value)
	{
		Current.SectorFlags &= CURRENT_FLAG_DIRTY;
		*FAT_entry = value;
		DBG_PRINTF("\nWrote FAT entry of cluster %d(%x)\n", cluster, value);
	}
	return ErrorLevel;
}

int FS_free_clusterchain(uint32_t cluster) {
	uint32_t fat_entry;
	fat_entry = FS_get_FAT_entry(cluster);
	while (fat_entry < FS_MASK_EOC) {
		if (fat_entry <= 2)
		{
			break;
		}
		FS_set_FAT_entry(cluster, 0x00000000);
		fat_entry = FS_get_FAT_entry(fat_entry);
		cluster = fat_entry;
	}
	return 0;
}

uint32_t FS_find_free_cluster()
{
	uint32_t i, entry, totalClusters;
	totalClusters = FS_Info.TotalSectors / FS_Info.SectorsPerCluster;
	for (i = 0; i<totalClusters; i++) {
		entry = FS_get_FAT_entry(i);
		if ((entry & 0x0fffffff) == 0) break;
	}
	return i;
}

uint32_t FS_find_free_cluster_from(uint32_t c)
{
	uint32_t i, entry, totalClusters;
	totalClusters = FS_Info.TotalSectors / FS_Info.SectorsPerCluster;
	for (i = c; i<totalClusters; i++) {
		entry = FS_get_FAT_entry(i);
		if ((entry & 0x0fffffff) == 0) break;
	}
	if (i == totalClusters) {
		return FS_find_free_cluster();
	}
	return i;
}

uint32_t FS_first_sector(uint32_t cluster)
{
	return ((cluster - 2)*FS_Info.SectorsPerCluster) + FS_Info.DataStartSector;
}

int FILE_read_info(char * filename, FileInfo *fp)
{
	int offset = 0;
	while (offset<FS_Info.SectorsPerCluster*SECTOR_SIZE)
	{

	}
	return 0;
}



int FS_intial()
{
	uint32_t ErrorLevel = 0;
	ErrorLevel |= MBR_read(Current.buffer);
	if (MBR_Info.FS_Type != "FAT32") {
		DBG_PRINTF("\nERROR_FS_TYPE %x\n",MBR_Info.FS_Type.c_str());
		ErrorLevel |= ERROR_FS_TYPE;
	}
	ErrorLevel |= BPB_read(Current.buffer);
	DBG_PRINTF("\n FS_intial over, ErrorLevel %x\n", ErrorLevel);
	strcpy(PWD.DirectoryName, "/");
	PWD.cluster = 2;
	return ErrorLevel;
}

FileInfo * FS_fopen(char * filename, const char * mode)
{
	FileInfo *fp;
	int n = strlen(filename);
	uint32_t cluster;
	//TODO:Multi-Level DIR
	fp->CurrentCluster = 2;
	fp->StartCluster = 2;
	fp->ParentStartCluster = 0xffffffff;
	fp->CurrentClusterIdx = 0;
	fp->CurrentSector = 0;
	fp->CurrentByte = 0;
	fp->attributes = ENRTRY_ATTR_DIRECTORY;
	fp->pos = 0;
	fp->flags |= ENRTRY_FLAG_ROOT;
	fp->size = 0xffffffff;
	fp->mode = ENRTRY_MODE_READ | ENRTRY_MODE_WRITE | ENRTRY_MODE_OVERWRITE;
	if (strchr(mode, 'r')) {
		fp->mode |= ENRTRY_MODE_READ;
	}
	if (strchr(mode, 'a')) {

		FS_fseek(fp, fp->size, 0);
		fp->mode |= ENRTRY_MODE_WRITE | ENRTRY_MODE_OVERWRITE;
	}
	if (strchr(mode, '+')) fp->mode |= ENRTRY_MODE_OVERWRITE | ENRTRY_MODE_WRITE;
	if (strchr(mode, 'w')) {
		if (!(fp->attributes & ENRTRY_ATTR_DIRECTORY)) {
			fp->size = 0;
			FS_fseek(fp, 0, 0);
			if ((cluster = FS_get_FAT_entry(fp->StartCluster)) != FS_MASK_EOC) {
				FS_free_clusterchain(cluster);
				FS_set_FAT_entry(fp->StartCluster, FS_MASK_EOC);
			}
		}
		fp->mode |= ENRTRY_MODE_WRITE;
	}


	return nullptr;
}

int FS_fclose(FileInfo * fp)
{
	FS_store();
	return 0;
}

bool FS_compare_filename_segment(DirectoryEntry_struct *entry, uint8_t *name) { 
	int i, j;
	uint8_t reformatted_file[16];
	uint8_t *entryname = entry->filename;
	if (entry->attributes != 0x0f) {
		j = 0;
		for (i = 0; i<8; i++) {
			if (entryname[i] != ' ') {
				reformatted_file[j++] = entryname[i];
			}
		}
		reformatted_file[j++] = '.';
		for (i = 8; i<11; i++) {
			if (entryname[i] != ' ') {
				reformatted_file[j++] = entryname[i];
			}
		}
	}
	reformatted_file[j++] = '\x00';
	bool FlagFind= !(strcasecmp((char *)&reformatted_file, (char *)&name));
	return FlagFind;
}

int FS_fread(uint8_t * dest, int size, FileInfo * fp) {
	uint32_t sector;
	while (size > 0) {
		sector = FS_first_sector(fp->CurrentCluster) + (fp->CurrentByte / 512);
		FS_fetch(sector);       // wtfo?  i know this is cached, but why!?
								//printHex(&FS_info.buffer[fp->currentByte % 512], 1);
		*dest++ = Current.buffer[fp->CurrentByte % 512];
		size--;
		if (fp->attributes & ENRTRY_ATTR_DIRECTORY) {
			//dbg_printf("READING DIRECTORY");
			if (FS_fseek(fp, 0, fp->pos + 1)) {
				return -1;
			}
		}
		else {
			if (FS_fseek(fp, 0, fp->pos + 1)) {
				return -1;
			}
		}
	}
	return 0;
}

int FS_fseek(FileInfo * fp, uint32_t offset, int origin) {
	uint32_t cluster_idx;
	long pos = origin + offset;
	uint32_t mark = FS_MASK_EOC;
	uint32_t temp;
	if (pos > fp->size) {
		return ERROR_INVALID_SEEK;
	}
	if (pos == fp->size) {
		fp->size += 1;
		fp->flags |= ENRTRY_FLAG_SIZECHANGED;
	}
	cluster_idx = pos / (FS_Info.SectorsPerCluster * 512); 

	if (cluster_idx != fp->CurrentClusterIdx) {
		temp = cluster_idx;
		if (cluster_idx > fp->CurrentClusterIdx) {
			cluster_idx -= fp->CurrentClusterIdx;
		}
		else {
			fp->CurrentCluster = fp->StartCluster;
		}
		fp->CurrentClusterIdx = temp;
		while (cluster_idx > 0) {
			temp = FS_get_FAT_entry(fp->CurrentCluster);
			if (temp & 0x0fffffff != mark) fp->CurrentCluster = temp;
			else {
				temp = FS_find_free_cluster_from(fp->CurrentCluster);
				FS_set_FAT_entry(fp->CurrentCluster, temp); 
				FS_set_FAT_entry(temp, mark); 
				fp->CurrentCluster = temp;
			}
			cluster_idx--;
			if (fp->CurrentCluster >= mark) {
				if (cluster_idx > 0) {
					return     ERROR_INVALID_SEEK;
				}
			}
		}
	}
	fp->CurrentByte = pos % (FS_Info.SectorsPerCluster * 512); 
	fp->pos = pos;
	return 0;
}

int FS_compare_filename(FileInfo *fp, uint8_t *name) {
	uint32_t i, j = 0;
	uint32_t namelen;
	DirectoryEntry_struct entry;
	uint8_t *compare_name = name;
	uint32_t lfn_entries;

	FS_fread((uint8_t*)&entry, sizeof(DirectoryEntry_struct), fp);

	if (entry.filename[0] == 0x00) return -1;

	if (entry.attributes != 0x0f) {
		if (1 == FS_compare_filename_segment(&entry, name)) { 
			FS_fseek(fp, -sizeof(DirectoryEntry_struct), fp->pos);
			return 1;
		}
		else {
			return 0;
		}
	}
	return -1;
}
