#pragma once
#include <stdint.h>
namespace groove { namespace ui {
enum class TextAlign:uint8_t {Left,Center,Right};
enum class TextKind:uint8_t {Plain,Cell,ValueLabel,ValueNumber,Section,Hero};
enum class ValueShape:uint8_t {Bar,Bipolar,Segments,Chip};
struct ContentValue {const char* name="";const char* text="";float normalized=0;ValueShape shape=ValueShape::Bar;unsigned steps=0;bool enabled=true,selected=false;};
struct ContentNode {const char* name="";const char* state="";float activity=0;bool selected=false,enabled=true;};
struct ContentRow {const char* primary="";const char* secondary="";const char* state="";bool selected=false,enabled=true;uint8_t icon=0;};
inline ValueShape settings_value_shape(unsigned id){return id==8?ValueShape::Bipolar:id==5?ValueShape::Bar:id==6?ValueShape::Segments:ValueShape::Chip;}
}}
