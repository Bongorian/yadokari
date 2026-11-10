#include "project.h"
namespace groove {
// One immutable default owns the Layer subobject too. This avoids a duplicate
// 54KiB ROM constant without allocating or constructing a large caller temporary.
const SoundState& empty_sound(){static const SoundState value{};return value;}
const LayerState& empty_layer(){return empty_sound().layer;}
}
