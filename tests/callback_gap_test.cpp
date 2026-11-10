#include "../src/callback_gap.h"
#include <cassert>
int main(){
 groove::CallbackGap gap;gap.record(0xfffffff0u);gap.record(100);gap.record(600);gap.record(1101);gap.record(2102);
 auto s=gap.stats();assert(s.maximum==1001 && s.over500==2 && s.over1000==1);
}
