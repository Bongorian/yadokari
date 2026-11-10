#include "update/input.h"
#include <cassert>
int main(){
 groove::update::ButtonLatch buttons;
 assert(buttons.update(9, 0x08, true)==0);
 assert(buttons.update(9, 0x08, false)==0);
 assert(buttons.update(9, 0, false)==0);
 assert(buttons.update(9, 0x08, false)==0x08);
 assert(buttons.update(9, 0x08, false)==0);
 assert(buttons.update(1, 0x80, true)==0);
 assert(buttons.update(1, 0, false)==0);
 assert(buttons.update(1, 0x80, false)==0x80);
 assert(buttons.update(2, 0xff, false)==0);
}
