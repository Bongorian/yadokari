#pragma once
#include "performance.h"
#include "ui_axes.h"
#include "ui/screen_grammar.h"
namespace groove {
// Page descriptors (SPEC sections 1.2 and 2.1): every EditPage declares
// its identity, the pad grammar it implies, and its geometry in one
// place. Geometry is the shared token frame of section 1.2; the
// consistency test (tests/page_descriptor_test.cpp) pins every page to
// it so a page cannot drift its header/body/knob boundaries.
// Body geometry is fixed; knob_y/h describes its floating overlay.
struct PageGeometry {
 uint16_t header_y, body_y, body_h, knob_y, knob_h;
 uint16_t title_x0, title_x1;   // title zone; longer titles ellipsize
 uint16_t dots_x0, dots_x1;     // fixed page-dot zone (4 dots, r=3, pitch 11)
 uint16_t status_x0, status_x1; // right-aligned status zone
};
inline constexpr PageGeometry kPageGeometry{24, 64, ui::Layout::body_h, 186, 52, 10, 190, 198, 237, 245, 470};

enum class PageFamily:uint8_t {Play,Write,Arrange,Sound,Manage};
struct PageDescriptor {
 UiMode id;
 PageFamily family;
 const char* name;       // matches mode_name(id)
 PadMode pad_mode;       // grammar this page selects (grammar pages only)
 bool pad_grammar_page;  // true when entering the page sets the PadMode axis
 PageGeometry geometry;
};
inline constexpr PageDescriptor kPageDescriptors[] = {
 {UiMode::Pad, PageFamily::Play, "PAD", PadMode::Play, true, kPageGeometry},
 {UiMode::Step, PageFamily::Write, "STEP", PadMode::Step, true, kPageGeometry},
 {UiMode::Clip, PageFamily::Arrange, "CLIP", PadMode::Clip, true, kPageGeometry},
 {UiMode::Scene, PageFamily::Arrange, "SCENE", PadMode::Scene, true, kPageGeometry},
 {UiMode::Mixer, PageFamily::Sound, "MIXER", PadMode::Play, false, kPageGeometry},
 {UiMode::Synth, PageFamily::Sound, "SYNTH", PadMode::Play, false, kPageGeometry},
 {UiMode::Effects, PageFamily::Sound, "FX", PadMode::Play, false, kPageGeometry},
 {UiMode::Perform, PageFamily::Play, "PERFORM", PadMode::Play, false, kPageGeometry},
 {UiMode::Generate, PageFamily::Write, "GENERATE", PadMode::Play, false, kPageGeometry},
 {UiMode::Keyboard, PageFamily::Play, "KEYBOARD", PadMode::Keyboard, true, kPageGeometry},
 {UiMode::Sampler, PageFamily::Sound, "SAMPLER", PadMode::Play, false, kPageGeometry},
 {UiMode::Project, PageFamily::Manage, "PROJECT", PadMode::Play, false, kPageGeometry},
 {UiMode::Settings, PageFamily::Manage, "SETTINGS", PadMode::Play, false, kPageGeometry},
 {UiMode::Modulation, PageFamily::Sound, "MODULATION", PadMode::Play, false, kPageGeometry},
 {UiMode::Browser, PageFamily::Manage, "BROWSER", PadMode::Play, false, kPageGeometry},
 {UiMode::Library, PageFamily::Manage, "PRESETS", PadMode::Play, false, kPageGeometry},
 {UiMode::Routing, PageFamily::Sound, "ROUTING", PadMode::Play, false, kPageGeometry},
 {UiMode::Diagnostics, PageFamily::Manage, "DIAGNOSTICS", PadMode::Play, false, kPageGeometry},
 {UiMode::Chord, PageFamily::Play, "CHORDS", PadMode::Chords, true, kPageGeometry},
  {UiMode::Song, PageFamily::Arrange, "SONG", PadMode::Step, false, kPageGeometry},
};
inline constexpr unsigned kPageDescriptorCount = sizeof(kPageDescriptors) / sizeof(kPageDescriptors[0]);
inline const PageDescriptor* page_descriptor(UiMode mode) {
 for (const auto& d : kPageDescriptors) if (d.id == mode) return &d;
 return nullptr;
}
// The pad grammar a page implies, or nullptr for non-grammar pages
// (entering them leaves the PadMode axis untouched).
inline const PadMode* page_pad_mode(UiMode mode) {
 const PageDescriptor* d = page_descriptor(mode);
 return d && d->pad_grammar_page ? &d->pad_mode : nullptr;
}
// Approved Layer-list exception: left Top3/4 execute child edits.
inline bool top_placement_exception(const Performance& p,unsigned i){return p.layer_page() && (i==2 || i==3);}
inline ui::ScreenType screen_type(const Performance& p){
 using T=ui::ScreenType;
 if(p.modal_active() || p.browser_editing() || p.note_repeat_mode())return T::Transient;
 if(p.source_select() || p.kit_edit_page())return T::Selection;
 switch(p.mode()){
 case UiMode::Pad:case UiMode::Keyboard:case UiMode::Chord:return T::Instrument;
 case UiMode::Step:return T::Writing;
 case UiMode::Synth:case UiMode::Modulation:return T::Sound;
 case UiMode::Sampler:return T::Material;
 case UiMode::Mixer:case UiMode::Routing:return T::Signal;
 case UiMode::Effects:return p.fx_page()==3 || p.fx_page()==4 || p.fx_page()==5?T::Sound:T::Signal;
 case UiMode::Clip:case UiMode::Scene:case UiMode::Song:return T::Arrangement;
 case UiMode::Perform:return p.audio_page() && p.audio_lane().kind==TrackKind::Audio?T::Arrangement:T::Performance;
 case UiMode::Project:case UiMode::Browser:case UiMode::Library:return T::Selection;
 case UiMode::Settings:return T::Settings;
 case UiMode::Diagnostics:return T::Diagnostics;
 case UiMode::Generate:return T::Sound;
 }
 return T::Transient;
}

}
