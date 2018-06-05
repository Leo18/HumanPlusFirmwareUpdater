#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "stdlib.h"
#include "unistd.h"
#include <iostream>
#include "libusb-1.0/libusb.h"
// #include "MoveSenseSDK/MoveSenseCamera/MoveSenseCamera.h"
#include "MoveSenseCamera.h"
// #include "MoveSenseSDK/MoveSenseCamera/CameraRegister.h"
#include "CameraRegister.h"
#include "fx3_update.h"
#include "errno.h"

using namespace movesense;

#define usb_interface interface

// #define Video_VID 0x2d90
// #define Video_PID 0x00a2
// #define VID	0x04b4 	/* 0x1286 0x0525  */     
// #define PID	0x00f1		//0x0154    /* 0x4e25 0xA4A0  */ 

using namespace std;

void CrcTabel();
void CrcCal(unsigned char* buffer, unsigned int len);
void Change_VPID();
void Updata_FPGA();
bool Fresh_Device();

uint32_t Crc32Table[256];
uint32_t ii,jj;
uint32_t Crc;
int len = 0;
unsigned char buf[16];

libusb_device **devs;
uint8_t endpoint_in = 0, endpoint_out = 0;	// default IN and OUT endpoints
int count_dev;
struct pvid_List{
	int number;
	int idVendor;
	int idProduct;
} *newPVid_List;
int number_device;
int  CLIcmd;
int exist;
libusb_device_handle *handle;
libusb_context *ctx;
char * name_UpdateFile;

int main(int argc, char*argv[])
{
	/*****************判断待下载固件是否存在，若不存在重新输入************/
	string option;
	option = argv[1];			//--fpga or --fx3
	name_UpdateFile = NULL;
	name_UpdateFile = argv[2];
	if(argc < 2){
		cout << "format error "<<endl;
		char x[50];
		cin >> x;
		name_UpdateFile = x; 
	}
	if(option == "--fpga")
		CLIcmd=1;
	else if(option == "--fx3")
		CLIcmd=2;
	else if(option == "--help" && name_UpdateFile == NULL){
		cout << "Sample_update <option> <filename>"<<endl<<endl;;
		cout << "option:"<<endl<<"\t --fpga  or --fx3 or --help" <<endl;
		exit(-1);
	}
	else
		exit(-1);

	if(fopen(name_UpdateFile,"r+b") == NULL){
		cout<< "file not exist"<<endl;
		exit(-1);
	}

	ctx = NULL;
	ssize_t cnt;
	
	endpoint_in = 0x81;
	endpoint_out = 0x01;

	char string[128];
	int r;
	r = libusb_init(&ctx);
	if (r < 0)
		return r;

	libusb_set_debug(ctx, 3); 

	/**************查找video设备列表***************/
	exist = Fresh_Device();
	if(!exist){
		cout <<"no humanplus device"<<endl;
	}

	/*****************切换video设备以备下载*****************/
	Change_VPID();
	sleep(2);

	/*****************重新查找切换后的设备列表用来下载****************/
	exist = Fresh_Device();
	if(!exist){
		cout << "error second changed pvid"<<endl;
		exit(-1);
	}
	
	if(CLIcmd == 1)
		Updata_FPGA();
	else if(CLIcmd == 2){
		if(Updata_FX3(devs[number_device],1))
			cout << "update fx3 error"<<endl;
		
		sleep(1);
		exist = Fresh_Device();
		if(!exist){
			cout << "error third changed pvid"<<endl;
			exit(-1);
		}

		if(Updata_FX3(devs[number_device],3))
			cout << "update fx3 error" <<endl;
	}
	libusb_free_device_list(devs, 1);
	libusb_exit(NULL);

	return 0; 
}

void Updata_FPGA(){	
	int err;
	err = libusb_open(devs[number_device],&handle);
	if(err){
		cout << "open handle error" <<endl;
		exit(-1);
	}
	int r;
	r = libusb_claim_interface(handle, 0);
	if(r != 0){
		cout << "libusb claim interface error"<<endl;
		exit(-1);
	}

	int size1, size2;
	int r1, r2;
	int file_bytes_len = 0;
	char *file_bytes_cont;
	
	/****************打开待下载固件*******************/
	FILE *imgStream;
	if((imgStream = fopen(name_UpdateFile,"r+b")) != NULL)
	{
		fseek(imgStream,0,SEEK_END);
		int lsize = ftell(imgStream);
		rewind(imgStream);
		file_bytes_cont = (char*) malloc(sizeof(char)*lsize);
		file_bytes_len = fread(file_bytes_cont,sizeof(char),lsize,imgStream);
	}
	else
		exit(-1);
	cout << file_bytes_cont<<endl;
	cout << "file length: "<<file_bytes_len<<endl;

	/********************发送擦除固件命令*************/
		
	CrcTabel();
	CrcCal((unsigned char *)file_bytes_cont, (unsigned int)file_bytes_len);
	cout<< "Calculate Crc result:"<<Crc<<endl;
	buf[4] = (unsigned char)(Crc & 0x000000FF);
	buf[5] = (unsigned char)((Crc & 0x0000FF00) >> 8);
	buf[6] = (unsigned char)((Crc & 0x00FF0000) >> 16);
	buf[7] = (unsigned char)((Crc & 0xFF000000) >> 24);
	buf[0] = (unsigned char)(file_bytes_len & 0x000000FF);
	buf[1] = (unsigned char)((file_bytes_len & 0x0000FF00) >> 8);
	buf[2] = (unsigned char)((file_bytes_len & 0x00FF0000) >> 16);
	buf[3] = (unsigned char)((file_bytes_len & 0xFF000000) >> 24);

	uint8_t ReqType = LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_RECIPIENT_DEVICE|LIBUSB_ENDPOINT_OUT ;
	uint8_t ReqCode= 0xb3;
	uint16_t Value= 0;
	uint16_t Index= 1;
	uint16_t len = 8;
	unsigned int timeout = 30000;	//millseconds
	printf("waiting erasering(about 30s)...\n");
	int r_count;
	r_count = libusb_control_transfer(handle,ReqType,ReqCode,Value,Index,(unsigned char*)buf,len,timeout);
	if(r_count != len){
		cout<< "control eraser transfer error"<<endl;
		exit(-1);
	}
	r_count = 0;
	usleep(file_bytes_len*10);	//2M数据一次擦除大概需要20s+(经验?)；先空等待file_bytes/110毫秒，然后就是循环去获取数值了

	/***************校验擦除是否完成******************/

	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	ReqType = LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_RECIPIENT_DEVICE|LIBUSB_ENDPOINT_IN;
	ReqCode= 0xb1;
	Value= 0;
	Index= 1;
	len = 4;
	timeout = 2000;	//millseconds
	r_count=libusb_control_transfer(handle,ReqType,ReqCode,Value,Index,(unsigned char*)buf,len,timeout);
	if(r_count != len){
		cout<<"conrol eraser check transfer error"<<endl;
	}
	r_count = 0;
	printf("first eraser check return value: %d\n",r_count);
	while(buf[0] != 0x11){
		printf("buf[0] %x\n",buf[0]);	//Debug
		sleep(2);
		r_count=libusb_control_transfer(handle,ReqType,ReqCode,Value,Index,(unsigned char*)buf,len,timeout);
		if(r_count != len){
			cout<<"conrol eraser check transfer error"<<endl;
		}
	}

	cout <<" check success"<<endl;

	/***************通知设备将要下载固件***************/

	ReqType = LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_RECIPIENT_DEVICE|LIBUSB_ENDPOINT_OUT;
	ReqCode= 0xb2;
	Value= 0;
	Index= 1;
	len=5;
	buf[0] = (char)(file_bytes_len & 0x000000FF);
	buf[1] = (char)((file_bytes_len & 0x0000FF00) >> 8);
	buf[2] = (char)((file_bytes_len & 0x00FF0000) >> 16);
	buf[3] = (char)((file_bytes_len & 0xFF000000) >> 24);
	buf[4] = 0x22; //表示正常包
	r_count =libusb_control_transfer(handle,ReqType,ReqCode,Value,Index,(unsigned char*)buf,len,timeout);
	if(r_count != len){
		cout<<"conrol toupdate transfer error"<<endl;
	}
	// start transmit
	
	/**************下载操作及提示*************/
	cout << "last error before bulk"<<endl;
	perror(strerror(errno));
	size1 = 0;
	int size_count = 0;
	int error_count  = 0;
	{
		int i;
		int sr1 = 0;
		int prcount = 0;
		for(i=0;i<file_bytes_len/4096;i++){
			r1 = libusb_bulk_transfer(handle, endpoint_out,(unsigned char*) file_bytes_cont+4096*i,4096, &size1, 1000);
			if(r1 != 0){
				if(r1 == LIBUSB_ERROR_TIMEOUT)	cout<<"1 LIBUSB_ERROR_TIMEOUT, bulk transfer timeout"<<endl;
				else if(r1 == LIBUSB_ERROR_PIPE ) cout<<"1 LIBUSB_ERROR_PIPE, endpoint halted"<<endl;
				else if(r1 == LIBUSB_ERROR_OVERFLOW) cout<<"1 LIBUSB_ERROR_OVERFLOW"<<endl;
				else if(r1 == LIBUSB_ERROR_NO_DEVICE) cout<<"1 LIBUSB_ERROR_NO_DEVICE, device has been disconnected"<<endl;
				else if(r1 == LIBUSB_ERROR_BUSY) cout<<"1 LIBUSB_ERROR_BUSY, if called from event handing context"<<endl;
				else 
					cout<<"1 unknowed error in bulk transfer"<<endl;
				
			}
			size_count = size_count + size1;
			if(size1 != 4096){
				error_count ++;
			}
			if(size_count)
			if(++prcount == 50){
				cout<<"downloaded size : " << size_count<<endl;
				prcount = 0;
			}
		}
		if(error_count != 0)
			cout << "error count " <<error_count<<endl;
		libusb_bulk_transfer(handle, endpoint_out,(unsigned char*) file_bytes_cont+4096*i,file_bytes_len%4096, &size1, 1000);
	}
	cout << "last error after bulk"<<endl;
	perror(strerror(errno));
	//onetime only 4096;
	if(r == 0){
		cout<<"downloaded size : " << size_count+file_bytes_len%4096<<endl;
		cout<<"download success"<<endl;
	}
	else {
		cout << "download second progress error"<<endl;
		exit(-1);
	} 


	/***********检测下载完成后的校验返回是否正确********************/
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	ReqType = LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_RECIPIENT_DEVICE|LIBUSB_ENDPOINT_IN;
	ReqCode= 0xb1;
	Value= 0;
	Index= 1;
	len = 4;
	libusb_control_transfer(handle,ReqType,ReqCode,Value,Index,(unsigned char*)buf,len,timeout);
	while(buf[0] != 0x51){
		sleep(5);		//crc check time
		libusb_control_transfer(handle,ReqType,ReqCode,Value,Index,(unsigned char*)buf,len,timeout);
	}
	cout << "update completely"<<endl;

	libusb_release_interface(handle, 0);

	printf("Closing device...\n");
	libusb_close(handle);

}

void CrcTabel()
        {
            for (ii = 0; ii < 256; ii++)
            {
                Crc = ii;
                for (jj = 0; jj < 8; jj++)
                {
                    if (Crc % 2 > 0)
                        Crc = (Crc >> 1) ^ 0xEDB88320;
                    else
                        Crc >>= 1;
                }
                Crc32Table[ii] = Crc;
            }
        }

 void CrcCal(unsigned char* buffer, unsigned int len)
        {
            Crc = 0;
            for (ii = 0; ii < len; ii++)
            {
				int size = (Crc & 0xFF) ^ buffer[ii];
                Crc = (Crc >> 8) ^ Crc32Table[(Crc & 0xFF) ^ buffer[ii]];
            }
        }


void Change_VPID(){
	MoveSenseCamera *camera = new MoveSenseCamera();
	if (!camera->IsAvailable())
	{
			cout << "No camera connected..." << endl;
			exit(0);
	}

	if (camera->Open() != MS_SUCCESS)
	{
			cout << "Open camera failed!" << endl;
			exit(0);
	}
	cout << "Open camera ok!" << endl;

	uint8_t reg = 0;
	if(CLIcmd == 1)
		reg = UPDATE_FPGA;
	else if(CLIcmd == 2)
		reg = UPDATE_FX3;
	else ;
	camera->SetRegister(SETTINGS_CMD, reg);

	cout << "change Vendor Product success!"<<endl;
	camera->StopStream();
	camera->Close();
	delete camera;

}


bool Fresh_Device()
{
	static int assume_fx3_flag = 0;
	static uint assume_fx3_idProduct = 0;
	int r = libusb_init(&ctx);
	if(r < 0){
		exit(-1);
	}
	int cnt = 0;
	cnt = libusb_get_device_list(ctx, &devs); 	
	if(cnt < 0) {
		cout<<"Get Device Error"<<endl; 
		return false;
	}
	
	for(int i = 0; i < cnt; i++) {
		libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(devs[i], &desc);
		if (r < 0) {
			cout<<"failed to get device descriptor"<<endl;
			exit(-1);
		}
		if(desc.idVendor == 0x2d90){
			cout << "first discover device: "<<hex << desc.idVendor
								  <<":"<<hex << desc.idProduct<<endl;
			return true;
		}
		else if(desc.idVendor == 0x04b4 && assume_fx3_flag == 0){
			number_device = i;
			assume_fx3_flag = 1;
			assume_fx3_idProduct = desc.idProduct;
			cout << "second discover device: "<<hex << desc.idVendor
										<<":"<<hex << desc.idProduct<<endl;
			return true;
		}
		else if(desc.idVendor == 0x04b4 && assume_fx3_flag == 1 && desc.idProduct != assume_fx3_idProduct){
			number_device = i;
			cout << "last discover device: "<<hex << desc.idVendor
								  <<":"<<hex << desc.idProduct<<endl;
			return true;
		}

	}

	return false;
}







