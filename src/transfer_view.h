#pragma once
#include <stdint.h>
namespace groove {
struct TransferView {
 bool active=false,started=false,station=false,failed=false,usb=false;char ssid[40]{},address[80]{},discovery[80]{},code[16]{},status[80]{};
 unsigned remaining=0;uint64_t received=0,length=0;
};
}
