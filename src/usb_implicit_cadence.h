#pragma once
// Bounded rendezvous for one-packet USB implicit-feedback streams. Calls are
// serialized by the transport's state lock. No nominal-rate feedback is made.
namespace groove {
class UsbImplicitCadence {
 public:
  // Experimental headroom for delayed USB completions. Entries remain actual
  // captured packet lengths; no nominal or fabricated feedback is generated.
  static constexpr unsigned Capacity = 4096;
  void reset(unsigned slots) { read_=write_=count_=captures_=takes_=completions_=zero_captures_=dropped_=0; ready_=limit_=slots; }
  bool capture(unsigned frames) {
   // An xHCI TX stall can outlive the old 128 ms queue. Keep the newest
   // measured packet lengths and resume after TX completions return.
   // Discarding stale cadence is preferable to canceling the USB stream.
   if (count_ == Capacity) { unsigned remove=Capacity-16;read_=(read_+remove)%Capacity;count_-=remove;dropped_+=remove; }
   ++captures_; if (!frames) ++zero_captures_; entries_[write_]=frames; write_=(write_+1)%Capacity; ++count_; return true;
  }
  bool completed() { if (ready_>=limit_) return false; ++ready_; ++completions_; return true; }
  bool take(unsigned& frames) {
   if (!ready_ || !count_) return false;
   frames=entries_[read_]; read_=(read_+1)%Capacity; --count_; --ready_; ++takes_; return true;
  }
  unsigned captures() const { return captures_; }
  unsigned takes() const { return takes_; }
  unsigned completions() const { return completions_; }
  unsigned zero_captures() const { return zero_captures_; }
  unsigned queued() const { return count_; }
  unsigned ready() const { return ready_; }
  unsigned dropped() const { return dropped_; }
 private:
  unsigned entries_[Capacity]{},read_=0,write_=0,count_=0,ready_=0,limit_=0,captures_=0,takes_=0,completions_=0,zero_captures_=0,dropped_=0;
};
}
