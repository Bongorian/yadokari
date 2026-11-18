#pragma once

namespace groove {
// Layout shared with third_party/chimera/src/lib.rs.
struct ChimeraGeometry {
 struct Point {float x,y;};
 static constexpr unsigned points=32,sections=16;
 Point outer[points],inner[points],tongue[points],nasal[points],glottis;
 Point section_inner[sections],section_outer[sections];
 float pressure[sections],gate;
};
static_assert(sizeof(ChimeraGeometry)==1356,"Chimera C ABI geometry layout");
}
