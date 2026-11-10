#pragma once
#include "alloc_policy.h"
#include <stdint.h>
namespace groove {
// Task-owned bounded diagnostic ledger, independent of pixels and ownership gates.
class DisplayAttribution {
public:
 struct Frame {unsigned serial,stage,steady,due,gate,begin,drawn,origin[2],completed[2],window[2];};
 struct Task {unsigned kind,begin,end;};
 struct Job {unsigned serial,side,copy_begin,copy_end,submitted,began,ended,collected,scan,encode,coalesce,bytes,dirty,attempted;};
 ~DisplayAttribution(){delete[] frames_;delete[] jobs_;delete[] tasks_;}
 bool begin(unsigned serial){if(!frames_ && allocation_admitted(sizeof(Frame)*4096))frames_=alloc_array<Frame>(4096);if(!jobs_ && allocation_admitted(sizeof(Job)*8192))jobs_=alloc_array<Job>(8192);if(!tasks_ && allocation_admitted(sizeof(Task)*4096))tasks_=alloc_array<Task>(4096);task_count_=task_read_=lost_tasks_=0;base_=serial;frame_count_=job_count_=frame_read_=job_read_=lost_frames_=lost_jobs_=0;return frames_ && jobs_ && tasks_;}
 void frame(const Frame& f){if(!frames_)return;if(frame_count_>=4096){++lost_frames_;return;}frames_[frame_count_++]=f;}
 void complete(unsigned serial,unsigned side,unsigned origin,unsigned completed,unsigned window){if(!frames_ || side>1 || serial<=base_)return;unsigned at=serial-base_-1;if(at<frame_count_ && frames_[at].serial==serial){auto& f=frames_[at];f.origin[side]=origin;f.completed[side]=completed;f.window[side]=window;}}
 unsigned job(const Job& j){if(!jobs_)return 0;if(job_count_>=8192){++lost_jobs_;return 0;}jobs_[job_count_++]=j;return job_count_;}
 void task(unsigned kind,unsigned begin,unsigned end){if(end-begin<100 || !tasks_)return;if(task_count_<4096)tasks_[task_count_++]={kind,begin,end};else ++lost_tasks_;}
 const Task* next_task(){return task_read_<task_count_?&tasks_[task_read_++]:nullptr;}
 unsigned tasks()const{return task_count_;}unsigned lost_tasks()const{return lost_tasks_;}
 const Job* next_job(){return job_read_<job_count_?&jobs_[job_read_++]:nullptr;}
 const Frame* next_frame(){return frame_read_<frame_count_?&frames_[frame_read_++]:nullptr;}
 unsigned jobs()const{return job_count_;}unsigned frames()const{return frame_count_;}unsigned lost_jobs()const{return lost_jobs_;}unsigned lost_frames()const{return lost_frames_;}
private:Task* tasks_=nullptr;unsigned task_count_=0,task_read_=0,lost_tasks_=0;Frame* frames_=nullptr;Job* jobs_=nullptr;unsigned base_=0,frame_count_=0,job_count_=0,frame_read_=0,job_read_=0,lost_frames_=0,lost_jobs_=0;
};
}
