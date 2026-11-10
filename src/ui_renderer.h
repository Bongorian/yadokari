#pragma once
#include "tracker_view.h"
#include "performance.h"
#include "engine_guids.h"
#include "note_transform.h"
#include "instrument_math.h"
#include "ui/widgets.h"
#include "ui/instrument_visualizer.h"
#include "live_effects.h"
#include "daisy_track_drive.h"
#include "ui/page_transition.h"
#include "ui/toast.h"
#include "ui/visual_motion.h"
#include "ui/engine_kind.h"
#include "ui/meter_ballistics.h"
#include "ui/sample_wave_cache.h"
namespace groove {
class UiRenderer:public ui::Widgets {
 using Font=ui::FontRole;
 uint64_t phrase_attributes_=0,last_ms_=0,page_ms_=0;float previous_[8]{},smooth_[8]{};int last_mode_=-1,last_track_=-1;bool initialized_=false;bool repaint_body_=true,repaint_knobs_=true,body_invalidated_=true,prev_dialog_=false,theme_seen_=false;uint32_t last_theme_bg_=0;unsigned last_brightness_=100;
 ui::VisualMotion visual_motion_[2];uint64_t live_signature_=0;
 bool toast_was_=false;uint32_t toast_hash_=0;
 bool help_was_=false;
 uint8_t overlay_mask_was_=0,turning_mask_was_=0;uint32_t overlay_underlay_[2][480*ui::Layout::overlay_h]{};uint32_t body_snapshot_[2][480*ui::Layout::body_h]{};uint32_t body_scratch_[480]{};ui::PageTransition transition_;bool transition_pending_=false;uint64_t last_subpage_sig_=0,last_selection_sig_=0;bool signatures_valid_=false;unsigned last_sub_values_[13]{};uint64_t last_context_=~uint64_t(0);bool context_matches_=false;
 ui::SampleWaveCache wave_cache_;ui::MeterBallistics meters_[16]{};uint32_t meter_generation_=~0u;
 static const char* capture_name(unsigned source){static const char* names[]={"Main","Input","Cue","Fader","Dry","Insert","Returns","Perform","Group"};return names[capture_kind(source)];}
 static const char* audio_input_name(unsigned source,char* out);
 static float meter_amount(float peak){return ui::clamp01((20*log10f(peak+0.00000001f)+60)/60);}
#include "ui/header.h"
#include "ui/tracker.h"
 void pattern(const ProjectState& p,const Performance& perf);
 void kind_chip(unsigned side,int x,int y,int w,ui::EngineKind kind){const auto& chip=ui::engine_kind_chip(kind);fill(side,x,y,w,12,theme.monochrome?theme.surface:0xff101820);centered_label(side,x,y,w,12,chip.name,ui::engine_kind_color(kind,theme),Font::Small);}
 void pads(const ProjectState& p,const Performance& perf);
 void kit_panel(const ProjectState& p,const Performance& perf);
 void note_grid(const ProjectState& p,const Performance& perf,bool chord);
 void step_chord_detail(const ProjectState& p,const Performance& perf);
 void chords(const ProjectState& p,const Performance& perf){note_grid(p,perf,true);}
 void scenes(const ProjectState& p,const Performance& perf);
 void song(const ProjectState& p,const Performance& perf);
 void clips(const ProjectState& p,const Performance& perf);
 void sampler(ConstTrackView t,const Performance& perf,WavView sample,const char* name);
 void sample_wave(unsigned side,int x,int y,int w,int h,const Performance& perf,const SampleEdit& edit,bool point,float position,float first=0,float last=1,bool markers=true);

 void sampler_body(ConstTrackView t,const Performance& perf,WavView sample,const char* name);
 void library_catalog(const ProjectState& p,const Performance& perf);
 void browser(const ProjectState& p,const Performance& perf);

 #include "ui/signal_views.h"

 void diagnostics(const Performance& perf);
 void mixer(const ProjectState& p,const Performance& perf);
 void twin_panel(const Performance& perf);
 void composition_panel(const ProjectState&,const Performance&);
 void modulation(const ProjectState& p,const Performance& perf);
 void audio_lane_page(const ProjectState& p,const Performance& perf);
 void performance(const ProjectState& p,const Performance& perf);
 void performance_fx_page(const Performance& perf);
 void mutation(const ProjectState& p,const Performance& perf);
 void generate(const ProjectState& p,const Performance& perf);
 void module_picture(const ProjectState& p,const Performance& perf);
 void modulation_editor(const ProjectState& p,const Performance& perf);
 void macro_performance(const ProjectState& p);

 void project(const ProjectState& p,const Performance& perf);

 void settings(const Performance& perf);

 void gesture_page(const ProjectState& p,const Performance& perf);
 void source_list(const ProjectState& p,const Performance& perf);
 void source_detail(const ProjectState& p,const Performance& perf);
 void synth_signal(const ProjectState& p,ConstTrackView t,const Performance& perf);
 void layer_picture(ConstTrackView t,const Performance& perf);
 void sample_picture(ConstTrackView t,const Performance& perf);
 void source_visual(ConstTrackView t,const Performance& perf){if(t.instrument==Instrument::Sampler)sample_picture(t,perf);else ui::InstrumentVisualizer::draw(*this,t,perf);}
 void synth(ConstTrackView t,const Performance& perf);
 void draw_visual(unsigned side,const VisualDeclaration& declaration,const VisualData& data,const Performance& perf,uint64_t token,int x=24,int y=82,int w=430,int h=118);
 void toast(const char* text,uint32_t color);
 void hints(const Performance& perf);
 void knob_legend(const ProjectState& p,const Performance& perf);

#include "ui/knob_value.h"
 void knobs(const ProjectState& p,const Performance& perf,uint64_t now);

public:
 static uint64_t fnv(uint64_t h, unsigned v) { return (h ^ v) * 1099511628211ull; }
 uint64_t selection_signature(const Performance& perf, ConstTrackView t, unsigned pattern) const ;
 // Section 2.5 transitions: renderer-side local interpolation over a
 // snapshot of the previous body band. Input is never gated - the
 // Performance state has already moved; the renderer only animates
 // toward it, and a new change re-triggers from the current pixels.
 void start_transition(uint64_t now,int direction);
 void composite_transition(uint64_t now);
 bool knobs_active(const Performance& perf,uint64_t now) const ;
 uint8_t last_pad_output_[16]{};
 public:
 void invalidate_body() { body_invalidated_=true; }
 void invalidate(){initialized_=false;signatures_valid_=false;transition_.cancel();body_invalidated_=true;}
 void draw(const ProjectState& p,const Performance& perf,unsigned peak=0,unsigned load=0,WavView sample={});
};
}
