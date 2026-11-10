#pragma once
#include <stdint.h>
namespace groove { namespace ui {
enum class ScreenType:uint8_t {Instrument,Writing,Sound,Material,Signal,Arrangement,Performance,Selection,Settings,Diagnostics,Transient};
struct ScreenMeaning {const char* name;const char* left;const char* right;};
inline constexpr ScreenMeaning kScreenMeanings[]={
 {"instrument","performance monitor","sound monitor"},{"writing","shared time axis","shared time axis"},
 {"sound","parameter values","parameter shape"},{"material","source waveform","playback shape and values"},
 {"signal","whole signal path","selected path and values"},{"arrangement","ordered elements","selected element"},
 {"performance","transformation values","transformation shape"},{"selection","list and hierarchy","selected item and preview"},
 {"settings","items and values","selected item preview"},{"diagnostics","USB HID audio","MIDI and event log"},
 {"transient","temporary layer","temporary layer"}};
inline const ScreenMeaning& screen_meaning(ScreenType type){return kScreenMeanings[unsigned(type)];}
enum class TopWork:uint8_t {Empty,Selection,Execute,Exit,Confirm,Cancel};
inline const char* top_work_name(TopWork w){const char* names[]={"empty","selection","execute","exit","confirm","cancel"};return names[unsigned(w)];}
}}
