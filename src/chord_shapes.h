#pragma once
#include <stdint.h>
namespace groove {
constexpr unsigned kChordShapes=13;
struct ChordShape {const char* name;const char* suffix;uint8_t notes[4];};
inline const ChordShape& chord_shape_definition(unsigned shape){
 static constexpr ChordShape shapes[]={
 {"SCALE","",{0,4,7,255}},{"MAJOR","",{0,4,7,255}},{"MINOR","m",{0,3,7,255}},
 {"SUS4","sus4",{0,5,7,255}},{"DIM","dim",{0,3,6,255}},{"DOM 7","7",{0,4,7,10}},
 {"MAJ7","maj7",{0,4,7,11}},{"MIN7","m7",{0,3,7,10}},{"MIN7b5","m7b5",{0,3,6,10}},
 {"AUG","aug",{0,4,8,255}},{"SUS2","sus2",{0,2,7,255}},{"6","6",{0,4,7,9}},{"ADD9","add9",{0,4,7,14}}};
 return shapes[shape<kChordShapes?shape:0];
}
}
