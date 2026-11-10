#pragma once
namespace groove {class Engine;struct ProjectState;class Performance;class SessionFiles;}
class CProjectStorage;class CircleDisplay;class CMk3Display;class CMk3Audio;
class CUiProbe {
 struct State;State* state_=nullptr;bool completed_=false;
public:
 ~CUiProbe();bool busy()const{return state_!=nullptr;}bool completed()const{return completed_;}
 bool start(groove::Engine&,groove::ProjectState&,groove::Performance&,groove::SessionFiles&,CProjectStorage&,CMk3Audio*);
 void service(groove::Engine&,groove::ProjectState&,groove::Performance&,groove::SessionFiles&,CircleDisplay&,bool,CMk3Audio*);
};
