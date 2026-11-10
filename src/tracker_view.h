#pragma once
#include "project.h"
namespace groove {
inline unsigned tracker_row(unsigned center,int offset,unsigned length){if(!length)return 0;int n=(int(center%length)+offset)%int(length);return unsigned(n<0?n+int(length):n);}
inline bool tracker_hidden(const StepState& s){if(s.ratchet!=1 || s.gate!=96 || s.microtiming || s.probability!=100 || s.condition!=TrigCondition::Always || s.sound!=kNoPreset || s.lock_only)return true;for(auto n:s.chord)if(n!=255)return true;for(auto& l:s.locks)if(l.enabled)return true;return false;}
inline bool tracker_hidden(const KitStepState& s){if(s.ratchet!=1 || s.microtiming || s.probability!=100 || s.sample_lock!=255 || s.start_lock!=255 || s.pitch_lock || s.reverse_lock!=2 || s.filter_lock!=255 || s.level_lock!=255 || s.delay_lock!=255 || s.reverb_lock!=255)return true;for(auto l:s.fx_lock)if(l!=255)return true;return false;}
}
