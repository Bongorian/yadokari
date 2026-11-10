#pragma once
#include "mk3_surface.h"
namespace groove { namespace mk3 {
// Transport quantization only: the pinned indexed-wire profile's approximate
// native hues. These are NOT a UI/LED theme or calibrated controller RGB values.
inline uint32_t indexed_appearance(unsigned hue){static constexpr uint32_t native[]={0xff111a20,0xffe46c77,0xffed8868,0xffefad78,0xfff0b768,0xffe0d279,0xffabc985,0xff71c799,0xff40cfb4,0xff80c9bd,0xff70a8de,0xff758fd0,0xff849edd,0xffc0a4de,0xffc592db,0xffd88fba,0xffd17e9b,0xffeaf1ef};return native[hue<18?hue:17];}
}}
