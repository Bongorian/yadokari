#pragma once
// Audio endpoint requests complete in submission order on the same USB pipe.
// The caller serializes submit/completion using the transport state lock.
namespace groove {
class UsbReceiveLengths {
 public:
  void reset(unsigned slots=2){completed_=0;slots_=slots && slots<=Capacity?slots:2;for(auto& value:expected_)value=0;}
  void submitted(unsigned buffer,unsigned bytes){if(buffer<slots_)expected_[buffer]=bytes;}
  bool completed(unsigned bytes){const unsigned limit=expected_[completed_];completed_=(completed_+1)%slots_;return bytes!=~0u && bytes<=limit;}
 private:
  static constexpr unsigned Capacity=8;
  unsigned completed_=0,slots_=2,expected_[Capacity]{};
};
}
