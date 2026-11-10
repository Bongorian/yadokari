#pragma once
namespace groove {
class RecoveryFramePublisher {
 bool first_=true;
public:
 template<class Updater,class Canvas,class Display> bool draw(Updater& updater,Canvas& canvas,Display& display,bool connected){
  if(!first_ && connected && !display.FrameComplete())return false;
  updater.Draw(canvas);display.NewFrame();first_=false;return true;
 }
};
}
