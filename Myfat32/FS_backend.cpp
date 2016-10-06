#include "FS_backend.h"
struct_CurrentData_struct Current;
int FS_Start_Sector;
struct_MBR_Info_struct MBR_Info;
struct_FS_Info_struct FS_Info;
struct_PWD_struct PWD;
char DiskFileName[]="fat32.vhd";
struct_CutedFilename_struct CutedFilename;
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
	uint32_t ErrorLevel = 0;
	ErrorLevel|=read_sector(DiskFileName, Current.buffer, 0);
	struct_MBR_struct *MBR;
	MBR = (struct_MBR_struct *)Current.buffer;
	MBR_Info.Active = (MBR->MBR_PartitionTable->MBR_BootIndicator) ?1 : 0;
	MBR_Info.FS_Type = (MBR->MBR_PartitionTable->MBR_PartionType == 0x0c) ? "FAT32" : "UNKNOWN";
	MBR_Info.ReservedSector = MBR->MBR_PartitionTable->MBR_SectorsPreceding;
	FS_Start_Sector = MBR_Info.ReservedSector;
	return ErrorLevel;
}

int BPB_read(uint8_t * buffer)
{
	DBG_PRINTF("\nStart reading BPB\n");
	uint32_t ErrorLevel = 0;
	ErrorLevel |= FS_read_sector(Current.buffer, 0);
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
		ErrorLevel |= FS_fsync();
	}
	DBG_PRINTF("\nFS_read_sector %d\n",sector);
	ErrorLevel |= FS_read_sector(Current.buffer, sector);
	return ErrorLevel;
}

int FS_fsync()
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

std::list<struct_FileInfo_struct> FS_dir(uint32_t cluster)
{
	std::list<struct_FileInfo_struct> dir_content;
	int offset = 0;
	std::string LongFileNameBuffer;
	for (; offset < FS_Info.SectorsPerCluster*SECTOR_SIZE; offset += 0x20)
	{

		DirectoryEntry_struct *CurrentOneFileInfo = (DirectoryEntry_struct *)&Current.buffer[offset];
		struct_FileInfo_struct *one_file_info;
		one_file_info=FS_read_one_file_info(CurrentOneFileInfo, cluster);
		if (one_file_info->attributes & ATTR_LONG_NAME_MASK == ATTR_LONG_NAME)
		{
			std::string LongFileNamePart = one_file_info->LongFilename;
			LongFileNamePart.append(LongFileNameBuffer);
			LongFileNameBuffer = LongFileNamePart;
		}
		else
		{
			strcpy(one_file_info->LongFilename,LongFileNameBuffer.c_str());
			dir_content.push_back(*one_file_info);
		}
	}
	return dir_content;
}


struct_FileInfo_struct *FS_read_one_file_info(DirectoryEntry_struct * CurrentOneFileInfo,uint32_t cluster)
{
	if (CurrentOneFileInfo->attributes&ATTR_LONG_NAME_MASK == ATTR_LONG_NAME)
	{
		struct_LongFileName_struct *LongFileName = (struct_LongFileName_struct*)CurrentOneFileInfo;
		struct_FileInfo_struct one_file_info;
		strcpy(one_file_info.LongFilename, FS_format_file_name(CurrentOneFileInfo));
		one_file_info.attributes = ATTR_LONG_NAME;
		one_file_info.checksum = LongFileName->checksum;
		return (struct_FileInfo_struct*)(LongFileName);
	}
	else
	{
		struct_FileInfo_struct one_file_info;
		strcpy(one_file_info.filename, "");
		strcpy(one_file_info.filename, FS_format_file_name(CurrentOneFileInfo));
		one_file_info.attributes = CurrentOneFileInfo->attributes;
		one_file_info.ParentStartCluster = cluster;
		one_file_info.CurrentByte = 0;
		one_file_info.StartCluster = one_file_info.CurrentCluster = (((uint32_t)CurrentOneFileInfo->ClusterLow) << 16) | CurrentOneFileInfo->FirstCluster;
		one_file_info.flags = 0;
		one_file_info.mode = ENRTRY_MODE_WRITE;
		one_file_info.pos = 0;
		one_file_info.FileSize = CurrentOneFileInfo->FileSize;
		one_file_info.CurrentClusterIdx = 0;
		struct_FileInfo_struct *p_one_file_info = new struct_FileInfo_struct;
		*p_one_file_info = one_file_info;
		return p_one_file_info;
	}
}

int FS_free_clusterchain(uint32_t cluster) {
	uint32_t fat_entry;
	fat_entry = FS_get_FAT_entry(cluster);
	while (fat_entry < FAT_MASK_EOC) {
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
	DBG_PRINTF("\nFS_intial over, ErrorLevel %x\n", ErrorLevel);
	strcpy(PWD.DirectoryName, "/");
	PWD.cluster = 2;
	return ErrorLevel;
}
int FS_Cut_Filename(char*filename, int n) {
	if (n == 0)
		return 0;
	int position = 0;
	while (filename[position]!='/' && filename[position]!='\0')
	{
		position++;
	}
	char temp[255] = { 0 };
	strncpy(temp, filename, position);
	CutedFilename.CutedFilename[CutedFilename.used] = temp;
	CutedFilename.used += 1;
	FS_Cut_Filename(filename + position+1, n - position -1 );
}

FileInfo * FS_fopen(char * filename, const char * mode)
{
	int ErrorLevel = 0;
	int cluster = 0;
	FileInfo *fp = new FileInfo;
	int n = strlen(filename)+1;
	if (filename[0] == '/')
	{
		cluster = 2;
		if (filename[1] == '\0')
		{
			fp->CurrentCluster = 2;
			fp->StartCluster = 2;
			fp->ParentStartCluster = 0xffffffff;
			fp->CurrentClusterIdx = 0;
			fp->CurrentSector = 0;
			fp->CurrentByte = 0;
			fp->attributes = ATTR_DIRECTORY;
			fp->pos = 0;
			fp->flags |= ENRTRY_FLAG_ROOT;
			fp->FileSize = 0xffffffff;
			fp->mode = ENRTRY_MODE_READ | ENRTRY_MODE_WRITE | ENRTRY_MODE_OVERWRITE;
			return fp;
		}
		else
		{
			CutedFilename.used =0;
			FS_Cut_Filename(filename + 1, n - 1);
			while (CutedFilename.used>0)
			{
				fp=FS_find_file(cluster,CutedFilename.CutedFilename[--CutedFilename.used].c_str());
				cluster = fp->StartCluster;
			}
		}
	}
	else
	{
		ErrorLevel |= ERROR_ILLEGAL_PATH;
	}
	return fp;
}

int FS_fclose(FileInfo * fp)
{
	FS_fsync();
	free (fp);
	return 0;
}
char *FS_format_file_name(DirectoryEntry_struct *entry) {
	int i, j;
	uint8_t *entryname = entry->filename;
	uint8_t *formated_file_name=new uint8_t [16];
	if (entry->attributes != 0x0f) {
		j = 0;
		for (i = 0; i<8; i++) {
			if (entryname[i] != ' ') {
				formated_file_name[j++] = entryname[i];
			}
		}
		formated_file_name[j++] = '.';
		for (i = 8; i<11; i++) {
			if (entryname[i] != ' ') {
				formated_file_name[j++] = entryname[i];
			}
		}
		formated_file_name[j++] = '\x00';
	}
	else {
		struct_LongFileName_struct *LongEntry = (struct_LongFileName_struct*)entry;
		j = 0;
		for (i = 0; i<5; i++) {
			formated_file_name[j++] = (uint8_t)LongEntry->name_1[i];
		}
		for (i = 0; i<6; i++) {
			formated_file_name[j++] = (uint8_t)LongEntry->name_2[i];
		}
		for (i = 0; i<2; i++) {
			formated_file_name[j++] = (uint8_t)LongEntry->name_3[i];
		}
		if (LongEntry->sequence_number & LAST_LONG_ENTRY_MASK == LAST_LONG_ENTRY_MASK) {
			formated_file_name[j++] = '\x00';
		}
	}
	
	return (char *)formated_file_name;
}
FileInfo *FS_find_file(uint32_t cluster,const char* filename)
{
	uint32_t FAT_content = FS_get_FAT_entry(cluster);
	if (FAT_content >= FAT_MASK_EOC) {
		FS_read_sector(Current.buffer, (uint32_t)((FS_Info.DataStartSector+(4 * cluster) / 512)));
		std::list<FileInfo> dir_content = FS_dir(cluster);
		std::list<FileInfo>::iterator dir_iterator;
		dir_iterator = dir_content.begin();
		while (dir_iterator != dir_content.end())
		{
			char FilenameFirstChar = dir_iterator->filename[0];
			if (FilenameFirstChar == '\000')
			{
				dir_content.erase(dir_iterator++);
			}
			else
			{
				++dir_iterator;
			}
		}

		for (dir_iterator = dir_content.begin(); dir_iterator != dir_content.end(); ++dir_iterator)
		{
			int n = sizeof(filename);
			struct_FileInfo_struct *fp = &(*dir_iterator);
			std::string filename1 = fp->filename;
			if ((strncasecmp(filename1.c_str(), filename,n) == 0))
			{
				struct_FileInfo_struct *fp_return = new struct_FileInfo_struct;
				*fp_return = *fp;
				return fp_return;
			}
		}
		return NULL;
	}
	//////////////////////////////
	else if(FAT_content <2)
	{
		return NULL;
	}
	else
	{
		FS_read_sector(Current.buffer, (uint32_t)((FS_Info.ReservedSectors + (4 * cluster) / 512)));
		std::list<FileInfo> dir_content = FS_dir(cluster);
		if (FAT_content == FAT_MASK_EOC) {
			FS_read_sector(Current.buffer, (uint32_t)((FS_Info.ReservedSectors + (4 * cluster) / 512)));
			std::list<FileInfo> dir_content = FS_dir(cluster);
			std::list<FileInfo>::iterator dir_iterator;
			for (dir_iterator = dir_content.begin(); dir_iterator != dir_content.end(); ++dir_iterator)
			{
				struct_FileInfo_struct *fp = &(*dir_iterator);
				if ((FS_compare_filename(fp, (uint8_t*)filename) == 1))
				{
					return fp;
				}
			}
			return FS_find_file(FAT_content,filename);
		}

	}
}
bool FS_compare_filename_raw_entry(DirectoryEntry_struct *entry, uint8_t *name) { 
	char *formated_file_name = FS_format_file_name(entry);
	bool FlagFind= !(strcasecmp(formated_file_name, (char *)&name));
	return FlagFind;
}

int FS_fread(uint8_t * dest, int size, FileInfo * fp) {
	uint32_t sector;
	while (size > 0) {
		sector = FS_first_sector(fp->CurrentCluster) + (fp->CurrentByte / 512);
		FS_fetch(sector); 
		*dest++ = Current.buffer[fp->CurrentByte % 512];
		size--;
		if (fp->attributes & ATTR_DIRECTORY) {
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
	uint32_t mark = FAT_MASK_EOC;
	uint32_t temp;
	if (pos > fp->FileSize) {
		return ERROR_INVALID_SEEK;
	}
	if (pos == fp->FileSize) {
		fp->FileSize += 1;
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

uint8_t FS_LongFilename_checksum(const uint8_t * DosName)
{
	int i=11;
	uint8_t checksum = 0;
		while (i)
		{
			checksum = ((checksum & 1) << 7) + (checksum >> 1) + *DosName++;
			i--;
		}
	return checksum;
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
		if (1 == FS_compare_filename_raw_entry(&entry, name)) { 
			FS_fseek(fp, -sizeof(DirectoryEntry_struct), fp->pos);
			return 1;
		}
		else {
			return 0;
		}
	}
	return -1;
}
int FS_fwrite(uint8_t *src, int size, int count, FileInfo *fp) {
	int i, tracking, segsize;
	fp->flags |= ENRTRY_FLAG_DIRTY;
	while (count > 0) {
		i = size;
		fp->flags |= ENRTRY_FLAG_SIZECHANGED;
		while (i > 0) {  
			FS_fetch(FS_first_sector(fp->CurrentCluster) + (fp->CurrentByte / 512));
			tracking = fp->CurrentByte % 512;
			segsize = (i<512 ? i : 512);
			memcpy(&Current.buffer[tracking], src, segsize);
			Current.SectorFlags |= ENRTRY_FLAG_DIRTY; 
			if (fp->pos + segsize > fp->FileSize)
			{
				fp->FileSize += segsize - (fp->pos % 512);
			}
			if (FS_fseek(fp, 0, fp->pos + segsize)) {
				return -1;
			}
			i -= segsize;
			src += segsize;		
		}
		count--;
	}
	return size - i;
}
