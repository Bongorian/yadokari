#pragma once
#include <stddef.h>
#include <stdint.h>
namespace groove {
struct Endpoint { uint8_t interface_number, alternate, interface_class, address, attributes, interval; uint16_t max_packet; uint8_t interface_subclass; };
// Strict bounds checking; callers choose policy, never assume interface or endpoint numbers.
template<class F> bool visit_endpoints(const uint8_t* data,size_t n,F emit) {
 if(!data || n<9 || data[0]<9 || data[1]!=2) return false;
 size_t total=size_t(data[2])|(size_t(data[3])<<8); if(total<9 || total>n) return false;
 uint8_t number=0,alt=0,cls=0,subclass=0; bool have_interface=false;
 for(size_t i=0;i<total;) {
  if(total-i<2) return false;
  size_t len=data[i]; if(len<2 || len>total-i) return false;
  if(data[i+1]==4) { if(len<9) return false; number=data[i+2]; alt=data[i+3]; cls=data[i+5]; subclass=data[i+6]; have_interface=true; }
  if(data[i+1]==5) { if(len<7 || !have_interface) return false; emit(Endpoint{number,alt,cls,data[i+2],data[i+3],data[i+6],uint16_t(data[i+4]|(uint16_t(data[i+5])<<8)),subclass}); }
  i+=len;
 }
 return true;
}

// MK3 interface numbers are reference data, not an identification rule.
// A unique endpoint shape within the selected configuration is required; an
// ambiguous configuration must not be bound to either controller driver.
enum class Mk3InterfaceRole:uint8_t { None, Hid, Display };
inline Mk3InterfaceRole mk3_interface_role(const uint8_t* data,size_t n,uint8_t number,uint8_t alternate) {
 if(!visit_endpoints(data,n,[](Endpoint){}))return Mk3InterfaceRole::None;
 uint8_t hid[32]{},display[32]{};bool target_hid=false,target_display=false,active=false;
 uint8_t current_number=0,current_alt=0,current_class=0;unsigned interrupt_in=0,interrupt_out=0,bulk_out=0;
 auto finish=[&](){if(!active)return;bool h=current_class==3 && interrupt_in==1 && interrupt_out==1;bool d=current_class==0xff && bulk_out==1;
  if(h)hid[current_number/8]|=uint8_t(1u<<(current_number%8));
  if(d)display[current_number/8]|=uint8_t(1u<<(current_number%8));
  if(current_number==number && current_alt==alternate){target_hid|=h;target_display|=d;}};
 visit_endpoints(data,n,[&](Endpoint e){if(!active || current_number!=e.interface_number || current_alt!=e.alternate){finish();active=true;current_number=e.interface_number;current_alt=e.alternate;current_class=e.interface_class;interrupt_in=interrupt_out=bulk_out=0;}
  if(!(e.max_packet&0x7ff))return;
  unsigned type=e.attributes&3;if(type==3){if(e.address&0x80)++interrupt_in;else ++interrupt_out;}else if(type==2 && !(e.address&0x80))++bulk_out;});
 finish();unsigned hcount=0,dcount=0;for(unsigned i=0;i<32;++i)for(unsigned bit=0;bit<8;++bit){hcount+=(hid[i]>>bit)&1;dcount+=(display[i]>>bit)&1;}
 if(target_hid && hcount==1)return Mk3InterfaceRole::Hid;
 if(target_display && dcount==1)return Mk3InterfaceRole::Display;
 return Mk3InterfaceRole::None;
}
}
