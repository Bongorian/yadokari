#pragma once
#include <stdint.h>
#include <stddef.h>
namespace groove { namespace update {
// Physical UART commands are disabled unless DATA:/REMOTE.ON exists. Exact,
// bounded lines only; malformed or overlong input never triggers an action.
enum class SerialCommand:uint8_t { None, Status, Enter, Ethernet, Wifi, Usb, Trial, Confirm, Boot, Cancel, Log, Benchmark, Hardware, PlaitsStress, PlaitsFast, DrumDemo, LoopTest, SampleTest, FxStress, SaveTest, KitSwap, ChopTest, KitRecordTest, UiTest, MidiTest, NativeTest, AudioTest, InputTest, PresetCheck, Wave4Test, FinalTest, DspTest, DspStress, Efficiency, EfficiencyTail, EfficiencySampler, DeployCheck, OptimizationTest };
class SerialControl {
 char line_[40]{};unsigned used_=0;bool discard_=false;
 static bool same(const char* a,const char* b){while(*a && *a==*b){++a;++b;}return *a==*b;}
public:
 bool idle()const{return used_==0 && !discard_;}
 SerialCommand feed(uint8_t c){
  if(c=='\r')return SerialCommand::None;
  if(c=='\n'){
   if(discard_){discard_=false;used_=0;return SerialCommand::None;}
   line_[used_]=0;used_=0;
   struct Entry{const char* text;SerialCommand value;};
   static const Entry commands[]={{"!GBX1 STATUS",SerialCommand::Status},{"!GBX1 ENTER",SerialCommand::Enter},{"!GBX1 ETHERNET",SerialCommand::Ethernet},{"!GBX1 WIFI",SerialCommand::Wifi},{"!GBX1 USB",SerialCommand::Usb},{"!GBX1 TRIAL",SerialCommand::Trial},{"!GBX1 CONFIRM",SerialCommand::Confirm},{"!GBX1 BOOT",SerialCommand::Boot},{"!GBX1 CANCEL",SerialCommand::Cancel},{"!GBX1 LOG",SerialCommand::Log},{"!GBX1 BENCHMARK",SerialCommand::Benchmark},{"!GBX1 HARDWARE",SerialCommand::Hardware},{"!GBX1 PLSTRESS",SerialCommand::PlaitsStress},{"!GBX1 PLFAST",SerialCommand::PlaitsFast},{"!GBX1 DRUMDEMO",SerialCommand::DrumDemo},{"!GBX1 LOOPTEST",SerialCommand::LoopTest},{"!GBX1 SAMPLETEST",SerialCommand::SampleTest},{"!GBX1 FXSTRESS",SerialCommand::FxStress},{"!GBX1 SAVETEST",SerialCommand::SaveTest},{"!GBX1 KITSWAP",SerialCommand::KitSwap},{"!GBX1 CHOPTEST",SerialCommand::ChopTest},{"!GBX1 KITREC",SerialCommand::KitRecordTest},{"!GBX1 UITEST",SerialCommand::UiTest},{"!GBX1 MIDITEST",SerialCommand::MidiTest},{"!GBX1 NATIVETEST",SerialCommand::NativeTest},{"!GBX1 AUDIOTEST",SerialCommand::AudioTest},{"!GBX1 INPUTTEST",SerialCommand::InputTest},{"!GBX1 PRESETCHECK",SerialCommand::PresetCheck},{"!GBX1 WAVE4TEST",SerialCommand::Wave4Test},{"!GBX1 FINALTEST",SerialCommand::FinalTest},{"!GBX1 DSPTEST",SerialCommand::DspTest},{"!GBX1 DSPSTRESS",SerialCommand::DspStress},{"!GBX1 EFFICIENCY",SerialCommand::Efficiency},{"!GBX1 EFFTAIL",SerialCommand::EfficiencyTail},{"!GBX1 EFFSAMPLER",SerialCommand::EfficiencySampler},{"!GBX1 DEPLOYCHECK",SerialCommand::DeployCheck},{"!GBX1 OPTTEST",SerialCommand::OptimizationTest}};
   for(const auto& command:commands)if(same(line_,command.text))return command.value;
   return SerialCommand::None;
  }
  if(discard_)return SerialCommand::None;
  if(c<32 || c>126 || used_>=sizeof(line_)-1){discard_=true;used_=0;return SerialCommand::None;}
  line_[used_++]=char(c);return SerialCommand::None;
 }
};
}}
