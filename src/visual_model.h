#pragma once
#include <stdint.h>
namespace groove {
enum class VisualElement:uint8_t {None,Scope,Spectrum,Bars,Ring,Curve,Response};
enum class VisualSource:uint8_t {None,TrackPcm,CapturePcm,InputPcm,MainPcm,SamplePeaks,TrackSpectrum,InputSpectrum,MainSpectrum,Harmonics,Vocoder,Loop,Envelope,WavetableScan,Morph,Parameters};
enum class VisualRecipe:uint8_t {Controls,Fm2,Oscillators};
enum class VisualCadence:uint8_t {Static,Control,Audio};
enum class VisualStatus:uint8_t {None,TextureBuffer};
enum class VisualFallback:uint8_t {Empty,Parameters};
enum class ResponseModel:uint8_t {None,LowPass,HighPass,BandPass,Notch,Drive,Fold,Gate,Tilt,Width,Clip,Compressor,Decay,Modulation,Quantize};
struct VisualDeclaration {VisualElement element=VisualElement::Scope;VisualSource source=VisualSource::TrackPcm;VisualCadence cadence=VisualCadence::Audio;VisualFallback fallback=VisualFallback::Empty;ResponseModel model=ResponseModel::None;const char* caption="TRACK POST FADER";VisualStatus status=VisualStatus::None;VisualRecipe recipe=VisualRecipe::Controls;};
struct VisualData {float values[128]{},low[128]{};unsigned count=0;float phase=0,position=0,level=0,recorded=0,amount=0;unsigned layers=0;bool available=false,live=false,bipolar=false,point=false,recording=false;float range_low=0,range_high=1;bool range=false;VisualElement presentation=VisualElement::None;const char* names[16]{};float parameters[8]{};const char* label="NO DATA";};
inline constexpr VisualDeclaration kLoopVisual{VisualElement::Ring,VisualSource::Loop,VisualCadence::Audio,VisualFallback::Empty,ResponseModel::None,"LOOP POSITION / STORED TAKES"};
inline constexpr VisualDeclaration kCaptureVisual{VisualElement::Scope,VisualSource::CapturePcm,VisualCadence::Audio,VisualFallback::Empty,ResponseModel::None,"CAPTURE SOURCE / PCM"};
inline constexpr VisualDeclaration kSampleVisual{VisualElement::Scope,VisualSource::SamplePeaks,VisualCadence::Control,VisualFallback::Empty,ResponseModel::None,"SAMPLE PEAKS / POSITION"};
inline constexpr VisualDeclaration kMorphVisual{VisualElement::Curve,VisualSource::Morph,VisualCadence::Control,VisualFallback::Parameters,ResponseModel::None,"MORPH"};
}
