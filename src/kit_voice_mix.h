#pragma once

namespace groove {
// Per-voice mixer state. All parameters are initialized at note-on, so a
// step lock affects this hit without ramping from the previous voice owner.
struct KitVoiceMix {
 float cutoff=1,level=1,pan=0,delay=0,reverb=0,filter_l=0,filter_r=0;
 void reset(float c,float l,float p,float d,float r){cutoff=c;level=l;pan=p;delay=d;reverb=r;filter_l=filter_r=0;}
 void process(float& l,float& r,float target_cutoff,float target_level,float target_pan,float target_delay,float target_reverb,float alpha,float& delay_l,float& delay_r,float& reverb_l,float& reverb_r){
  cutoff+=(target_cutoff-cutoff)*alpha;level+=(target_level-level)*alpha;pan+=(target_pan-pan)*alpha;delay+=(target_delay-delay)*alpha;reverb+=(target_reverb-reverb)*alpha;
  if(cutoff<0.999f){float c=0.001f+cutoff*cutoff*0.95f;filter_l+=(l-filter_l)*c;filter_r+=(r-filter_r)*c;l=filter_l;r=filter_r;}else{filter_l=l;filter_r=r;}
  l*=level*(pan>0?1-pan:1);r*=level*(pan<0?1+pan:1);
  delay_l+=l*delay;delay_r+=r*delay;reverb_l+=l*reverb;reverb_r+=r*reverb;
 }
};
}
