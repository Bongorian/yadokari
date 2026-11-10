#pragma once
#include <stdint.h>
#include "dsp_assist_counts.h"
#include "render_audit.h"
namespace groove {
// Diagnostic work-duty counters, not PMU utilization. A row has one owning
// core; no resetting a live producer and no shared counter cache line.
class CoreWorkMeter {
public:
 using Clock=uint32_t(*)();
 struct Snapshot {uint64_t busy=0;unsigned calls=0,max_us=0;};
 struct Token {uint32_t start=0;bool enabled=false;};
 void clock(Clock fn){clock_=fn;} // before publishing to worker cores
 void enable(bool on){__atomic_store_n(&enabled_,unsigned(on && clock_),__ATOMIC_RELEASE);}
 Token begin()const{bool on=__atomic_load_n(&enabled_,__ATOMIC_ACQUIRE);return {on?clock_():0,on};}
 void end(unsigned core,Token t,bool productive=true){if(!t.enabled || !productive || core>=4)return;unsigned elapsed=clock_()-t.start;auto& row=rows_[core];__atomic_store_n(&row.busy,__atomic_load_n(&row.busy,__ATOMIC_RELAXED)+elapsed,__ATOMIC_RELAXED);__atomic_add_fetch(&row.calls,1u,__ATOMIC_RELAXED);if(elapsed>__atomic_load_n(&row.max_us,__ATOMIC_RELAXED))__atomic_store_n(&row.max_us,elapsed,__ATOMIC_RELAXED);}
 Snapshot snapshot(unsigned core)const{if(core>=4)return {};const auto& row=rows_[core];return {__atomic_load_n(&row.busy,__ATOMIC_RELAXED),__atomic_load_n(&row.calls,__ATOMIC_RELAXED),__atomic_load_n(&row.max_us,__ATOMIC_RELAXED)};}
private:
 struct alignas(64) Row{uint64_t busy=0;unsigned calls=0,max_us=0;};Row rows_[4]{};Clock clock_=nullptr;unsigned enabled_=0;
};
class CoreWorkScope {
 CoreWorkMeter& meter_;CoreWorkMeter::Token token_;unsigned core_;bool finished_=false;
public:CoreWorkScope(CoreWorkMeter& meter,unsigned core):meter_(meter),token_(meter.begin()),core_(core){}
 void finish(){if(!finished_){meter_.end(core_,token_);finished_=true;}}
 ~CoreWorkScope(){finish();}
};
// Task publishes a phase; the DSP owner acknowledges after completing a whole
// block. The previous phase row is immutable after that acknowledgement.
struct PolyStealCounts {
 uint64_t triggered=0,released=0,held=0,refused=0;
 PolyStealCounts operator-(const PolyStealCounts& b)const{return {triggered-b.triggered,released-b.released,held-b.held,refused-b.refused};}
 void add(const PolyStealCounts& b){triggered+=b.triggered;released+=b.released;held+=b.held;refused+=b.refused;}
};
class EfficiencyAudioMeter {
public:
 static constexpr unsigned rows=64,off=~0u;
 struct Population{unsigned main=0,poly=0,va=0,sampler=0;DspAssistCounts assist{};unsigned owned=0,va_main=0,va_poly=0,released=0;PolyStealCounts steals{};RenderAudit::EventCounts event_work{};};
 struct Stats {uint64_t us=0;unsigned blocks=0,max_us=0,over500=0,first_missing=0,last_missing=0,pool32_blocks=0,pool32_missing=0,main_peak=0,poly_peak=0,va_peak=0,sampler_peak=0,native_seen=0,refused_seen=0;DspAssistCounts assist{};unsigned owned_peak=0,va_main_peak=0,va_poly_peak=0,released_peak=0,peak_block=0;PolyStealCounts steals{};RenderAudit::Sample peak{};RenderAudit::EventCounts event_work{};};
 void request(unsigned row){__atomic_store_n(&requested_,row<rows?row:off,__ATOMIC_RELEASE);}
 unsigned requested()const{return __atomic_load_n(&requested_,__ATOMIC_ACQUIRE);}
 unsigned acknowledged()const{return __atomic_load_n(&acknowledged_,__ATOMIC_ACQUIRE);}
 void cap(unsigned row,unsigned blocks){if(row<rows)limits_[row]=blocks;}
 unsigned blocks(unsigned row)const{return row<rows?__atomic_load_n(&published_[row],__ATOMIC_ACQUIRE):0;}
 void record(unsigned row,unsigned us,Population pop,unsigned native,unsigned refused,unsigned missing=0,bool advance=true,const RenderAudit::Sample* audit=nullptr){if(advance && row<rows && (!limits_[row] || stats_[row].blocks<limits_[row])){auto& s=stats_[row];s.assist.add(pop.assist);s.steals.add(pop.steals);if(pop.owned>s.owned_peak)s.owned_peak=pop.owned;if(pop.va_main>s.va_main_peak)s.va_main_peak=pop.va_main;if(pop.va_poly>s.va_poly_peak)s.va_poly_peak=pop.va_poly;if(pop.released>s.released_peak)s.released_peak=pop.released;unsigned delta=s.blocks?missing-s.last_missing:0;if(!s.blocks)s.first_missing=missing;s.last_missing=missing;if(pop.owned==32){++s.pool32_blocks;s.pool32_missing+=delta;}s.us+=us;++s.blocks;if(us>s.max_us){s.max_us=us;s.peak_block=s.blocks;if(audit)s.peak=*audit;s.event_work=pop.event_work;}s.over500+=us>500;if(pop.main>s.main_peak)s.main_peak=pop.main;if(pop.poly>s.poly_peak)s.poly_peak=pop.poly;if(pop.va>s.va_peak)s.va_peak=pop.va;if(pop.sampler>s.sampler_peak)s.sampler_peak=pop.sampler;s.native_seen|=native;s.refused_seen|=refused;__atomic_store_n(&published_[row],s.blocks,__ATOMIC_RELEASE);}__atomic_store_n(&acknowledged_,row,__ATOMIC_RELEASE);}
 const Stats& sealed(unsigned row)const{return stats_[row];} // only after phase acknowledgement
private:Stats stats_[rows]{};unsigned published_[rows]{},limits_[rows]{};unsigned requested_=off,acknowledged_=off;
};
}
