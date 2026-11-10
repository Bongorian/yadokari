#pragma once
// Canonical current-format fixtures, frozen against v557/1ee9e4a3's writer.
inline void review_writer_fixture(groove::ProjectState& p,unsigned kind){
 using namespace groove;make_empty_project(p);
 if(!kind){p.tracks[0].instrument=Instrument::VirtualAnalog;p.sounds[0].instrument=Instrument::VirtualAnalog;p.patterns[0].mute_mask=1;p.patterns[0].tracks[0].clip_follow.action=ClipFollowAction::Next;p.patterns[0].tracks[0].kit.steps[1][2].enabled=1;p.patterns[1]=p.patterns[0];}
 else {p.content_kind=kind==1?ContentKind::Preset:ContentKind::Kit;p.tracks[0].instrument=kind==1?Instrument::VirtualAnalog:Instrument::DrumKit;auto& id=p.tracks[0].preset_identity;parse_preset_guid("11111111111111111111111111111111",id.guid);id.revision=1;}
}
