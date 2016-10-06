#include "FS_frontend.h"
#include "FS_backend.h"
#include <iostream>
using namespace std;
void Show_MBR()
{
	cout << "Found Volume" << endl;
	cout << "Actvie:" << MBR_Info.Active<<endl;
	cout << "Volume Type:" << MBR_Info.FS_Type<<endl;
}

void Show_FS_Info()
{
	cout <<endl<< "This Volume Total have # Sectors:" << FS_Info.TotalSectors<<endl;
	printf("\nSectors Per Cluster:%d\n", FS_Info.SectorsPerCluster);
	cout<< endl << "Reserved Sectors:"<<FS_Info.ReservedSectors<<endl;
	cout << endl << "Fat zone Total have # Sectors:" << FS_Info.FatSize << endl;
	cout<< endl << "Data zone Start from Cluster:"<<FS_Info.DataStartSector<<endl;
	cout << endl << "Data zone Total have # Sectors:" << FS_Info.DataSectors<<endl;
	cout << endl << "Data zone Total have # Clusters:" << FS_Info.DataClusters<<endl;
}

void Read_Print_File(const char * filename)
{
	FileInfo *fp;
	fp = FS_fopen("/hello.txt", "r");
	cout << endl << "File " << filename << " opened" << endl;
	cout << endl << "Parent Directory at Cluster:" << fp->ParentStartCluster << endl;;
	cout << endl << "File Start at Cluster:"<<fp->StartCluster<<endl;
	cout << endl << "File Start at Cluster:" << fp->CurrentSector << endl;
	cout<< endl << "File size (byte):"<<fp->FileSize<<endl;
	FS_fread(Current.buffer, 1, fp);
	cout << "-------------------File Content---------------------" << endl;
	cout << Current.buffer << endl;
	cout << "-------------------File Content---------------------" << endl;
}
