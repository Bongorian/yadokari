#pragma once

namespace groove {
// Lossy oral waveguide. Section areas are squared inner-to-outer distances
// sampled from the 16 sections of CHIMERA's male HumanAirway geometry.
// The four corners are O, A, U, I. E is an intermediate front vowel.
class TalkTract {
 static constexpr unsigned sections=16;
 static constexpr float area[5][sections]={
  {2.007f,1.456f,1.071f,.346f,.352f,1.199f,1.552f,.657f,.926f,1.799f,5.495f,7.479f,6.948f,5.703f,3.290f,.171f},
  {2.029f,1.652f,2.460f,2.295f,4.302f,7.161f,7.634f,4.747f,4.599f,5.199f,7.110f,7.602f,6.765f,5.595f,3.963f,.836f},
  {2.008f,1.466f,1.403f,.599f,.188f,.332f,.274f,.035f,.035f,.244f,2.663f,4.056f,3.620f,2.692f,1.785f,.097f},
  {2.072f,2.072f,7.648f,9.548f,9.670f,10.540f,8.881f,4.027f,2.230f,.843f,.065f,.035f,.035f,.035f,1.567f,.907f},
  {2.068f,2.028f,7.823f,8.887f,7.923f,9.053f,8.023f,4.027f,2.954f,2.129f,1.221f,.694f,.364f,.226f,2.552f,.865f}
 };
 float right_[sections]{},left_[sections]{},reflection_[sections-1]{};
 float x_=-1,y_=-1;
 unsigned phase_=0;
public:
 void reset(){*this=TalkTract{};}
 float process(float excitation,float x,float y){
  if(!phase_){
   if(x_<0){x_=x;y_=y;}
   const bool update=x!=x_ || y!=y_ || !initialized_;
   if(update){
    x_=__builtin_fabsf(x-x_)<.0001f?x:x_+.25f*(x-x_);
    y_=__builtin_fabsf(y-y_)<.0001f?y:y_+.25f*(y-y_);
    const float front_mid=1-__builtin_fabsf(2*y_-1);
    float previous=0;
    for(unsigned i=0;i<sections;++i){
     float back=area[0][i]+y_*(area[2][i]-area[0][i]);
     float front=(y_<.5f?area[1][i]:area[3][i])*(1-front_mid)+area[4][i]*front_mid;
     float next=back+x_*(front-back);
     if(i)reflection_[i-1]=(previous-next)/(previous+next);
     previous=next;
    }
    initialized_=true;
   }
  }
  phase_=(phase_+1)&31;
  float forward=.25f*excitation+.70f*left_[0],out=right_[sections-1];
  for(unsigned i=0;i<sections-1;++i){
   float scatter=reflection_[i]*(right_[i]+left_[i+1]);
   float transmitted=.989f*(right_[i]-scatter);
   left_[i]=.989f*(left_[i+1]+scatter);
   right_[i]=forward;forward=transmitted;
  }
  right_[sections-1]=forward;left_[sections-1]=-.78f*out;
  return out;
 }
private:
 bool initialized_=false;
};
}
