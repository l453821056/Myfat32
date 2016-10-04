#include "FS_backend.h"
#include "FS_frontend.h"
#include <iostream>
using namespace std;
char filename[FS_MAX_PATH];
int main(int argc, char* argv[])
{
	if (argc == 1) {
		char *DiskFileName = argv[1];
	}
	else{
		char *DiskFileName = "fat32.vhd";
	}
	//FS_intial();
	//Print_MBR();
	//Print_FS_Info();
	//cout << "Input the file you want to read" << endl;
	//Read_Print_File(filename);
}