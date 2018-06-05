#ifndef __FX3_UPDATE_H
#define __FX3_UPDATE_H

#include "stdio.h"
#include <iostream>
#include "stdlib.h"
#include "unistd.h"
#include "stdint.h"
#include "string.h"
#include "libusb-1.0/libusb.h"
#include "cyusb.h"

extern libusb_device_handle *handle;        //extern ＂从 main.cpp中定义的引到外部的意思＂
extern char * name_UpdateFile;
bool Updata_FX3(libusb_device *dev,int position);
bool IsBootLoaderRunning();

int
my_fx3_usbboot_download (
    cyusb_handle *h,
    const char   *filename);
int
fx3_i2cboot_download (
    cyusb_handle *h,
    const char   *filename);
        
int
fx3_spiboot_download (
        cyusb_handle *h,
        const char   *filename);


#endif