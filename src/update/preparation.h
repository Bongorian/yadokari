#pragma once
namespace groove { namespace update {
// Called only from task/UI context. Implementations finish asynchronous audio
// cancellation before saving or arming a receiver. A failed save never arms it.
class Preparation {
public:
 virtual ~Preparation(){}
 virtual bool stop_and_quiesce()=0;
 virtual bool save_project()=0;
 virtual void enter_update_mode()=0;
 bool run(){if(!stop_and_quiesce())return false;if(!save_project())return false;enter_update_mode();return true;}
};
}}
