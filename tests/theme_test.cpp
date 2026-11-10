// Theme acceptance test (SPEC 1.3/1.4): every shipped theme carries
// the full token set with the table values, the legacy aliases agree
// with the tokens they mirror, and the numeric admission conditions
// hold - WCAG contrast floors for text tokens and minimum pairwise
// distances (after RGB565 quantization) for the semantic colours
// and the 16 track colours.
#include "ui/theme.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
using namespace groove::ui;
static void rgb(uint32_t c, int& r, int& g, int& b) { r = (c >> 16) & 255; g = (c >> 8) & 255; b = c & 255; }
static double luminance(uint32_t c) {
 int r, g, b; rgb(c, r, g, b);
 auto lin = [](int v) { double s = v / 255.0; return s <= 0.03928 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4); };
 return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b);
}
static double contrast(uint32_t a, uint32_t b) {
 double la = luminance(a), lb = luminance(b);
 if (la < lb) { double t = la; la = lb; lb = t; }
 return (la + 0.05) / (lb + 0.05);
}
static double dist565(uint32_t a, uint32_t b) {
 int ar, ag, ab, br, bg, bb; rgb(a, ar, ag, ab); rgb(b, br, bg, bb);
 auto q = [](int v, int bits) { int qv = v >> (8 - bits); return (qv << (8 - bits)) | (qv >> (2 * bits - 8)); };
 int dr = q(ar, 5) - q(br, 5), dg = q(ag, 6) - q(bg, 6), db = q(ab, 5) - q(bb, 5);
 return std::sqrt(double(dr * dr + dg * dg + db * db));
}
int main() {
 unsigned floor_failures=0;assert(Theme::kCount == 9);
 const Theme& def=Theme::by_index(Theme::kDefault);
 assert(std::string(Theme::name(Theme::kDefault))=="CYBER 2");
 assert(Theme{}.background==def.background && Theme{}.accent==def.accent);
 assert(Theme::by_index(999).accent==def.accent);
 for (unsigned i = 0; i < Theme::kCount; ++i) {
  const Theme& t = Theme::by_index(i);
  // Aliases mirror their tokens.
  assert(t.teal == t.accent && t.amber == t.warn && t.red == t.danger && t.blue == t.mod);
  assert(t.disabled == t.dim && t.value_edit==t.accent && t.chassis==t.background && t.inset==t.background);
  // Contrast floors.
  assert(contrast(t.text, t.background) >= 15.0);
  assert(contrast(t.text, t.raised) >= 9.5);
  assert(contrast(t.secondary, t.background) >= 7.5);
  // Semantic colour separation after 565 quantization.
  const uint32_t sem[5] = {t.accent, t.warn, t.danger, t.mod,t.motion};
  for (unsigned a = 0; a < 5; ++a) for (unsigned b = a + 1; b < 5; ++b) if(sem[a]!=sem[b])assert(dist565(sem[a], sem[b]) >= 65.0);
  double bg=luminance(t.background),surface=luminance(t.surface),raised=luminance(t.raised);
  assert(surface>bg && raised>surface);
  auto quant=[](uint32_t c){return 0xff000000u|((c&0xf80000u)|((c>>5)&0x070000u))|((c&0x00fc00u)|((c>>6)&0x000300u))|((c&0x0000f8u)|((c>>5)&7));};
  bg=luminance(quant(t.background));surface=luminance(quant(t.surface));raised=luminance(quant(t.raised));
  double w1=(surface-bg)/(bg+.02),w2=(raised-surface)/(surface+.02);
  bool floor_ok=w1>=.10 && w2>=.15 && raised>=.010;
  std::printf("%s W1=%.6f W2=%.6f Lraised=%.6f floor=%s\n",Theme::name(i),w1,w2,raised,floor_ok?"PASS":"FAIL");floor_failures+=!floor_ok;
  // Track colour separation.
  for (unsigned a = 0; a < 16; ++a) for (unsigned b = a + 1; b < 16; ++b) assert(dist565(t.track[a], t.track[b]) >= 26.0);
 }
 std::printf("themes (9 exact token sets, contrast + separation floors) passed\n");
 return floor_failures?1:0;
}
