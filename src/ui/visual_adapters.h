#pragma once
#include "../performance.h"
#include "../spectral_voices.h"
#include "../wavetable_bank.h"
#include "../daisy_track_drive.h"
#include "../visual_model.h"
namespace groove { namespace ui {
inline VisualData pcm_visual(const Performance& perf,VisualSource source){
 VisualData d;const auto& frame=perf.visual();const auto* analysis=perf.analysis();unsigned tap=source==VisualSource::InputPcm || source==VisualSource::InputSpectrum?0:source==VisualSource::MainPcm || source==VisualSource::MainSpectrum?2:1;
 bool spectrum=source==VisualSource::TrackSpectrum || source==VisualSource::InputSpectrum || source==VisualSource::MainSpectrum;
 d.label=spectrum?"MEASURED FFT / MONO / 512 SAMPLES":"LIVE PCM / MONO / 10.7 MS";d.live=perf.visual_fresh();d.available=d.live && (tap!=1 || (frame.taps.track==perf.selected_track() && frame.taps.source==perf.visual_source()));if(source==VisualSource::CapturePcm)d.label=frame.taps.source==2?"CUE BUS / LIVE PCM":frame.taps.source==1?"INPUT BUS / LIVE PCM":frame.taps.source==0?"MAIN BUS / LIVE PCM":frame.taps.source==255?"TRACK POST FADER / LIVE PCM":frame.taps.source>=53 && frame.taps.source<=60?"GROUP CAPTURE / LIVE PCM":"CAPTURE SOURCE / LIVE PCM";
 if(spectrum){d.available=analysis;d.live=analysis;if(analysis){d.count=kVisualBands;for(unsigned i=0;i<d.count;++i)d.values[i]=analysis->spectrum[tap][i];}else d.label="FFT NOT AVAILABLE";}
 else if(analysis){d.count=kVisualScope;for(unsigned i=0;i<d.count;++i){d.values[i]=analysis->scope_high[tap][i];d.low[i]=analysis->scope_low[tap][i];}}
 else if(d.available){d.count=128;for(unsigned i=0;i<128;++i){float lo=1,hi=-1;for(unsigned j=0;j<4;++j){float sample=frame.taps.pcm[tap][i*4+j];lo=fminf(lo,sample);hi=fmaxf(hi,sample);}d.low[i]=lo;d.values[i]=hi;}}
 if(!d.available && !spectrum)d.label="WAITING FOR CURRENT SOURCE PCM";

 return d;
}
inline bool word_contains(const char* name,const char* token){for(;*name;++name){unsigned i=0;while(token[i] && name[i] && (name[i]>='a' && name[i]<='z'?name[i]-32:name[i])==token[i])++i;if(!token[i])return true;}return false;}
inline float recipe_value(ConstTrackView track,unsigned local,const Performance* perf);
inline void envelope_values(ConstTrackView t,float* out,const Performance* perf=nullptr){out[0]=.01f;out[1]=.1f;out[2]=.7f;out[3]=.2f;const auto* engine=engine_descriptor(unsigned(t.instrument));if(!engine)return;for(unsigned i=0;i<engine->parameters.count;++i){const auto& declaration=engine->parameters.declarations[i];if(word_contains(declaration.ui.group,"PITCH"))continue;int index=word_contains(declaration.value.name,"ATTACK")?0:word_contains(declaration.value.name,"DECAY")?1:word_contains(declaration.value.name,"SUSTAIN")?2:word_contains(declaration.value.name,"RELEASE")?3:-1;if(index<0)continue;float value=recipe_value(t,i,perf);if(declaration.value.unit && !__builtin_strcmp(declaration.value.unit,"ms"))value*=.001f;out[index]=value;}}
inline VisualData response_visual(ResponseModel model,float a,float b,float rate=48000){VisualData d;d.available=true;d.count=128;d.label="PARAMETER RESPONSE MODEL";DaisyTrackDrive drive;
 for(unsigned i=0;i<128;++i){float u=float(i)/127,x=u*2-1,y=u;
  if(model==ResponseModel::LowPass || model==ResponseModel::HighPass || model==ResponseModel::BandPass || model==ResponseModel::Notch){float hz=20*powf(fminf(rate*.45f,20000)/20,u),fc=20+(rate*.30f-20)*a*a,r=hz/fc,q=.5f+b*7.5f,den=sqrtf((1-r*r)*(1-r*r)+(r/q)*(r/q));float mag=model==ResponseModel::LowPass?1/den:model==ResponseModel::HighPass?r*r/den:model==ResponseModel::BandPass?(r/q)/den:fabsf(1-r*r)/den;y=clamp01((20*log10f(fmaxf(mag,.0001f))+60)/72);}
  else if(model==ResponseModel::Compressor){float db=-60+60*u,threshold=20*log10f(fmaxf(a,.0001f)),out=db>threshold?threshold+(db-threshold)/fmaxf(1,b):db;y=clamp01((out+60)/60);}
  else if(model==ResponseModel::Drive)y=.5f+drive.process(x,a)*.5f;
  else if(model==ResponseModel::Fold)y=.5f+asinf(sinf(x*(1+8*a)))/3.14159265f;
  else if(model==ResponseModel::Gate)y=fabsf(x)<.001f+.4f*a*a?.5f:u;
  else if(model==ResponseModel::Clip)y=.5f+fmaxf(-(1-.9f*b),fminf(1-.9f*b,x*(1+7*a)))*.5f;
  else if(model==ResponseModel::Quantize){float q=float(1u<<(1+unsigned((1-clamp01(b))*14)));y=.5f+roundf(x*q)/q*.5f;}
  else if(model==ResponseModel::Tilt)y=clamp01(.5f+(u-.5f)*(a-.5f)*1.5f);
  else if(model==ResponseModel::Width)y=clamp01(.5f+x*a);
  else if(model==ResponseModel::Modulation)y=.5f+.5f*sinf(u*6.2831853f*(1+4*a))*b;
  else if(model==ResponseModel::Decay)y=powf(fmaxf(.001f,a),u*8);
  d.values[i]=clamp01(y);
 }return d;}
// The idle diagram is a projection of declared controls, never a generated PCM trace.
inline float recipe_value(ConstTrackView track,unsigned local,const Performance* perf){uint16_t target=engine_target(track.instrument,local);if(perf)for(unsigned i=0;i<8;++i)if(perf->knob_parameter(i)==target)return perf->knob_display_value(i);return engine_parameter_value(track,local);}
inline float recipe_channel(ConstTrackView track,unsigned target,const Performance* perf){if(perf)for(unsigned i=0;i<8;++i)if(perf->knob_parameter(i)==target)return perf->knob_display_value(i);return track_parameter(track,uint16_t(target));}
inline float oscillator_model(unsigned wave,float phase,float width){phase-=floorf(phase);switch(wave){case 1:return phase<width?1:-1;case 2:return 1-4*fabsf(phase-.5f);case 3:return sinf(phase*6.283185307f);default:return phase*2-1;}}
inline VisualData control_recipe(ConstTrackView track,bool no_pcm,const Performance* perf=nullptr){
 VisualData d;d.available=true;d.presentation=VisualElement::Bars;d.label=no_pcm?"DECLARED CONTROLS / PCM NOT AVAILABLE":"DECLARED CONTROL RECIPE / IDLE";const auto* engine=engine_descriptor(unsigned(track.instrument));if(!engine)return d;
 if(engine->visual.recipe==VisualRecipe::Fm2){float ratio=fm_ratio(recipe_value(track,0,perf)),amount=fm_index(recipe_value(track,1,perf));d.count=128;d.presentation=VisualElement::Curve;d.bipolar=true;float level=recipe_channel(track,1,perf),drive=recipe_channel(track,4,perf);DaisyTrackDrive shaper;auto response=response_visual(ResponseModel::LowPass,recipe_channel(track,3,perf),0);d.layers=2;for(unsigned i=0;i<128;++i){float phase=float(i)/127*6.283185307f;d.values[i]=.5f+.5f*shaper.process(sinf(phase+amount*sinf(phase*ratio)),drive)*level;d.low[i]=response.values[i];}return d;}
 if(engine->visual.recipe==VisualRecipe::Oscillators && recipe_value(track,18,perf)==0 && recipe_value(track,19,perf)==0 && recipe_value(track,20,perf)==0 && recipe_value(track,22,perf)==0 && recipe_value(track,23,perf)==0){float detune=recipe_value(track,2,perf),blend=recipe_value(track,3,perf),sub=recipe_value(track,6,perf),width=recipe_value(track,8,perf);unsigned first=unsigned(recipe_value(track,0,perf)),second=unsigned(recipe_value(track,1,perf));d.count=128;d.presentation=VisualElement::Curve;d.bipolar=true;auto response=response_visual(ResponseModel::LowPass,sqrtf(fmaxf(0,(recipe_value(track,4,perf)-20)/(48000*.30f-20))),recipe_value(track,5,perf));d.layers=2;float a=powf(2,-detune/2400),b=powf(2,detune/2400+recipe_value(track,16,perf)/12);for(unsigned i=0;i<128;++i){d.low[i]=response.values[i];float phase=float(i)/127;d.values[i]=clamp01(.5f+.5f*((1-blend)*oscillator_model(first,phase*a,width)+blend*oscillator_model(second,phase*b,1-width)+sub*sinf(phase*3.141592654f))/(1+sub));}return d;}
 for(const auto& ref:engine->page.core){if(ref.scope!=EngineParameterScope::Engine || d.count>=8)continue;const auto* p=engine_parameter(track.instrument,ref.index);if(!p)continue;float raw=recipe_value(track,ref.index,perf),low=p->value.minimum,high=p->value.maximum;d.names[d.count]=p->value.name;d.parameters[d.count]=raw;d.values[d.count++]=clamp01((raw-low)/fmaxf(high-low,.000001f));}return d;
}
inline VisualData engine_visual_data(const VisualDeclaration& declaration,ConstTrackView track,const Performance& perf){
 unsigned t=perf.selected_track();const auto& frame=perf.visual();bool voice=perf.visual_fresh() && frame.instrument[t]==unsigned(track.instrument) && frame.pattern[t]==perf.editing_pattern();
 if(declaration.source==VisualSource::CapturePcm || declaration.source==VisualSource::TrackPcm || declaration.source==VisualSource::MainPcm || declaration.source==VisualSource::InputPcm || declaration.source==VisualSource::TrackSpectrum || declaration.source==VisualSource::InputSpectrum || declaration.source==VisualSource::MainSpectrum){auto d=pcm_visual(perf,declaration.source);if(declaration.element==VisualElement::Scope && declaration.fallback==VisualFallback::Parameters){float peak=0;for(unsigned i=0;i<d.count;++i)peak=fmaxf(peak,fmaxf(fabsf(d.values[i]),fabsf(d.low[i])));if(!d.available || peak<.00001f){auto recipe=control_recipe(track,!d.available,&perf);if(recipe.count)return recipe;}}if(declaration.source==VisualSource::TrackSpectrum && declaration.status==VisualStatus::TextureBuffer && d.available){static const char* states[]={"FFT / BUFFER RECORDING / 32K ST","FFT / BUFFER RECORDING / 32K MONO","FFT / BUFFER RECORDING / 16K ST","FFT / BUFFER RECORDING / 16K MONO","FFT / BUFFER FROZEN / 32K ST","FFT / BUFFER FROZEN / 32K MONO","FFT / BUFFER FROZEN / 16K ST","FFT / BUFFER FROZEN / 16K MONO"};d.label=states[(frame.texture_frozen[t]?4:0)+unsigned(track.texture.values[11])%4];}return d;}
 VisualData d;d.available=true;d.label=declaration.caption;
 if(declaration.source==VisualSource::Harmonics){d.count=16;d.live=voice && frame.voice_level_valid[t] && frame.voice_level[t]>.00001f;if(d.live){for(unsigned i=0;i<16;++i)d.values[i]=frame.harmonics[t][i]*frame.voice_level[t];d.label="RELATIVE PARTIALS X ACTUAL ENVELOPE";}else{float values[25];for(unsigned i=0;i<25;++i)values[i]=recipe_value(track,i,&perf);additive_spectrum(values,d.values);d.label="NORMALIZED HARMONIC RECIPE / IDLE";}float reference=0;for(unsigned i=0;i<16;++i)reference=fmaxf(reference,d.live?frame.harmonics[t][i]:d.values[i]);if(reference>.000001f)for(unsigned i=0;i<16;++i)d.values[i]/=reference;}
 else if(declaration.source==VisualSource::WavetableScan){d.count=128;d.live=voice && frame.voice_level[t]>.00001f;float idle[]={recipe_value(track,0,&perf),recipe_value(track,1,&perf),recipe_value(track,2,&perf)};const float* p=d.live?frame.wavetable_controls[t]:idle;d.phase=d.live?frame.wavetable_phase[t]:0;d.position=d.live?frame.wavetable_position[t]:clamp01(p[0]/127);d.range=true;d.range_low=clamp01((p[0]-p[1])/127);d.range_high=clamp01((p[0]+p[1])/127);d.bipolar=true;
  float index=d.position*127;unsigned first=unsigned(index),second=first<127?first+1:first;float blend=index-first;for(unsigned i=0;i<128;++i){unsigned sample=i*255/127;float a=kWavetableData[first][0][sample]/32768.f,b=kWavetableData[second][0][sample]/32768.f;d.values[i]=clamp01(.5f+.5f*(a+(b-a)*blend));}
  envelope_values(track,d.parameters,&perf);d.parameters[4]=p[2];d.layers=3;for(unsigned layer=0;layer<2;++layer){unsigned table=unsigned(clamp01(d.position+(layer?1:-1)*.08f)*127);for(unsigned i=0;i<64;++i)d.low[layer*64+i]=.5f+.5f*kWavetableData[table][0][i*255/63]/32768.f;}
  d.label=d.live?"TABLE MODEL / ACTUAL SCAN POSITION":"TABLE MODEL / DECLARED SCAN RANGE";}
 else if(declaration.source==VisualSource::Envelope){float p[4];envelope_values(track,p,&perf);float total=p[0]+p[1]+.3f+p[3],a=p[0]/total,b=(p[0]+p[1])/total,r=1-p[3]/total;d.count=128;for(unsigned i=0;i<128;++i){float x=float(i)/127;d.values[i]=x<a?x/fmaxf(a,.0001f):x<b?1-(1-p[2])*(x-a)/fmaxf(b-a,.0001f):x<r?p[2]:p[2]*(1-x)/fmaxf(1-r,.0001f);}d.point=voice && frame.voice_level_valid[t];d.phase=1;d.position=frame.voice_level[t];d.live=d.point;d.label=d.live?"ADSR MODEL / CURRENT LEVEL":"ADSR PARAMETER MODEL";}
 else if(declaration.source==VisualSource::Morph){d.count=2;d.values[0]=0;d.values[1]=1;d.phase=d.position=perf.visual_fresh()?frame.morph_position:perf.morph_position();d.point=true;d.live=perf.visual_fresh();d.label="A / B / ACTUAL MORPH POSITION";}
 else {d.available=false;d.label=declaration.caption;}
 return d;
}
}}
