#pragma once
#include "project.h"
namespace groove {
inline uint8_t transform_note(int input,ConstTrackView state) {
 int note=input+state.transpose;if(note<0)note=0;if(note>127)note=127;
 while(note>0 && !(state.scale_mask&(1u<<((note+12-state.scale_root)%12)))) --note;
 while(note<127 && !(state.scale_mask&(1u<<((note+12-state.scale_root)%12))))++note;
 return uint8_t(note);
}
inline unsigned input_cycle(int value,unsigned count){int n=value%int(count);return unsigned(n<0?n+int(count):n);}
// One distinct scale degree per pad. No wrapped duplicate at MIDI limits.
inline int keyboard_input(ConstTrackView state,unsigned pad,int octave=0){
 if(pad>=16)return -1;
 if(state.instrument==Instrument::SoundFont && state.soundfont.values[0]==128)return int(36+pad);
 int base=int(state.root_note)+12*octave;if(base<0 || base>127)return -1;
 int pitch=transform_note(base,state);while(pad--){do{if(++pitch>127)return -1;}while(!(state.scale_mask&(1u<<((pitch+12-state.scale_root)%12))));}
 int source=pitch-state.transpose;return source>=0 && source<=127?source:-1;
}
inline void pitch_name(unsigned note,char* text,bool octave=true){static const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};const char* p=names[note%12];unsigned at=0;while(*p)text[at++]=*p++;if(octave){int o=int(note/12)-1;if(o<0){text[at++]='-';o=-o;}text[at++]=char('0'+o);}text[at]=0;}
// Generate a triad in the current scale while retaining source notes for
// recording and release. Chromatic mode uses a major triad.
inline unsigned chord_base(int input,ConstTrackView state,uint8_t* out,unsigned shape=0) {
 if(input<0 || input>127 || shape>=kChordShapes)return 0;
 if(shape){unsigned count=0;for(unsigned i=0;i<4;++i){unsigned interval=chord_shape_definition(shape).notes[i];if(interval==255 || input+int(interval)>127)break;int pitch=input+int(interval)+state.transpose;if(pitch>=0 && pitch<=127)out[count++]=uint8_t(input+interval);}return count;}
 out[0]=uint8_t(input);unsigned count=1;int base=transform_note(input,state);
 for(unsigned chord=1;chord<3;++chord){
  int pitch=base;
  if(state.scale_mask==0x0fff)pitch+=chord==1?4:7;
  else {unsigned degrees=chord*2;while(degrees--){do{if(pitch>=127){pitch=128;break;}++pitch;}while(!(state.scale_mask&(1u<<((pitch+12-state.scale_root)%12))));if(pitch>127)break;}}
  int source=pitch-int(state.transpose);
  if(pitch>127 || source<0 || source>127 || transform_note(source,state)!=pitch)continue;
  bool duplicate=false;for(unsigned i=0;i<count;++i)if(transform_note(out[i],state)==pitch)duplicate=true;
  if(!duplicate)out[count++]=uint8_t(source);
 }
 return count;
}
// Inversion moves the lowest playable source note up an octave. Its scale
// mapping moves by the same octave, so the sounding order remains consistent.
inline unsigned chord_inputs(int input,ConstTrackView state,uint8_t* out,unsigned shape=0,unsigned inversion=0) {
 unsigned count=chord_base(input,state,out,shape);if(inversion>3 || count<2)return count;
 for(unsigned turn=0;turn<inversion;++turn){unsigned lowest=0;for(unsigned i=1;i<count;++i)if((shape?out[i]:transform_note(out[i],state))<(shape?out[lowest]:transform_note(out[lowest],state)))lowest=i;if(out[lowest]>115 || int(out[lowest])+12+state.transpose>127)break;out[lowest]=uint8_t(out[lowest]+12);}
 for(unsigned i=1;i<count;++i){uint8_t value=out[i];unsigned j=i;while(j && (shape?out[j-1]:transform_note(out[j-1],state))>(shape?value:transform_note(value,state))){out[j]=out[j-1];--j;}out[j]=value;}
 unsigned unique=0;for(unsigned i=0;i<count;++i)if(!unique || (shape?out[i]:transform_note(out[i],state))!=(shape?out[unique-1]:transform_note(out[unique-1],state)))out[unique++]=out[i];count=unique;
 return count;
}
}
