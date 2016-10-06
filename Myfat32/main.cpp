#include "FS_backend.h"
#include "FS_frontend.h"
#include <iostream>
using namespace std;
char filename[FS_MAX_PATH];
int main(int argc, char* argv[])
{
	FS_intial();
	Show_MBR();
	Show_FS_Info();
	Read_Print_File("hello.txt");
}
