#include "midi_import_job.h"
#include "performance.h"
#include "../platform/desktop/file_access.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <vector>
#include <unistd.h>
using namespace groove;
int main(){
 char directory[]="/tmp/mk3-import-job-XXXXXX";assert(mkdtemp(directory));
 {
 DesktopFiles fs;char path[160];assert(join_path(path,sizeof path,directory,"notes.mid"));
 const uint8_t midi[]={'M','T','h','d',0,0,0,6,0,0,0,1,0,96,'M','T','r','k',0,0,0,12,0,0x90,60,100,24,0x80,60,0,0,0xff,0x2f,0};
 assert(fs.write(path,0,midi,sizeof midi,true));
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tracks[0].instrument=Instrument::ExternalMidi;
 track_at(*p,0,0).steps[10].enabled=true;
 auto e=std::make_unique<Engine>();assert(e->stage_project(*p));float frame[2]{};e->render(frame,0);
 Performance ui(*e,*p);MidiImportJob job(fs,*e,*p);
 auto request=[&]{MidiImportRequest r;assert(copy_text(r.path,sizeof r.path,path));r.generation=e->staged_generation();r.pattern=0;r.track=0;r.channel=0;return r;};
 auto publish=[&](unsigned pattern,unsigned track,ConstTrackView value,unsigned lanes,unsigned nrpn){return ui.publish_track_sequence(pattern,track,value,lanes,nrpn);};
 auto finish=[&](auto callback){for(unsigned n=0;n<200 && job.busy();++n)job.service(callback);assert(!job.busy());MidiImportOutcome result;assert(job.take_result(result));return result;};
 // A confirmed destination is independent of editor selection and Pattern focus.
 uint32_t first=job.accept(request());assert(first);assert(ui.select_track(1) && ui.select_pattern(1));
 auto applied=finish(publish);assert(applied.ticket==first && applied.stage==MidiImportStage::Applied);
 assert(track_at(*p,0,0).steps[0].enabled && !track_at(*p,0,0).steps[10].enabled);
 assert(ui.selected_track()==1 && p->active_pattern==1 && !track_at(*p,1,1).steps[0].enabled);
 e->render(frame,0);ui.poll();assert(ui.undo());e->render(frame,0);ui.poll();
 assert(track_at(*p,0,0).steps[10].enabled && !track_at(*p,0,0).steps[0].enabled);
 assert(ui.selected_track()==1 && p->active_pattern==1);
 // Data edits invalidate the snapshot; merely changing the screen does not.
 assert(job.accept(request()));track_at(*p,0,0).steps[10].velocity=77;
 auto changed=finish(publish);assert(changed.stage==MidiImportStage::Cancelled);
 assert(track_at(*p,0,0).steps[10].velocity==77 && !track_at(*p,0,0).steps[0].enabled);
 // Queue backpressure keeps the original intact and remains cancellable.
 auto waiting=job.accept(request());assert(waiting);
 while(e->command_space())assert(e->submit({CommandType::SetTempo,0,0,120}));
 for(unsigned n=0;n<20;++n)job.service(publish);
 assert(job.busy() && job.stage()==MidiImportStage::WaitingCommit && !track_at(*p,0,0).steps[0].enabled);
 assert(job.cancel(waiting));auto cancelled=finish(publish);assert(cancelled.stage==MidiImportStage::Cancelled);
 e->render(frame,0);ui.poll();
 // A stale cancellation cannot affect a new ticket.
 auto fresh=job.accept(request());assert(fresh && fresh!=waiting);assert(!job.cancel(waiting));
 auto fresh_result=finish(publish);assert(fresh_result.stage==MidiImportStage::Applied);
 e->render(frame,0);ui.poll();assert(ui.undo());e->render(frame,0);ui.poll();
 // Permanent admission failures are terminal, never an endless Busy retry.
 assert(job.accept(request()));unsigned attempts=0;
 auto denied=finish([&](unsigned,unsigned,ConstTrackView,unsigned,unsigned){++attempts;return SequenceCommitResult::NoMemory;});
 assert(attempts==1 && denied.stage==MidiImportStage::Failed && !track_at(*p,0,0).steps[0].enabled);
 // Changing a source after a read starts cannot produce a mixed import.
 std::vector<uint8_t> big(midi,midi+14);unsigned track_bytes=4+5+8192+4+4;
 const uint8_t chunk[]={'M','T','r','k',uint8_t(track_bytes>>24),uint8_t(track_bytes>>16),uint8_t(track_bytes>>8),uint8_t(track_bytes)};
 big.insert(big.end(),chunk,chunk+8);const uint8_t on[]={0,0x90,60,100,0,0xff,1,0xc0,0};big.insert(big.end(),on,on+9);
 big.insert(big.end(),8192,'A');const uint8_t tail[]={24,0x80,60,0,0,0xff,0x2f,0};big.insert(big.end(),tail,tail+8);
 assert(fs.write(path,0,big.data(),unsigned(big.size()),true));assert(job.accept(request()));
 job.service(publish);job.service(publish);
 // This assertion exercises the FileRevision path. A same-size write can
 // share a filesystem clock tick, so change the generation explicitly.
 FileRevision before_source{},after_source{};assert(fs.revision(path,before_source));
 const auto source_time=std::filesystem::last_write_time(path);
 uint8_t replacement='B';assert(fs.write(path,100,&replacement,1,false));
 std::filesystem::last_write_time(path,source_time+std::chrono::seconds(2));
 assert(fs.revision(path,after_source) && !same_file_revision(before_source,after_source));
 auto source=finish(publish);assert(source.stage==MidiImportStage::Failed && std::strstr(source.message,"source changed"));
 // A project generation change cancels before touching the replacement score.
 assert(fs.write(path,0,midi,sizeof midi,true));assert(job.accept(request()));assert(e->stage_project(*p));
 auto generation=finish(publish);assert(generation.stage==MidiImportStage::Cancelled && !track_at(*p,0,0).steps[0].enabled);
 e->render(frame,0);
 std::puts("MIDI backend: fixed destination, atomic Undo, target/source/generation changes, Busy, cancellation and permanent failures passed");
 }
 std::filesystem::remove_all(directory);
}
