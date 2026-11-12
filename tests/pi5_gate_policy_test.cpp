#include "../src/update/pi4_policy.h"
#include <cassert>
#include <cstdio>
// Host check for the Pi 5 tryboot gate rules (src/update/pi4_policy.h).
// Revision words are the real new-style codes: bit 23 set, board type in
// bits 4-11 (0x17 = Pi 5, 0x11 = Pi 4B), RAM code in bits 20-22, PCB
// revision in the low nibble. Build with the tree's host-test flags:
//   g++ -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined
//     tests/pi5_gate_policy_test.cpp -o build/test-pi5-gate-policy
// then run build/test-pi5-gate-policy.
int main(){
 using namespace groove::update;
 const uint32_t pi5_8gb_rev10=0xD04170,pi5_8gb_rev11=0xD04171,pi5_8gb_rev12=0xD04172;
 const uint32_t pi5_4gb_rev10=0xC04170,pi4_8gb_rev14=0xD03114,cm4=0xD03140;
 const uint32_t old_style_pi5_type=0x004170; // type bits 0x17 but bit 23 clear
 // Strict rule, rev 1.0 (low nibble 0): the ack is the only way in.
 assert(!pi5_tryboot_allowed_strict(pi5_8gb_rev10,false));
 assert(pi5_tryboot_allowed_strict(pi5_8gb_rev10,true));
 assert(!pi5_tryboot_allowed_strict(pi5_4gb_rev10,false));
 assert(pi5_tryboot_allowed_strict(pi5_4gb_rev10,true));
 // Strict rule, rev 1.1 and later: allowed with no ack at all.
 assert(pi5_tryboot_allowed_strict(pi5_8gb_rev11,false));
 assert(pi5_tryboot_allowed_strict(pi5_8gb_rev11,true));
 assert(pi5_tryboot_allowed_strict(pi5_8gb_rev12,false));
 // Wrong board type fails closed, ack or not.
 assert(!pi5_tryboot_allowed_strict(pi4_8gb_rev14,false));
 assert(!pi5_tryboot_allowed_strict(pi4_8gb_rev14,true));
 assert(!pi5_tryboot_allowed_strict(cm4,false));
 assert(!pi5_tryboot_allowed_strict(cm4,true));
 // Old-style encoding fails closed, ack or not; so does a zero revision.
 assert(!pi5_tryboot_allowed_strict(old_style_pi5_type,false));
 assert(!pi5_tryboot_allowed_strict(old_style_pi5_type,true));
 assert(!pi5_tryboot_allowed_strict(0,false));
 assert(!pi5_tryboot_allowed_strict(0,true));
 // The relaxed experiment rule is unchanged: encoding + type only.
 assert(pi5_tryboot_allowed(pi5_8gb_rev10,false));
 assert(pi5_tryboot_allowed(pi5_8gb_rev11,false));
 assert(!pi5_tryboot_allowed(pi4_8gb_rev14,false));
 assert(!pi5_tryboot_allowed(old_style_pi5_type,false));
 // The Pi 4 rule is untouched: rev 1.2+ free, rev 1.0/1.1 need the ack.
 assert(pi4_tryboot_allowed(0xC03112,false));
 assert(!pi4_tryboot_allowed(0xC03111,false));
 assert(pi4_tryboot_allowed(0xC03111,true));
 assert(pi4_tryboot_allowed(pi4_8gb_rev14,false));
 assert(!pi4_tryboot_allowed(pi5_8gb_rev11,true));
 std::puts("pi5 gate policy: strict truth table (rev 1.0/1.1 x ack, wrong type, old-style) PASS");
}
