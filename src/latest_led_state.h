#pragma once
#include <stdint.h>
namespace groove {
// Task-owned desired and acknowledged states; one immutable report is in
// flight. Enqueue is acceptance, never an acknowledgement of USB success.
class LatestLedState {
public:
 static constexpr unsigned capacity=81;
 bool update(unsigned kind,const uint8_t* data,unsigned size){if(kind>1 || !data || !size || size>capacity)return false;auto& r=reports_[kind];for(unsigned i=0;i<size;++i)r.desired[i]=data[i];r.size=size;return true;}
 bool prepare(uint8_t* packet,unsigned& kind,unsigned& size){if(active_ || !packet)return false;for(unsigned n=0;n<2;++n){unsigned k=(next_+n)%2;auto& r=reports_[k];if(!dirty(r))continue;kind=active_kind_=k;size=active_size_=r.size;for(unsigned i=0;i<size;++i)packet[i]=sent_[i]=r.desired[i];active_=true;next_=1-k;return true;}return false;}
 void complete(bool success){if(!active_)return;if(success){auto& r=reports_[active_kind_];for(unsigned i=0;i<active_size_;++i)r.ack[i]=sent_[i];r.ack_size=active_size_;r.valid=true;}else reports_[active_kind_].valid=false;active_=false;}
 bool pending()const{return active_ || dirty(reports_[0]) || dirty(reports_[1]);}
private:
 struct Report{uint8_t desired[capacity]{},ack[capacity]{};unsigned size=0,ack_size=0;bool valid=false;};
 static bool dirty(const Report& r){if(!r.size)return false;if(!r.valid || r.size!=r.ack_size)return true;for(unsigned i=0;i<r.size;++i)if(r.desired[i]!=r.ack[i])return true;return false;}
 Report reports_[2]{};uint8_t sent_[capacity]{};unsigned active_kind_=0,active_size_=0,next_=0;bool active_=false;
};
}
