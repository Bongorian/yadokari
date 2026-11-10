#pragma once
#include <stdint.h>
#include "../ui_axes.h"
namespace groove {
enum class ModalOwner:uint8_t {None,Project,Update,Freeze,Stretch,Loop,Extend,SongImport,SongClear,Reference,SourceDiscard,MidiImport,UiCommand,Twin,Composition};
struct ModalState {ModalOwner owner=ModalOwner::None;uint8_t value=0;ModalKind* axis=nullptr;
 void sync(){if(axis)*axis=owner==ModalOwner::None?ModalKind::None:((owner==ModalOwner::Freeze || owner==ModalOwner::Stretch || owner==ModalOwner::MidiImport || owner==ModalOwner::Twin) && value>=2)?ModalKind::Progress:ModalKind::Confirm;}
};
// Typed accessors for the current operation payload. Only one payload can own input;
// these objects carry no independent confirmation flags.
template<class Value> struct ModalField {ModalState& state;ModalOwner owner;
 operator Value()const{return state.owner==owner?Value(state.value):Value{};}
 Value operator=(Value value){if(value==Value{} && state.owner!=owner)return value;if(value!=Value{}){state.owner=owner;state.value=uint8_t(value);}else if(state.owner==owner){state.owner=ModalOwner::None;state.value=0;}state.sync();return value;}
};
}
