#include "../src/usb_pcm_convert.h"
#include <cassert>
#include <cstdint>
#include <cstring>
int main() {
 const uint8_t packed[]={0x00,0x00,0x80, 0xff,0xff,0x7f, 0x01,0x02,0x03};
 uint8_t data[32]{};std::memcpy(data,packed,sizeof packed);
 assert(groove::usb_pcm24_capture_inplace(data,9,3,4,sizeof data));
 const uint8_t right[]={0,0,0x80,0,0xff,0xff,0x7f,0,1,2,3,0};assert(!std::memcmp(data,right,sizeof right));
 uint8_t wire[]={0,0,0,0x80, 0,0xff,0xff,0x7f, 0,1,2,3};std::memcpy(data,wire,sizeof wire);
 assert(groove::usb_pcm24_capture_inplace(data,12,4,3,sizeof data));assert(!std::memcmp(data,packed,sizeof packed));
 std::memcpy(data,wire,sizeof wire);assert(groove::usb_pcm24_capture_inplace(data,12,4,4,sizeof data));assert(!std::memcmp(data,right,sizeof right));
 uint8_t unaligned[]={0x12,0x34,0x56,0x78};std::memcpy(data,unaligned,sizeof unaligned);
 assert(groove::usb_pcm24_capture_inplace(data,4,4,4,sizeof data));
 const uint8_t retained[]={0x34,0x56,0x78,0x12};assert(!std::memcmp(data,retained,sizeof retained));
 assert(!groove::usb_pcm24_capture_inplace(data,9,3,4,11));
 assert(!groove::usb_pcm24_capture_inplace(data,10,3,4,sizeof data));
 assert(!groove::usb_pcm24_capture_inplace(data,9,2,4,sizeof data));
 assert(!groove::usb_pcm24_capture_inplace(nullptr,0,3,4,0));
}
