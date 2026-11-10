#include "project.h"
#include "creative_voice.h"
#include <cassert>
#include <cmath>
int main(){groove::CreativeVoice voice;voice.reset();groove::CreativeState c;voice.trigger(c,unsigned(groove::Instrument::WestCoast));float p[5]={0.8f,0.3f,0.5f,0.5f,0.5f};float peak=0;for(unsigned i=0;i<4800;++i){float x=voice.next(unsigned(groove::Instrument::WestCoast),220,48000,120,p,c,nullptr,0,false,0);assert(std::isfinite(x));peak=std::fmax(peak,std::fabs(x));}assert(peak>0.01f);}
