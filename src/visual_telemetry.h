#pragma once
#include <stdint.h>
#include "live_visual_analysis.h"
namespace groove {
// Bounded audio-to-UI snapshots. Visual consumers may miss frames; audio never waits.
struct AudioVisualFrame {
 uint32_t controls_target[8]{};float controls_value[8]{};
 float engine_envelope[16][4]{},wavetable_controls[16][3]{};uint32_t preview_ticket=0;
 float delay_input=0,performance_fx_phase=0,compressor_input=0,track_compressor_input[16]{},node_activity[16][6]{};uint8_t performance_fx_actual_mode=0;
 float preview_position=0;bool preview_active=false;VisualPcmFrame taps{};float voice_level[16]{},wavetable_position[16]{},wavetable_phase[16]{},harmonics[16][16]{},vocoder_bands[16][2][16]{},group_vocoder_bands[8][2][16]{},insert_activity[16][2]{},group_insert_activity[8][2]{};bool voice_level_valid[16]{};
 // Installed engine differs from the last sounding voice after a silent mount.
 uint8_t attached_instrument[16]{},layer_failure[16]{};
 bool multitimbral=false;uint8_t module_reason[16]{};uint16_t module_slot[16]{};
 float input_lr[2]{},tuner_samples[1024]{};
 bool texture_frozen[16]{};
 float limiter_main=0,limiter_cue=0;bool invalid_audio=false;float input_peak=0,cue_peak=0,sample_values[16][7]{};
 uint64_t sample=0;uint32_t generation=0,rate=48000;bool playing=false,performance_fx_pending=false;uint8_t performance_fx_mode=0;float performance_fx_amount=0;uint16_t song_position=0;float beat=0,bar_phase=0,master_peak=0,master_wave[64]{};
 float creative_second[16]{},gene_size[16]{},creative_position[16]{},chip_clock[16]{},chip_divider[16]{},spectrum[16][16]{};uint8_t lanes[16][6]{};
 float gesture_phase=0,morph_position=0,macro_value[8]{};bool gesture_morph=false,macro_active[8]{};
 float effects[13]{},compressor_reduction=0,reverb_send[16]{},track_compressor[16][7]{},track_insert[16][8]{},track_compressor_reduction[16]{};uint8_t sources[16][8]{};
 uint16_t pitch_bend[16]{};uint8_t expression_pressure[16]{},expression_pattern[16]{};uint8_t lfo_synced[16]{},lfo_division[16]{};float pressure_depth[16]{};float peak[16]{},envelope[16]{},lfo_phase[16]{},sample_position[16]{};
 float record_wave[512]{};uint32_t record_write=0;uint64_t record_columns=0;bool record_active=false;uint8_t song_section=0;uint16_t song_bar=0,song_bars=0;bool song_active=false,song_snapshot=false;uint32_t song_tick=0,song_ticks=0;uint16_t song_muted=0,song_solo=0;
 float parameters[16][5]{},track_parameters[16][8]{},waveform[16][64]{};
 bool sample_active[16]{};uint8_t sample_slot[16]{};uint8_t note[16]{},instrument[16]{},pattern[16]{},model[16]{};
};
}
