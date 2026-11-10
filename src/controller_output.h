#pragma once
#include "backends.h"
namespace groove {
// Latest desired state remains pending until its backend accepts it; an async
// backend owns retry and retains its state until a full successful ACK. Reconnection
// invalidates both reports, even when the requested LED values did not change.
class ControllerOutputPump {
public:
 enum class Attempt { Idle, Sent, Failed };
 void update(const uint8_t pads[16],const uint8_t buttons[62],const uint8_t* strip=nullptr) {for(unsigned i=0;i<25;++i){uint8_t value=strip?strip[i]:0;if(strip_[i]!=value){strip_[i]=value;pads_dirty_=true;}}for(unsigned i=0;i<16;++i)if(pads_[i]!=pads[i]){pads_[i]=pads[i];pads_dirty_=true;}for(unsigned i=0;i<62;++i)if(buttons_[i]!=buttons[i]){buttons_[i]=buttons[i];buttons_dirty_=true;}}
 void invalidate(){pads_dirty_=buttons_dirty_=true;}
 bool flush(IControllerOutput& backend){if(pads_dirty_ && backend.set_pad_strip_leds(pads_,strip_))pads_dirty_=false;if(buttons_dirty_ && backend.set_button_leds(buttons_))buttons_dirty_=false;return !pads_dirty_ && !buttons_dirty_;}
 // Limit a task iteration to one USB write; alternate reports after failures
 // so a broken pad report does not permanently starve the button LEDs.
 Attempt flush_one(IControllerOutput& backend,bool allow_pad,bool allow_buttons,bool& pad_attempt){
  const bool can_pad=pads_dirty_ && allow_pad,can_buttons=buttons_dirty_ && allow_buttons;
  bool pad=can_pad && (next_pad_ || !can_buttons);
  if(pad){pad_attempt=true;next_pad_=false;if(!backend.set_pad_strip_leds(pads_,strip_))return Attempt::Failed;pads_dirty_=false;return Attempt::Sent;}
  if(can_buttons){pad_attempt=false;next_pad_=true;if(!backend.set_button_leds(buttons_))return Attempt::Failed;buttons_dirty_=false;return Attempt::Sent;}
  return Attempt::Idle;
 }
private:uint8_t pads_[16]{},buttons_[62]{},strip_[25]{};bool pads_dirty_=true,buttons_dirty_=true,next_pad_=true;
};
}
