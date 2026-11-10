#pragma once
#include <stdint.h>
namespace groove { namespace ui {
// At most three distinct rendered frames. No pending animation FIFO and no
// relationship to controller ownership. begin() interrupts the previous slide.
class PageTransition {
 uint64_t started_=0,last_=~uint64_t(0);unsigned frame_=3,mask_=0;int direction_=1;
public:
 void begin(uint64_t now,int direction){started_=now;last_=~uint64_t(0);frame_=0;mask_=3;direction_=direction<0?-1:1;}
 void cancel(){frame_=3;mask_=0;}
 void changed_sides(unsigned mask){mask_=mask;if(!mask)cancel();}
 bool active()const{return frame_<3 && mask_;}
 unsigned sides()const{return mask_;}
 int offset(uint64_t now){if(!active())return 0;if(now-started_>=100){cancel();return 0;}if(last_!=now){last_=now;++frame_;}return direction_*int((3-frame_)*8);}
};
}}
