#include "engine.h"
#include "performance.h"
#include "ui_renderer.h"
#include "ui/framebuffer565.h"
#include "demo_project.h"
#include "project_editor.h"
#include "../platform/desktop/sample_assets.h"
#include <cstdio>
#include <cstring>
#include <memory>
static bool write_screen(const char* name,const groove::UiRenderer& canvas,const groove::Performance& perf){
 char path[128];std::snprintf(path,sizeof path,"build/mk3-ui-%s.ppm",name);FILE* f=std::fopen(path,"wb");if(!f)return false;std::fprintf(f,"P6\n960 272\n255\n");for(unsigned y=0;y<272;++y)for(unsigned x=0;x<960;++x){uint32_t v=canvas.pixels[x/480][y*480+x%480];if(perf.visual_settings().preview565)v=groove::ui::preview565(v,groove::ui::PixelOrder(perf.visual_settings().pixel_order));uint8_t rgb[]={uint8_t(v>>16),uint8_t(v>>8),uint8_t(v)};if(std::fwrite(rgb,1,3,f)!=3){std::fclose(f);return false;}}return std::fclose(f)==0;
}
int main(int argc,char** argv){
 (void)argv;
 auto project=std::make_unique<groove::ProjectState>();groove::make_demo(*project);project->song.sections[0].scene=0;project->song.sections[0].bars=2;project->song.sections[1].scene=1;project->song.sections[1].bars=1;project->song.sections[1].transpose=12;project->song.sections[2].scene=2;project->song.sections[2].bars=4;project->song.sections[2].transpose=-3;project->song.length=3;project->song.enabled=true;
 auto engine=std::make_unique<groove::Engine>();groove::DesktopSampleAssets assets;groove::WavView bindings[groove::kSampleAssets]{};if(!assets.prepare(*project,bindings) || !engine->stage_project(*project,bindings))return 1;
 groove::Performance perf(*engine,*project);auto canvas=std::make_unique<groove::UiRenderer>();
 auto advance=[&](unsigned frames){float block[256];while(frames){unsigned n=frames>128?128:frames;engine->render(block,n);perf.animate(engine->samples_rendered()/48);perf.poll();groove::MidiEvent m;while(engine->pop_midi(m)){}groove::ClockEvent clock;while(engine->pop_clock(clock)){}frames-=n;}};
 auto settled_draw=[&](bool with_sample){groove::WavView v=with_sample?bindings[0]:groove::WavView{};canvas->draw(*project,perf,0,0,v);advance(12000);canvas->draw(*project,perf,0,0,v);}; // shots show the settled page, after the section 2.5 transition lands
 auto audio_pages=[&](){
 // Real shared-core audio-lane pages, using the already prepared demo WAV.
 perf.stop();advance(1);perf.select_track(12);project->assets[0].metadata.rate=bindings[0].rate;project->assets[0].metadata.frames=bindings[0].frames;project->assets[0].metadata.channels=uint8_t(bindings[0].channels);project->assets[0].metadata.bits=bindings[0].bits;groove::AudioLane lane;lane.kind=groove::TrackKind::Looper;lane.source=39;lane.bars=2;lane.feedback=0.65f;lane.clips[0]={0,0,bindings[0].frames,1,0};if(!perf.set_audio_lane(12,lane,true,0,bindings[0],project->assets[0].metadata))return false;
 perf.set_mode(groove::UiMode::Song);perf.set_shift(true);perf.soft_key(5);perf.set_shift(false);perf.play();advance(4800);settled_draw(false);if(!write_screen("LOOPER",*canvas,perf))return false;
 lane.kind=groove::TrackKind::Audio;lane.clips[1]=lane.clips[0];lane.clips[1].tick=7680;lane.clips[2]=lane.clips[0];lane.clips[2].tick=15360;if(!perf.set_audio_lane(12,lane))return false;settled_draw(false);if(!write_screen("AUDIO",*canvas,perf))return false;
 return true;
 };
 if(argc>1 && std::strcmp(argv[1],"--lights-only")==0){
  perf.set_mode(groove::UiMode::Settings);perf.soft_key(2);settled_draw(false);
  if(!write_screen("PAD-LIGHTS",*canvas,perf))return 1;
  perf.set_mode(groove::UiMode::Step);perf.select_track(0);perf.play();advance(2400);settled_draw(false);
  if(!write_screen("PAD-STEPS",*canvas,perf))return 1;
  perf.stop();advance(128);perf.set_mode(groove::UiMode::Pad);settled_draw(false);
  return write_screen("PAD-COLORS",*canvas,perf)?0:1;
 }
 if(argc>1)return audio_pages()?0:1;
 if(!perf.play())return 1;
 advance(30000);
 for(unsigned mode=0;mode<19;++mode){perf.select_track(mode==unsigned(groove::UiMode::Sampler)?5:4);perf.set_mode(groove::UiMode(mode));if(mode==unsigned(groove::UiMode::Clip))perf.launch_clip(4,1);if(mode==unsigned(groove::UiMode::Scene))perf.launch_scene(1);settled_draw(true);if(!write_screen(groove::mode_name(perf.mode()),*canvas,perf))return 1;}
 perf.select_track(4);perf.set_mode(groove::UiMode::Effects);perf.encoder_direction(0x08);perf.encoder_direction(0x08);perf.encoder_direction(0x08);settled_draw(true);if(!write_screen("FX-SLOT",*canvas,perf))return 1;perf.encoder_direction(0x08);settled_draw(true);if(!write_screen("FX-CHAIN",*canvas,perf))return 1;
 perf.select_track(5);perf.set_mode(groove::UiMode::Sampler);perf.set_shift(true);settled_draw(true);if(!write_screen("SAMPLER-FILE-LOOP",*canvas,perf))return 1;perf.set_shift(false);perf.select_track(5);perf.set_mode(groove::UiMode::Sampler);perf.encoder_direction(0x08);perf.encoder_direction(0x08);perf.encoder_direction(0x08);settled_draw(true);if(!write_screen("SAMPLER-WAVE",*canvas,perf))return 1;perf.soft_key(3);settled_draw(true);if(!write_screen("SAMPLER-STRETCH",*canvas,perf))return 1;perf.soft_key(1);perf.encoder_direction(0x08);{auto e=track_at(*project,project->active_pattern,5).sample_edit;e.slice_count=4;for(unsigned i=0;i<=4;++i)e.markers[i]=float(i)/4;e.zone_count=4;e.slice_warp[1]=1;for(unsigned i=0;i<4;++i){e.zones[i].key_low=uint8_t(i*32);e.zones[i].key_high=uint8_t(i*32+31);e.zones[i].velocity_low=1;e.zones[i].velocity_high=127;e.zones[i].root=uint8_t(60+i);}perf.sample_edit(e);}settled_draw(true);if(!write_screen("SAMPLER-ZONES",*canvas,perf))return 1;
 perf.select_track(4);perf.set_mode(groove::UiMode::Chord);settled_draw(true);if(!write_screen("CHORDS",*canvas,perf))return 1;
 perf.set_shift(true);if(!perf.soft_key(1))return 1;settled_draw(true);if(!write_screen("CHORD-INVERSION",*canvas,perf))return 1;perf.set_shift(false);
 perf.select_track(4);perf.set_mode(groove::UiMode::Synth);perf.control(7,0x20);perf.control(7,0x00);if(!perf.knob(1,6000))return 1;settled_draw(false);if(!write_screen("SOURCE-SELECT",*canvas,perf))return 1;perf.control(7,0x20);perf.control(7,0x00);
 if(!perf.stop())return 1;
 advance(128);for(auto& clip:project->clips)clip.pattern=0;project->active_pattern=0;if(!engine->stage_project(*project,bindings))return 1;perf.project_loaded();advance(128);
 perf.select_track(4);perf.set_mode(groove::UiMode::Step);groove::ProjectEditor editor(*project,*engine);if(!editor.set_length(4,256)||!editor.set_step(4,255,true))return 1;perf.step_page(15);perf.set_shift(true);if(!perf.pad(3,0x10,2000)||!perf.pad(3,0x20,0))return 1;perf.set_shift(false);advance(128);settled_draw(false);if(!write_screen("STEP-256",*canvas,perf))return 1;perf.set_shift(true);if(!perf.soft_key(3))return 1;perf.set_shift(false);settled_draw(false);if(!write_screen("STEP-256-OVERVIEW",*canvas,perf))return 1;perf.set_shift(true);if(!perf.soft_key(3))return 1;perf.set_shift(false);
 perf.set_mode(groove::UiMode::Synth);perf.soft_key(0);
 perf.select_track(4);{auto lt=track_at(*project,project->active_pattern,4);lt.instrument=groove::Instrument::WaveSequence;lt.creative.value[0][0]=96;lt.creative.value[0][2]=64;lt.creative.value[1][1]=80;lt.creative.value[2][4]=112;lt.creative.length[0]=6;lt.creative.length[1]=4;lt.creative.mutation=35;lt.creative.follower_source=5;lt.creative.follower_amount=40;}if(!perf.soft_key(3))return 1;settled_draw(false);if(!write_screen("ENGINE-LAB",*canvas,perf))return 1;perf.soft_key(0);
 struct Example{groove::Instrument instrument;const char* name;unsigned model;};
 const Example examples[]={{groove::Instrument::PolySaw,"VA",0},{groove::Instrument::DaisySynth,"DAISY",0},{groove::Instrument::Acid,"ACID",0},{groove::Instrument::DaisyAnalogDrums,"DAISY ANALOG DRUMS",0},{groove::Instrument::DaisySyntheticDrums,"DAISY SYNTH DRUMS",0},{groove::Instrument::PlaitsMacro,"WAVETABLE",13},{groove::Instrument::PlaitsMacro,"CHIPTUNE",7},{groove::Instrument::PlaitsMacro,"MODAL",20},{groove::Instrument::PlaitsMacro,"GRANULAR-FORMANT",11},{groove::Instrument::FM,"FM",0}};
 for(const auto& example:examples){if(!perf.set_instrument(example.instrument))return 1;if(example.instrument==groove::Instrument::PlaitsMacro){auto track=track_at(*project,project->active_pattern,4);int delta=int(example.model)-int(track.plaits_model);if(delta && !perf.knob(1,int16_t(delta*1000)))return 1;}if(!engine->submit({groove::CommandType::Trigger,4,0,115}))return 1;
 advance(3000);settled_draw(true);if(!write_screen(example.name,*canvas,perf))return 1;}
 if(!perf.soft_key(1) || !perf.knob(0,-22000))return 1;
 advance(256);settled_draw(false);if(!write_screen("TONE",*canvas,perf))return 1;
 if(!perf.soft_key(2))return 1;
 settled_draw(false);if(!write_screen("ENVELOPE",*canvas,perf))return 1;
 perf.soft_key(0);if(!perf.set_instrument(groove::Instrument::ExternalMidi) || !perf.set_wave_profile(true) || !perf.soft_key(1))return 1;
 settled_draw(false);if(!write_screen("WAVE-CLOCK",*canvas,perf))return 1;
 groove::make_generative_demo(*project);if(!engine->stage_project(*project,bindings))return 1;perf.project_loaded();advance(5000);perf.select_track(4);perf.set_mode(groove::UiMode::Generate);
 for(unsigned page=1;page<4;++page){if(!perf.soft_key(page))return 1;
 settled_draw(false);const char* names[]={"","ACCUMULATOR","GROOVE","LANES"};if(!write_screen(names[page],*canvas,perf))return 1;}
 perf.set_mode(groove::UiMode::Perform);perf.set_shift(true);if(!perf.soft_key(2))return 1;perf.set_shift(false);if(!perf.morph(0.65f))return 1;perf.set_shift(true);if(!perf.soft_key(0))return 1;perf.set_shift(false);advance(128);settled_draw(false);if(!write_screen("MUTATION",*canvas,perf))return 1;
 perf.set_mode(groove::UiMode::Keyboard);perf.set_shift(true);if(!perf.control(3,0x10))return 1;perf.set_shift(false);if(!perf.knob(0,1000)||!perf.knob(1,1000)||!perf.knob(2,-10000)||!perf.knob(3,1000))return 1;settled_draw(false);if(!write_screen("NOTE-REPEAT",*canvas,perf))return 1;if(!perf.control(3,0)||!perf.set_repeat_latch(false))return 1;advance(128);
 FILE* motion=std::fopen("build/mk3-led-motion.csv","w");if(!motion)return 1;
 if(!perf.play())return 1;
 perf.select_track(4);perf.set_mode(groove::UiMode::Modulation);if(!perf.soft_key(1))return 1;groove::AutomationLane longer;longer.enabled=true;longer.length=64;for(unsigned i=0;i<64;++i)longer.values[i]=float(i%16)/15;if(!perf.automation(0,longer))return 1;advance(128);if(!perf.soft_key(5)||!perf.soft_key(5))return 1;settled_draw(false);if(!write_screen("AUTO64",*canvas,perf))return 1;groove::AutomationLane expression;expression.enabled=true;expression.parameter=260;for(unsigned i=0;i<64;++i)expression.values[i]=(i%16<8)?8192:12288;if(!perf.automation(1,expression)||!perf.knob(0,1000)||!perf.soft_key(4)||!perf.soft_key(4))return 1;advance(128);settled_draw(false);if(!write_screen("EXPRESSION",*canvas,perf))return 1;if(!perf.knob(0,-1000))return 1;
 auto previous_instrument=track_at(*project,project->active_pattern,4).instrument;if(!perf.set_instrument(groove::Instrument::ExternalMidi))return 1;perf.set_shift(true);if(!perf.soft_key(3))return 1;perf.set_shift(false);if(!perf.knob(2,1000)||!perf.knob(1,1000)||!perf.set_parameter(256,8193))return 1;advance(128);settled_draw(false);if(!write_screen("NRPN",*canvas,perf))return 1;if(!perf.knob(5,1000))return 1;advance(128);settled_draw(false);if(!write_screen("RPN",*canvas,perf))return 1;
 auto& midi_browser=perf.browser();if(!groove::copy_text(midi_browser.directory,sizeof midi_browser.directory,"RECORDINGS")||!groove::copy_text(midi_browser.entries[0].name,sizeof midi_browser.entries[0].name,"MIDI00.MID")||!groove::copy_text(midi_browser.status,sizeof midi_browser.status,"IMPORT again: steps / RPN or NRPN may send"))return 1;midi_browser.count=1;midi_browser.cursor=0;perf.set_mode(groove::UiMode::Browser);perf.set_shift(true);settled_draw(false);if(!write_screen("MIDI-IMPORT",*canvas,perf))return 1;perf.set_shift(false);if(!perf.set_instrument(previous_instrument))return 1;advance(128);
 perf.set_mode(groove::UiMode::Modulation);if(!perf.soft_key(3))return 1;perf.set_shift(true);settled_draw(false);if(!write_screen("SCALE-EDITOR",*canvas,perf))return 1;perf.set_shift(false);
 perf.set_mode(groove::UiMode::Project);perf.set_shift(true);settled_draw(false);if(!write_screen("MIDI-CAPTURE",*canvas,perf))return 1;perf.set_shift(false);
 perf.select_track(4);perf.set_mode(groove::UiMode::Synth);
 for(auto instrument:{groove::Instrument::Resonator,groove::Instrument::Texture}){
  if(!perf.set_instrument(instrument))return 1;
  advance(128);
  for(unsigned page:{0u,2u,3u}){if(instrument==groove::Instrument::Resonator && page==3)continue;if(!perf.soft_key(page))return 1;settled_draw(false);char name[40];std::snprintf(name,sizeof name,"%s-%u",instrument==groove::Instrument::Resonator?"RESONATOR":"TEXTURE",page);if(!write_screen(name,*canvas,perf))return 1;}
 }
 perf.select_track(2);perf.set_mode(groove::UiMode::Step);
 for(unsigned frame=0;frame<42;++frame){
  if(frame==6){perf.set_mode(groove::UiMode::Clip);perf.launch_clip(2,1);}
  if(frame==12)perf.mute_or_solo(0,false);
  if(frame==16)perf.undo();
  if(frame==18){perf.set_mode(groove::UiMode::Perform);perf.stutter(true);}
  if(frame==24){perf.stutter(false);perf.set_mode(groove::UiMode::Synth);perf.select_track(4);perf.soft_key(0);}
  if(frame==27)perf.knob(3,4000);
  if(frame==32){perf.store_morph(0);perf.set_mode(groove::UiMode::Mixer);perf.knob(2,-8000);perf.store_morph(1);}
  if(frame>=33 && frame<=40)perf.morph(float(frame-33)/7);
  advance(2400);settled_draw(true);char name[24];std::snprintf(name,sizeof name,"MOTION-%02u",frame);if(!write_screen(name,*canvas,perf))return 1;
  groove::ui::LedFrame leds;perf.led_frame(leds);std::fprintf(motion,"%u",frame*50);for(auto v:leds.pads)std::fprintf(motion,",%u",v);for(auto v:leds.buttons)std::fprintf(motion,",%u",v);for(auto v:leds.strip_level)std::fprintf(motion,",%u",v);for(auto v:leds.pads)std::fprintf(motion,",%u",groove::ui::LedStateEngine::preview(v,canvas->theme));std::fprintf(motion,"\n");
 }
 if(!audio_pages())return 1;
 if(std::fclose(motion)!=0)return 1;
 FILE* f=std::fopen("build/mk3-surface.tsv","w");if(!f)return 1;std::fprintf(f,"control\treport_byte\tmask\tled_payload_offset\tled_kind\n");for(const auto& b:groove::mk3::buttons)std::fprintf(f,"%s\t%u\t0x%02x\t%d\t%u\n",b.name,b.report_byte,b.mask,b.led_offset,unsigned(b.led));return std::fclose(f)?1:0;
}
