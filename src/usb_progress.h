#pragma once
#include <stdint.h>
namespace groove {
// Core0 task opens windows; its IRQ updates them. No stored device pointers.
// Activation is published last, so a preempting callback sees complete state.
class UsbProgress {
public:
 struct Result {unsigned callbacks=0,max_gap=0;};
 unsigned open(unsigned at){if(!__atomic_load_n(&enabled_,__ATOMIC_ACQUIRE))return 0;for(unsigned i=0;i<2;++i){auto& w=windows_[i];if(__atomic_load_n(&w.active,__ATOMIC_ACQUIRE))continue;w.token=++sequence_;if(!w.token)w.token=++sequence_;w.last=at;w.count=w.maximum=0;__atomic_store_n(&w.active,1u,__ATOMIC_RELEASE);return w.token;}return 0;}
 Result close(unsigned token,unsigned at){for(auto& w:windows_)if(__atomic_load_n(&w.active,__ATOMIC_ACQUIRE) && w.token==token){unsigned tail=at-w.last;Result r{w.count,tail>w.maximum?tail:w.maximum};__atomic_store_n(&w.active,0u,__ATOMIC_RELEASE);return r;}return {};}
 void enabled(bool enabled){__atomic_store_n(&enabled_,enabled,__ATOMIC_RELEASE);}
 void audio(unsigned at){if(!__atomic_load_n(&enabled_,__ATOMIC_ACQUIRE))return;for(auto& w:windows_)if(__atomic_load_n(&w.active,__ATOMIC_ACQUIRE)){unsigned gap=at-w.last;if(gap>w.maximum)w.maximum=gap;w.last=at;++w.count;}}
private:
 struct Window {unsigned active=0,token=0,last=0,count=0,maximum=0;}windows_[2];unsigned sequence_=0;bool enabled_=false;
};
inline UsbProgress display_usb_progress;
}
