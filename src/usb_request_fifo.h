#pragma once
// Bounded pointer FIFO. The caller serializes all operations. Erase by
// identity permits rejection while earlier completions have shifted entries.
namespace groove {
template <typename T, unsigned Capacity> class UsbRequestFifo {
 public:
  bool empty() const { return count_ == 0; }
  unsigned size() const { return count_; }
  T* front() const { return count_ ? entries_[0] : nullptr; }
  bool push(T* item) { if (!item || count_ == Capacity) return false; entries_[count_++]=item; return true; }
  bool erase(T* item) {
   for(unsigned i=0;i<count_;++i) if(entries_[i]==item) {
    for(unsigned j=i+1;j<count_;++j) entries_[j-1]=entries_[j];
    entries_[--count_]=nullptr; return true;
   }
   return false;
  }
 private:
  T* entries_[Capacity]{};unsigned count_=0;
};
}
