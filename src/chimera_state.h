#pragma once
#include "parameters.h"
#include <stdint.h>

namespace groove {
constexpr unsigned kChimeraIpaBytes=1024;
struct ChimeraIpaText {
 uint16_t size=0;
 char data[kChimeraIpaBytes]{};
};
extern "C" bool yadokari_chimera_valid_ipa(const char*,unsigned);
enum ChimeraParameter : unsigned {
 ChimeraTongueX, ChimeraTongueY, ChimeraJaw, ChimeraLipRound, ChimeraBreath,
 ChimeraBody, ChimeraVibratoDepth, ChimeraVibratoRate, ChimeraGlide, ChimeraPressure, ChimeraBreathiness,
 ChimeraTipX, ChimeraTipY, ChimeraTipCurl, ChimeraRoot, ChimeraLipAperture, ChimeraLipProtrusion, ChimeraLipWidth,
 ChimeraLabiodental, ChimeraVelum, ChimeraLarynx, ChimeraEpilarynx, ChimeraVentricular, ChimeraLateral, ChimeraDorsum,
 ChimeraProgram, ChimeraSpecies, ChimeraVoiceMode, ChimeraUnison, ChimeraSpread, ChimeraSceneSpeak, ChimeraTalkOutput, kChimeraParameterCount
};
constexpr unsigned kChimeraMacroCount=5;
inline constexpr const char* kChimeraBodies[]={"MALE","FEMALE","CHILD"};
inline constexpr const char* kChimeraPrograms[]={"PLAY","BEATBOX","SPEAK","SING","BESTIARY"};
enum ChimeraVoiceModeValue : unsigned { ChimeraPoly, ChimeraMono, ChimeraLegato };
inline constexpr const char* kChimeraVoiceModes[]={"POLY","MONO","LEGATO"};
enum ChimeraProgramValue : unsigned { ChimeraPlay, ChimeraBeatbox, ChimeraSpeak, ChimeraSing, ChimeraBestiary };
inline constexpr const char* kChimeraSpecies[]={"SONGBIRD","SEAL","ALLIGATOR","ELEPHANT"};
inline constexpr ParameterDescriptor kChimeraDetails[]={
 {ChimeraBody,"BODY",0,2,0,Interpolation::Discrete,"",0,1},
 {ChimeraVibratoDepth,"VIBRATO DEPTH",0,100,0,Interpolation::Linear,"ct",0,1},
 {ChimeraVibratoRate,"VIBRATO RATE",1,12,6,Interpolation::Linear,"Hz",1,.1f},
 {ChimeraGlide,"GLIDE",2,500,12,Interpolation::Linear,"ms",0,1},
 {ChimeraPressure,"PRESSURE",-4,8,0,Interpolation::Linear,"",1,.1f},
 {ChimeraBreathiness,"BREATHINESS",-.3f,.7f,0,Interpolation::Linear,"",2,.01f},
 {ChimeraTipX,"TIP X",-3,3,0,Interpolation::Linear,"cm",2,.05f},
 {ChimeraTipY,"TIP Y",-2.5f,2.5f,0,Interpolation::Linear,"cm",2,.05f},
 {ChimeraTipCurl,"TIP CURL",0,1,0,Interpolation::Linear,"",2,.02f},
 {ChimeraRoot,"TONGUE ROOT",-1,1,0,Interpolation::Linear,"",2,.02f},
 {ChimeraLipAperture,"LIP APERTURE",-1.5f,1.5f,0,Interpolation::Linear,"cm",2,.02f},
 {ChimeraLipProtrusion,"LIP PROTRUSION",-1.5f,1.5f,0,Interpolation::Linear,"cm",2,.02f},
 {ChimeraLipWidth,"LIP WIDTH",-1,1,0,Interpolation::Linear,"",2,.02f},
 {ChimeraLabiodental,"LIP TO TEETH",0,1,0,Interpolation::Linear,"",2,.02f},
 {ChimeraVelum,"VELUM",-.25f,1.4f,-.15f,Interpolation::Linear,"",2,.02f},
 {ChimeraLarynx,"LARYNX",-1.6f,1.6f,0,Interpolation::Linear,"",2,.02f},
 {ChimeraEpilarynx,"EPILARYNX",0,1,0,Interpolation::Linear,"",2,.02f},
 {ChimeraVentricular,"VENTRICULAR",0,1,0,Interpolation::Linear,"",2,.02f},
 {ChimeraLateral,"LATERAL",0,1,0,Interpolation::Linear,"",2,.02f},
 {ChimeraDorsum,"DORSUM",0,1,0,Interpolation::Linear,"",2,.02f},
 {ChimeraProgram,"MODE",0,4,0,Interpolation::Discrete,"",0,1},
 {ChimeraSpecies,"SPECIES",0,3,0,Interpolation::Discrete,"",0,1},
 {ChimeraVoiceMode,"VOICE MODE",0,2,ChimeraPoly,Interpolation::Discrete,"",0,1},
 {ChimeraUnison,"UNISON",1,4,1,Interpolation::Discrete,"",0,1},
 {ChimeraSpread,"SPREAD",0,40,12,Interpolation::Linear,"ct",0,1},
 {ChimeraSceneSpeak,"SCENE SPEAK",0,1,0,Interpolation::Discrete,"",0,1},
 {ChimeraTalkOutput,"TALK TO TRACK",0,16,0,Interpolation::Discrete,"",0,1},
};
constexpr unsigned kChimeraDetailCount=kChimeraParameterCount-kChimeraMacroCount;
static_assert(sizeof kChimeraDetails/sizeof *kChimeraDetails==kChimeraDetailCount);
struct ChimeraState {
 float values[kChimeraDetailCount]{};
 constexpr ChimeraState(){for(unsigned i=0;i<kChimeraDetailCount;++i)values[i]=kChimeraDetails[i].initial;}
};
inline unsigned chimera_unison(const ChimeraState& s){return unsigned(s.values[ChimeraUnison-kChimeraMacroCount]);}
inline unsigned chimera_voice_mode(const ChimeraState& s){return unsigned(s.values[ChimeraVoiceMode-kChimeraMacroCount]);}
inline bool valid_chimera_ipa(const ChimeraIpaText& text){
 return !text.size || (text.size<=kChimeraIpaBytes && yadokari_chimera_valid_ipa(text.data,text.size));
}
inline constexpr bool chimera_sound_attribute(unsigned local){
 return local==ChimeraProgram || local==ChimeraSpecies || local==ChimeraVoiceMode || local==ChimeraUnison || local==ChimeraSceneSpeak || local==ChimeraTalkOutput;
}
inline constexpr uint8_t kChimeraModelCosts[]={2,2,2,1,2,3,2};
inline constexpr unsigned kChimeraPoolCapacity=4;
inline unsigned chimera_model(const ChimeraState& state){
 return state.values[ChimeraProgram-kChimeraMacroCount]==ChimeraBestiary?
        3+unsigned(state.values[ChimeraSpecies-kChimeraMacroCount]):unsigned(state.values[0]);
}
inline bool valid_chimera(const ChimeraState& state){
 for(unsigned i=0;i<kChimeraDetailCount;++i){
  const auto& descriptor=kChimeraDetails[i];float value=state.values[i];
  if(!(value>=descriptor.minimum && value<=descriptor.maximum))return false;
  if(descriptor.interpolation==Interpolation::Discrete && value!=float(unsigned(value)))return false;
 }
 return true;
}
}
