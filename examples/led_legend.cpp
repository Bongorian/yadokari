// Render the legend from the same encoder/palette as Desktop and Circle.
#include "ui/pad_feedback.h"
#include <cstdio>
using namespace groove::ui;
int main(){
 const Theme& theme=Theme::by_index(Theme::kDefault);
 struct Row{const char* name;LedState state;};
 const Row rows[]={{"OFF",LedState::Off},{"IDLE",LedState::Idle},{"ASSIGNED",LedState::Assigned},{"SELECTED",LedState::Selected},{"EDITING",LedState::Editing},{"PLAYING",LedState::Playing},{"TAP / RELEASED",LedState::Triggered},{"MUTED",LedState::Muted},{"PATTERN MUTE",LedState::MutedPattern},{"BOTH MUTES",LedState::MutedBoth},{"PERFORMANCE",LedState::PerformanceActive},{"QUEUED",LedState::Queued},{"ARMED",LedState::Armed},{"FINALIZING",LedState::Finalizing},{"RECORDING",LedState::Recording},{"WARNING",LedState::Warning}};
 std::printf("<svg xmlns='http://www.w3.org/2000/svg' width='1000' height='790' viewBox='0 0 1000 790'>\n<rect width='1000' height='790' fill='#%06x'/><g font-family='sans-serif' fill='#%06x'>\n",theme.background&0xffffff,theme.text&0xffffff);
 std::puts("<text x='28' y='38' font-size='24'>PAD COLORS / HOST PREVIEW</text><text x='28' y='64' font-size='13'>Final indexed wire preview. Event hues are reserved; physical colors and brightness are not calibrated.</text>");
 const char* names[]={"FULL / hold + 120 ms hit","REDUCED / hold + waiting","OFF / no timed animation"};
 const Motion motions[]={Motion::Full,Motion::Reduced,Motion::Off};
 for(unsigned column=0;column<3;++column)std::printf("<text x='%u' y='102' font-size='14'>%s</text>",235+column*250,names[column]);
 for(unsigned row=0;row<sizeof rows/sizeof rows[0];++row){
  unsigned y=130+row*37;
  std::printf("<text x='28' y='%u' font-size='13'>%s</text>",y+17,rows[row].name);
  for(unsigned column=0;column<3;++column)for(unsigned step=0;step<4;++step){
   auto state=rows[row].state;
   if(state==LedState::Triggered && (step || motions[column]!=Motion::Full))state=LedState::Assigned;
   const auto color=PadPalette::rgb(state,theme,{},step*250,motions[column]);
   std::printf("<rect x='%u' y='%u' width='35' height='25' rx='5' stroke='#%06x' fill='#%06x'/>",235+column*250+step*46,y,theme.dim&0xffffff,color&0xffffff);
  }
 }
 std::puts("<text x='28' y='752' font-size='13'>Each row: 0 / 250 / 500 / 750 ms. TAP is a released short press; Full retains 120 ms of feedback.</text><text x='28' y='775' font-size='13'>Track hue: 55% ready / 100% selected / 18% muted. Motion: play. Warn: wait. Danger: record. Empty: off.</text></g></svg>");
}
