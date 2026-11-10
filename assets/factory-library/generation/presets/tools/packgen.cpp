// packgen — MK3 signature preset pack generator / verifier.
//
// Builds .mgxp preset files (MGXC v12, ProjectState kind=Preset) from a
// JSON definition file, using the current canonical MK3 codec
// (repository src/, historical vendor provenance is retained). Construction mirrors the firmware's
// own factory preset builder (future_factory_presets.h): the sound lives
// on track 0 of a Preset project; layers embed their leaves; the
// optional PFXS v1 snapshot carries insert FX + sends.
//
// MGXC v12 requires every standalone preset to carry a PresetIdentity
// (IDEN v1). Identities are NOT computed here: they come from the
// pack's identity manifest (preset-guids.json, see IDENTITY.md), whose
// guid128 values are the repo-pinned factory allocation
// (assets/factory-library/identities.json) plus the derived sampler
// set. The manifest is a required third argument:
//   packgen gen|verify <presets.json> <outdir> <preset-guids.json>
//
// Parameters are always addressed by descriptor NAME and applied with
// the codec's own set_engine_parameter(), which validates range and
// discrete-step constraints against the current code's descriptors.
// A name that does not exist in the current registry is a hard error.
//
// Modes:
//   packgen gen    <presets.json> <outdir>   build all presets
//   packgen verify <presets.json> <outdir>   decode + full check of outdir
//
// Optional 5th argument "factory": the output files are factory
// masters whose device placement is FACTORY/PRESETS/LIBRARY/<file>,
// so asset records are stored relative to that directory
// ("../../../" + canonical) instead of the user shelf's PRESETS/
// ("../" forms in the table). Verify canonicalizes against the
// matching base directory. Used for the MGXC12 factory submission.
//
// Sample/SF2 assets referenced by presets. Path records follow the
// firmware's own on-disk convention (file_access.h project_paths()):
// the in-memory canonical path is root-relative, and a file stored in
// PRESETS/ carries it relative to its own directory ("../" prefix).
// The loader canonicalizes the stored path against the file's
// directory and verifies existence + CRC (session_files.h
// load_preset / verify_project_assets), so the stored form below is
// what a PRESETS/*.mgxp must contain. CRC32 is CRC-32/ISO-HDLC over
// the file, matching file_checksum() / SampleMetadata.content_crc.
//   key          stored (in PRESETS/)              canonical (device)
//   demo-slices  ../ASSETS/SAMPLES/demo-slices.wav ASSETS/SAMPLES/demo-slices.wav
//                48kHz 48000f mono, crc 0x7c7e7b61 (benchmark_project.h)
//   va-hit       ../ASSETS/SAMPLES/GBX-22f22446-VA-Hit.wav
//                ASSETS/SAMPLES/GBX-22f22446-VA-Hit.wav
//                48kHz 48000f stereo, crc 0x22f22446 (repo file
//                assets/factory/VA-Hit.wav; the DATA placement is the
//                CRC-named copy in ASSETS/SAMPLES, per the firmware
//                importer's remap of the retired assets/factory tree)
//   fm-metal     ../ASSETS/SAMPLES/GBX-9810cb6c-FM-Metal.wav
//                ASSETS/SAMPLES/GBX-9810cb6c-FM-Metal.wav
//                48kHz 48000f stereo, crc 0x9810cb6c (repo file)
//   sine-sf2     ../FACTORY/DEBUG/Sine.sf2         FACTORY/DEBUG/Sine.sf2
//                crc 0xf27be67b (src/soundfont_fixture.h bytes; the
//                firmware's own SF2 debug preset leaves rate/frames 0)
//   fluidr3-gm   ../ASSETS/SOUNDFONTS/FluidR3 GM.sf2
//                ASSETS/SOUNDFONTS/FluidR3 GM.sf2 (user Banks shelf)
//                crc 0x7886b4e9 over 148,398,306 bytes; FluidR3 GM from
//                Debian fluid-soundfont-gm 3.1-6 (assets/FluidR3 GM.sf2);
//                rate/frames 0 like the firmware's own SF2 preset
//
// Sampler set (tools/sampler-presets.json): twelve assets copied verbatim
// from the firmware repo's factory library (assets/factory/, library.json
// revision 2) — six 4-bar drum loops arranged from CC0/public-domain
// one-shots and six packed multisample strips from VCSL/VSCO-2-CE
// (CC0-1.0). Stored/canonical paths follow the kit library's shelf
// convention (ASSETS/SAMPLES/<basename>); each copy's SHA-256 was checked
// against library.json at import time, and the CRCs below are CRC-32/
// ISO-HDLC over the copied bytes. Loop bpm comes from library.json and is
// recorded in SampleMetadata.bpm; multisample zone layouts (slices +
// zones in the definitions) are library.json's own zone tables.

#include "project_codec.h"
#include "future_factory_presets.h"
#include "engine_registry.h"
#include "fx_registry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

using namespace groove;

// ---------------- minimal JSON parser (objects/arrays/strings/numbers) ----
struct Json {
  enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
  double num = 0;
  bool boolean = false;
  std::string str;
  std::vector<Json> arr;
  std::vector<std::pair<std::string, Json>> obj;
  const Json* find(const char* key) const {
    if (type != Obj) return nullptr;
    for (const auto& kv : obj) if (kv.first == key) return &kv.second;
    return nullptr;
  }
};

struct JsonParser {
  const char* p;
  const char* end;
  bool fail = false;
  void ws() { while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p; }
  bool expect(char c) { ws(); if (p < end && *p == c) { ++p; return true; } fail = true; return false; }
  Json value() {
    ws(); Json out;
    if (p >= end) { fail = true; return out; }
    char c = *p;
    if (c == '{') {
      ++p; out.type = Json::Obj;
      ws(); if (p < end && *p == '}') { ++p; return out; }
      while (true) {
        ws(); if (p >= end || *p != '"') { fail = true; return out; }
        Json key = value();
        if (!expect(':')) return out;
        Json v = value();
        out.obj.emplace_back(key.str, std::move(v));
        ws(); if (p < end && *p == ',') { ++p; continue; }
        if (p < end && *p == '}') { ++p; return out; }
        fail = true; return out;
      }
    }
    if (c == '[') {
      ++p; out.type = Json::Arr;
      ws(); if (p < end && *p == ']') { ++p; return out; }
      while (true) {
        Json v = value();
        out.arr.push_back(std::move(v));
        ws(); if (p < end && *p == ',') { ++p; continue; }
        if (p < end && *p == ']') { ++p; return out; }
        fail = true; return out;
      }
    }
    if (c == '"') {
      ++p; out.type = Json::Str;
      while (p < end && *p != '"') {
        if (*p == '\\' && p + 1 < end) {
          ++p;
          switch (*p) {
            case 'n': out.str += '\n'; break;
            case 't': out.str += '\t'; break;
            case 'r': out.str += '\r'; break;
            case 'u': out.str += '?'; if (end - p >= 4) p += 3; break;
            default: out.str += *p; break;
          }
          ++p;
        } else {
          out.str += *p++;
        }
      }
      if (p < end) ++p; else fail = true;
      return out;
    }
    if (c == 't') { if (end - p >= 4 && !std::strncmp(p, "true", 4)) { p += 4; out.type = Json::Bool; out.boolean = true; return out; } fail = true; return out; }
    if (c == 'f') { if (end - p >= 5 && !std::strncmp(p, "false", 5)) { p += 5; out.type = Json::Bool; out.boolean = false; return out; } fail = true; return out; }
    if (c == 'n') { if (end - p >= 4 && !std::strncmp(p, "null", 4)) { p += 4; return out; } fail = true; return out; }
    char* e = nullptr;
    out.num = std::strtod(p, &e);
    if (e == p) { fail = true; return out; }
    p = e; out.type = Json::Num;
    return out;
  }
};

bool read_file(const char* path, std::vector<uint8_t>& out) {
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  out.resize(size_t(n));
  bool ok = n == 0 || std::fread(out.data(), 1, size_t(n), f) == size_t(n);
  std::fclose(f);
  return ok;
}
bool write_file(const char* path, const uint8_t* data, size_t n) {
  FILE* f = std::fopen(path, "wb");
  if (!f) return false;
  bool ok = std::fwrite(data, 1, n, f) == n;
  std::fclose(f);
  return ok;
}

struct Sink {
  std::vector<uint8_t> bytes;
  bool write(const uint8_t* p, size_t n) { bytes.insert(bytes.end(), p, p + n); return true; }
};
struct Source {
  const std::vector<uint8_t>& bytes;
  size_t pos = 0;
  explicit Source(const std::vector<uint8_t>& b) : bytes(b) {}
  bool read(uint8_t* p, size_t n) { if (n > bytes.size() - pos) return false; std::memcpy(p, bytes.data() + pos, n); pos += n; return true; }
  bool at_end() const { return pos == bytes.size(); }
};

// ---------------- factory sample assets ----------------------------------
struct AssetInfo {
  const char* path;       // stored form, relative to PRESETS/ (see header)
  const char* canonical;  // root-relative device path the loader resolves to
  uint32_t rate, frames, crc;
  uint16_t bits;
  uint8_t channels, root;
  uint16_t bpm = 0;       // loop tempo from library.json (0 = unrecorded)
};
// Factory-submission mode (CLI 5th argument "factory"): the file's
// device placement is FACTORY/PRESETS/LIBRARY/<file>, so the stored
// form is the canonical path made relative to that directory.
static bool g_factory = false;
static std::string stored_path(const AssetInfo& info) {
  if (g_factory) return std::string("../../../") + info.canonical;
  return info.path;
}
static const char* verify_base() { return g_factory ? "FACTORY/PRESETS/LIBRARY" : "PRESETS"; }
const std::map<std::string, AssetInfo>& asset_table() {
  static const std::map<std::string, AssetInfo> table = {
    {"demo-slices", {"../ASSETS/SAMPLES/demo-slices.wav", "ASSETS/SAMPLES/demo-slices.wav", 48000, 48000, 0x7c7e7b61u, 16, 1, 60}},
    {"va-hit",      {"../ASSETS/SAMPLES/GBX-22f22446-VA-Hit.wav", "ASSETS/SAMPLES/GBX-22f22446-VA-Hit.wav", 48000, 48000, 0x22f22446u, 16, 2, 60}},
    {"fm-metal",    {"../ASSETS/SAMPLES/GBX-9810cb6c-FM-Metal.wav", "ASSETS/SAMPLES/GBX-9810cb6c-FM-Metal.wav", 48000, 48000, 0x9810cb6cu, 16, 2, 60}},
    {"sine-sf2",    {"../FACTORY/DEBUG/Sine.sf2",         "FACTORY/DEBUG/Sine.sf2",         0, 0, 0xf27be67bu, 0, 0, 60}},
    {"fluidr3-gm",  {"../ASSETS/SOUNDFONTS/FluidR3 GM.sf2", "ASSETS/SOUNDFONTS/FluidR3 GM.sf2", 0, 0, 0x7886b4e9u, 0, 0, 60}},
    // Sampler set: factory-library loops (4 bars each, mono 48 kHz).
    {"break-132",   {"../ASSETS/SAMPLES/L-Breakbeat-132.wav", "ASSETS/SAMPLES/L-Breakbeat-132.wav", 48000, 349091, 0x33950a46u, 16, 1, 60, 132}},
    {"jungle-172",  {"../ASSETS/SAMPLES/L-Jungle-172.wav",    "ASSETS/SAMPLES/L-Jungle-172.wav",    48000, 267907, 0x0b9325afu, 16, 1, 60, 172}},
    {"funk-104",    {"../ASSETS/SAMPLES/L-Funk-104.wav",      "ASSETS/SAMPLES/L-Funk-104.wav",      48000, 443077, 0xad6db2d7u, 16, 1, 60, 104}},
    {"house-124",   {"../ASSETS/SAMPLES/L-House-124.wav",     "ASSETS/SAMPLES/L-House-124.wav",     48000, 371613, 0xe5bd4329u, 16, 1, 60, 124}},
    {"techno-138",  {"../ASSETS/SAMPLES/L-Techno-138.wav",    "ASSETS/SAMPLES/L-Techno-138.wav",    48000, 333913, 0x72f90f24u, 16, 1, 60, 138}},
    {"ambient-90",  {"../ASSETS/SAMPLES/L-Ambient-90.wav",    "ASSETS/SAMPLES/L-Ambient-90.wav",    48000, 512000, 0x0cf80127u, 16, 1, 60, 90}},
    // Sampler set: factory-library packed multisample strips (mono 48 kHz).
    {"grand-piano",    {"../ASSETS/SAMPLES/M-Grand-Piano.wav",    "ASSETS/SAMPLES/M-Grand-Piano.wav",    48000, 1920000, 0xd679f4c3u, 16, 1, 60}},
    {"electric-piano", {"../ASSETS/SAMPLES/M-Electric-Piano.wav", "ASSETS/SAMPLES/M-Electric-Piano.wav", 48000, 1900315, 0xba41059fu, 16, 1, 60}},
    {"clavisynth",     {"../ASSETS/SAMPLES/M-Clavisynth.wav",     "ASSETS/SAMPLES/M-Clavisynth.wav",     48000, 957571, 0x9b756da2u, 16, 1, 60}},
    {"marimba",        {"../ASSETS/SAMPLES/M-Marimba.wav",        "ASSETS/SAMPLES/M-Marimba.wav",        48000, 309855, 0xf55d5217u, 16, 1, 60}},
    {"kalimba",        {"../ASSETS/SAMPLES/M-Kalimba.wav",        "ASSETS/SAMPLES/M-Kalimba.wav",        48000, 232541, 0xa6bdedc4u, 16, 1, 60}},
    {"vibraphone",     {"../ASSETS/SAMPLES/M-Vibraphone.wav",     "ASSETS/SAMPLES/M-Vibraphone.wav",     48000, 1267207, 0x09e5c35bu, 16, 1, 60}},
  };
  return table;
}

// ---------------- parameter setting by descriptor name --------------------
bool find_param_local(Instrument eng, const std::string& name, unsigned& local) {
  const auto* d = engine_descriptor(unsigned(eng));
  if (!d) return false;
  for (unsigned i = 0; i < d->parameters.count; ++i) {
    if (name == d->parameters.declarations[i].value.name) { local = i; return true; }
  }
  return false;
}

// Applies a JSON "params" object ({name: value}) to a track already set
// to the target instrument. Every value goes through the codec's own
// set_engine_parameter(), which enforces the descriptor range and
// discrete-step rules of the current code.
bool apply_params(TrackView t, Instrument eng, const Json& params, const std::string& ctx, std::string& err) {
  if (params.type != Json::Obj) { err = ctx + ": params is not an object"; return false; }
  for (const auto& kv : params.obj) {
    unsigned local;
    if (!find_param_local(eng, kv.first, local)) {
      err = ctx + ": no parameter named '" + kv.first + "' in current descriptors";
      return false;
    }
    if (kv.second.type != Json::Num) { err = ctx + ": parameter '" + kv.first + "' is not a number"; return false; }
    if (!set_engine_parameter(t, local, float(kv.second.num))) {
      const auto* d = engine_parameter(eng, local);
      char buf[256];
      std::snprintf(buf, sizeof buf, "%s: parameter '%s' value %g rejected (range %g..%g%s)",
                    ctx.c_str(), kv.first.c_str(), kv.second.num,
                    d ? d->value.minimum : 0, d ? d->value.maximum : 0,
                    d && d->value.interpolation == Interpolation::Discrete ? ", discrete" : "");
      err = buf;
      return false;
    }
  }
  return true;
}

// ---------------- FM6 custom patch (PATCH = -1) ---------------------------
// Packed DX7 voice layout exactly as the firmware adapter reads it
// (src/fm6_voice.cpp): operator i occupies bytes [17*i, 17*i+17) in
// msfa signal-flow order (index 0 is the first modulator of a chain;
// the chain's carrier is the later index whose algorithm flags carry
// OUT_BUS_ADD). Per operator: [0..3] EG rates, [4..7] EG levels,
// [8] breakpoint, [9] left depth, [10] right depth, [11] L/R curves,
// [12] (detune code << 3) | rate scaling (detune 7 = centre),
// [13] (velocity sensitivity << 2) | AM sensitivity, [14] output level,
// [15] (coarse ratio << 1) | fixed flag, [16] fine ratio.
// Globals: [102..105] pitch EG rates, [106..109] pitch EG levels
// (50 = nominal pitch), [110] algorithm (0..31), [111] feedback (0..7),
// [112..116] LFO (speed, delay, PM depth, AM depth, sync|wave<<1|PMS<<4),
// [117] transpose (24 = centre), [118..127] name.
bool pack_fm6_patch(const Json& spec, uint8_t out[128], std::string& err) {
  std::memset(out, 0, 128);
  auto get_arr = [&](const Json* obj, const char* key, double vals[4]) -> bool {
    const Json* a = obj ? obj->find(key) : nullptr;
    if (!a || a->type != Json::Arr || a->arr.size() != 4) return false;
    for (unsigned i = 0; i < 4; ++i) { if (a->arr[i].type != Json::Num) return false; vals[i] = a->arr[i].num; }
    return true;
  };
  auto byte99 = [&](double v, const char* what) -> int {
    if (v < 0 || v > 99 || std::floor(v) != v) { err = std::string("fm6_patch: ") + what + " out of 0..99"; return -1; }
    return int(v);
  };
  const Json* ops = spec.find("ops");
  if (!ops || ops->type != Json::Arr || ops->arr.size() != 6) { err = "fm6_patch: ops must be an array of 6"; return false; }
  for (unsigned i = 0; i < 6; ++i) {
    const Json& op = ops->arr[i];
    if (op.type == Json::Null) continue;  // silent operator: all zeros
    if (op.type != Json::Obj) { err = "fm6_patch: op is not an object/null"; return false; }
    uint8_t* p = out + 17 * i;
    double r[4], l[4];
    if (!get_arr(&op, "rates", r) || !get_arr(&op, "levels", l)) { err = "fm6_patch: op rates/levels missing"; return false; }
    for (unsigned k = 0; k < 4; ++k) {
      int rv = byte99(r[k], "EG rate"), lv = byte99(l[k], "EG level");
      if (rv < 0 || lv < 0) return false;
      p[k] = uint8_t(rv); p[4 + k] = uint8_t(lv);
    }
    auto field = [&](const char* key, double dflt) -> double {
      const Json* v = op.find(key);
      return v && v->type == Json::Num ? v->num : dflt;
    };
    int bp = byte99(field("breakpoint", 39), "breakpoint"); if (bp < 0) return false;
    p[8] = uint8_t(bp);
    p[9] = 0; p[10] = 0;  // no keyboard level scaling
    p[11] = 0;            // scaling curves (unused at depth 0)
    double det = field("detune", 7), rs = field("rate_scaling", 0);
    if (det < 0 || det > 15 || rs < 0 || rs > 7) { err = "fm6_patch: detune/rate_scaling range"; return false; }
    p[12] = uint8_t((unsigned(det) << 3) | unsigned(rs));
    double vel = field("vel_sens", 0), am = field("am_sens", 0);
    if (vel < 0 || vel > 7 || am < 0 || am > 3) { err = "fm6_patch: sensitivity range"; return false; }
    p[13] = uint8_t((unsigned(vel) << 2) | unsigned(am));
    int ol = byte99(field("out_level", 0), "out_level"); if (ol < 0) return false;
    p[14] = uint8_t(ol);
    double coarse = field("coarse", 1), fine = field("fine", 0), fixed = field("fixed", 0);
    if (coarse < 0 || coarse > 31 || fine < 0 || fine > 99 || (fixed != 0 && fixed != 1)) { err = "fm6_patch: ratio range"; return false; }
    p[15] = uint8_t((unsigned(coarse) << 1) | unsigned(fixed));
    p[16] = uint8_t(fine);
  }
  const Json* peg = spec.find("pitch_eg");
  double pr[4] = {99, 99, 99, 99}, pl[4] = {50, 50, 50, 50};
  if (peg) {
    if (!get_arr(peg, "rates", pr) || !get_arr(peg, "levels", pl)) { err = "fm6_patch: pitch_eg rates/levels missing"; return false; }
  }
  for (unsigned k = 0; k < 4; ++k) {
    int rv = byte99(pr[k], "pitch EG rate"), lv = byte99(pl[k], "pitch EG level");
    if (rv < 0 || lv < 0) return false;
    out[102 + k] = uint8_t(rv); out[106 + k] = uint8_t(lv);
  }
  auto top = [&](const char* key, double dflt) -> double {
    const Json* v = spec.find(key);
    return v && v->type == Json::Num ? v->num : dflt;
  };
  double algo = top("algorithm", 1), fb = top("feedback", 0), transpose = top("transpose", 24);
  if (algo < 1 || algo > 32 || fb < 0 || fb > 7 || transpose < 0 || transpose > 48) { err = "fm6_patch: algorithm/feedback/transpose range"; return false; }
  out[110] = uint8_t(algo - 1);
  out[111] = uint8_t(fb);
  out[117] = uint8_t(transpose);
  const Json* name = spec.find("name");
  std::string nm = name && name->type == Json::Str ? name->str : "CUSTOM";
  for (unsigned i = 0; i < 10; ++i) out[118 + i] = i < nm.size() ? uint8_t(nm[i]) : uint8_t(' ');
  return true;
}

// Identity manifest (defined with its loader below, before load_defs).
std::map<std::string, PresetIdentity> g_identities;

// ---------------- preset construction -------------------------------------
bool set_name(char (&dst)[96], const std::string& name) {
  if (name.empty() || name.size() >= 96) return false;
  for (char c : name) if (c < 32 || c > 126) return false;
  std::memcpy(dst, name.data(), name.size());
  dst[name.size()] = 0;
  return true;
}

// Fills assets[slot] of the project from a JSON "sample" object and
// returns the slot. -1 on error.
int apply_sample(ProjectState& p, TrackView t, const Json& sample, const std::string& ctx, std::string& err) {
  const Json* asset = sample.find("asset");
  if (!asset || asset->type != Json::Str) { err = ctx + ": sample.asset missing"; return -1; }
  auto it = asset_table().find(asset->str);
  if (it == asset_table().end()) { err = ctx + ": unknown factory asset '" + asset->str + "'"; return -1; }
  const Json* slotj = sample.find("slot");
  unsigned slot = slotj && slotj->type == Json::Num ? unsigned(slotj->num) : 0;
  if (slot >= kSampleAssets) { err = ctx + ": sample slot out of range"; return -1; }
  auto& a = p.assets[slot];
  std::memset(a.path, 0, sizeof a.path);
  const std::string stored = stored_path(it->second);
  std::memcpy(a.path, stored.c_str(), stored.size());
  a.metadata.rate = it->second.rate;
  a.metadata.frames = it->second.frames;
  a.metadata.content_crc = it->second.crc;
  a.metadata.bits = it->second.bits;
  a.metadata.channels = it->second.channels;
  a.metadata.root = it->second.root;
  a.metadata.bpm = it->second.bpm;
  t.sample_slot = uint8_t(slot);
  if (const Json* loop = sample.find("loop")) t.sample_loop = loop->boolean;
  if (const Json* rev = sample.find("reverse")) t.sample_reverse = rev->boolean;
  if (const Json* edit = sample.find("edit")) {
    if (edit->type != Json::Obj) { err = ctx + ": sample.edit is not an object"; return -1; }
    for (const auto& kv : edit->obj) {
      // Array/object fields: "markers" (slice boundaries, normalized;
      // slice_count is derived as count-1), "pads" (16 pad->slice
      // indices), "zones" (one object per slice: root/key_low/key_high/
      // velocity_low/velocity_high). valid_sample_edit() below enforces
      // the firmware rules (zone_count == slice_count when zones exist,
      // strictly increasing markers, ranges).
      if (kv.first == "markers") {
        if (kv.second.type != Json::Arr || kv.second.arr.size() < 2 || kv.second.arr.size() > 65) {
          err = ctx + ": sample.edit.markers must be an array of 2..65 numbers"; return -1;
        }
        for (size_t i = 0; i < kv.second.arr.size(); ++i) {
          if (kv.second.arr[i].type != Json::Num) { err = ctx + ": sample.edit.markers entry not a number"; return -1; }
          t.sample_edit.markers[i] = float(kv.second.arr[i].num);
        }
        t.sample_edit.slice_count = uint8_t(kv.second.arr.size() - 1);
        continue;
      }
      if (kv.first == "pads") {
        if (kv.second.type != Json::Arr || kv.second.arr.size() != 16) {
          err = ctx + ": sample.edit.pads must be an array of 16 numbers"; return -1;
        }
        for (unsigned i = 0; i < 16; ++i) {
          if (kv.second.arr[i].type != Json::Num || kv.second.arr[i].num < 0 || kv.second.arr[i].num > 63 ||
              std::floor(kv.second.arr[i].num) != kv.second.arr[i].num) {
            err = ctx + ": sample.edit.pads entry out of range"; return -1;
          }
          t.sample_edit.pads[i] = uint8_t(kv.second.arr[i].num);
        }
        continue;
      }
      if (kv.first == "zones") {
        if (kv.second.type != Json::Arr || kv.second.arr.empty() || kv.second.arr.size() > 16) {
          err = ctx + ": sample.edit.zones must be an array of 1..16 objects"; return -1;
        }
        for (size_t i = 0; i < kv.second.arr.size(); ++i) {
          const Json& z = kv.second.arr[i];
          auto zf = [&](const char* key, double& out) -> bool {
            const Json* v = z.find(key);
            if (!v || v->type != Json::Num || v->num < 0 || v->num > 127 ||
                std::floor(v->num) != v->num) return false;
            out = v->num; return true;
          };
          double root, kl, kh, vl, vh;
          if (z.type != Json::Obj || !zf("root", root) || !zf("key_low", kl) || !zf("key_high", kh) ||
              !zf("velocity_low", vl) || !zf("velocity_high", vh)) {
            err = ctx + ": sample.edit.zones entry malformed"; return -1;
          }
          t.sample_edit.zones[i] = SampleZone{uint8_t(root), uint8_t(kl), uint8_t(kh), uint8_t(vl), uint8_t(vh)};
        }
        t.sample_edit.zone_count = uint8_t(kv.second.arr.size());
        continue;
      }
      if (kv.second.type != Json::Num) { err = ctx + ": sample.edit value not a number"; return -1; }
      float v = float(kv.second.num);
      if (kv.first == "start") t.sample_edit.start = v;
      else if (kv.first == "end") t.sample_edit.end = v;
      else if (kv.first == "speed") t.sample_edit.speed = v;
      else if (kv.first == "fade_ms") t.sample_edit.fade_ms = v;
      else if (kv.first == "crossfade_ms") t.sample_edit.crossfade_ms = v;
      else if (kv.first == "attack_ms") t.sample_edit.attack_ms = v;
      else if (kv.first == "release_ms") t.sample_edit.release_ms = v;
      else if (kv.first == "choke") {
        if (v < 0 || v > 16 || std::floor(v) != v) { err = ctx + ": sample choke out of range"; return -1; }
        t.sample_edit.choke = uint8_t(v);
      }
      else if (kv.first == "mode") {
        if (v < 0 || v > 4 || std::floor(v) != v) { err = ctx + ": sample mode out of range"; return -1; }
        t.sample_edit.mode = SampleMode(unsigned(v));
      } else { err = ctx + ": unknown sample.edit field '" + kv.first + "'"; return -1; }
    }
    if (!valid_sample_edit(t.sample_edit)) { err = ctx + ": sample edit fails valid_sample_edit"; return -1; }
  }
  return int(slot);
}

bool resolve_engine(const Json& def, Instrument& eng, const std::string& ctx, std::string& err) {
  const Json* g = def.find("guid");
  if (!g || g->type != Json::Str) { err = ctx + ": guid missing"; return false; }
  if (!engine_from_guid(g->str.c_str(), unsigned(g->str.size()), eng)) {
    err = ctx + ": unknown engine guid '" + g->str + "'";
    return false;
  }
  return true;
}

// Builds one sound (top-level track or layer leaf source) onto track t
// of project p. def keys: guid, params, sample, fm6_patch.
bool build_sound(ProjectState& p, TrackView t, const Json& def, const std::string& ctx, std::string& err) {
  Instrument eng;
  if (!resolve_engine(def, eng, ctx, err)) return false;
  if (eng == Instrument::Layer || eng == Instrument::DrumKit || eng == Instrument::ExternalMidi) {
    err = ctx + ": engine not usable as a sound/leaf";
    return false;
  }
  t.instrument = eng;
  if (const Json* params = def.find("params")) {
    if (!apply_params(t, eng, *params, ctx, err)) return false;
  }
  if (const Json* patch = def.find("fm6_patch")) {
    if (eng != Instrument::Fm6) { err = ctx + ": fm6_patch on a non-FM6 engine"; return false; }
    if (!pack_fm6_patch(*patch, t.fm6.patch, err)) { err = ctx + ": " + err; return false; }
  }
  if (const Json* sample = def.find("sample")) {
    if (apply_sample(p, t, *sample, ctx, err) < 0) return false;
  }
  return true;
}

bool build_preset(const Json& def, ProjectState& p, std::string& err) {
  const Json* namej = def.find("name");
  if (!namej || namej->type != Json::Str) { err = "preset without a name"; return false; }
  const std::string ctx = "preset '" + namej->str + "'";
  make_empty_project(p);
  // make_empty_project() does not reset preset_fx; clear it explicitly
  // so a reused ProjectState cannot leak a previous preset's snapshot.
  p.preset_fx = PresetFxSnapshot{};
  p.content_kind = ContentKind::Preset;
  auto t = track_at(p, 0, 0);
  if (!set_name(t.preset_name, namej->str)) { err = ctx + ": bad preset name"; return false; }
  // MGXC v12: the standalone identity comes from the pack manifest,
  // never from the definition file or a filename derivation.
  auto idit = g_identities.find(namej->str);
  if (idit == g_identities.end()) { err = ctx + ": no identity allocated in the manifest"; return false; }
  t.preset_identity = idit->second;

  if (const Json* layer = def.find("layer")) {
    t.instrument = Instrument::Layer;
    const Json* parts = layer->find("parts");
    if (!parts || parts->type != Json::Arr || parts->arr.empty() || parts->arr.size() > kLayerParts) {
      err = ctx + ": layer.parts must be 1..4 entries";
      return false;
    }
    // Scratch project used to build each leaf through a track view,
    // then snapshot_leaf() copies the portable payload into the part.
    auto* scratch = new ProjectState();
    t.layer.count = uint8_t(parts->arr.size());
    for (unsigned i = 0; i < parts->arr.size(); ++i) {
      const Json& pd = parts->arr[i];
      std::string pctx = ctx + " part " + std::to_string(i);
      make_empty_project(*scratch);
      auto st = track_at(*scratch, 0, 0);
      if (!build_sound(*scratch, st, pd, pctx, err)) { delete scratch; return false; }
      // Merge the scratch project's asset records into the real one.
      // snapshot_leaf() copies only the sound payload, so without this
      // step a sampler/SF2 leaf would point at an empty asset record
      // (the round-1 D-50 files had exactly that defect). Slots are
      // remapped onto identical or free records of the main project.
      for (unsigned s = 0; s < kSampleAssets; ++s) {
        const auto& sa = scratch->assets[s];
        if (!sa.path[0]) continue;
        int target = -1;
        for (unsigned q = 0; q < kSampleAssets && target < 0; ++q) {
          if (p.assets[q].path[0] && !std::strcmp(p.assets[q].path, sa.path) &&
              same_sample_metadata(p.assets[q].metadata, sa.metadata)) target = int(q);
        }
        if (target < 0 && !p.assets[s].path[0]) target = int(s);
        for (unsigned q = 0; target < 0 && q < kSampleAssets; ++q) {
          if (!p.assets[q].path[0]) target = int(q);
        }
        if (target < 0) { err = pctx + ": no free asset slot for the leaf sample"; delete scratch; return false; }
        p.assets[target] = sa;
        if (st.sample_slot == s) st.sample_slot = uint8_t(target);
      }
      auto& part = t.layer.parts[i];
      snapshot_leaf(part.sound, st);
      part.source = kNoPreset;
      auto field = [&](const char* key, float dflt) -> float {
        const Json* v = pd.find(key);
        return v && v->type == Json::Num ? float(v->num) : dflt;
      };
      part.level = field("level", 0.5f);
      part.pan = field("pan", 0.0f);
      part.detune = field("detune", 0.0f);
    }
    delete scratch;
    unsigned cost = 0;
    LayerReason status = layer_status(t.layer, &cost);
    if (status != LayerReason::Ready) {
      err = ctx + ": layer not Ready (" + layer_reason(status) + ", cost " + std::to_string(cost) + ")";
      return false;
    }
  } else {
    if (!build_sound(p, t, def, ctx, err)) return false;
  }

  if (const Json* pfxs = def.find("pfxs")) {
    auto& snap = p.preset_fx;
    snap.present = true;
    // Track defaults for both insert slots: BYPASS, A .5, B .25, MIX 1.
    const float dflt[8] = {0, 0.5f, 0.25f, 1, 0, 0.5f, 0.25f, 1};
    for (unsigned i = 0; i < 8; ++i) snap.inserts[i] = dflt[i];
    for (unsigned s = 0; s < 2; ++s) {
      const Json* fx = pfxs->find(s == 0 ? "fx1" : "fx2");
      if (!fx) continue;
      const Json* type = fx->find("type");
      if (!type || type->type != Json::Num) { err = ctx + ": pfxs fx type missing"; return false; }
      unsigned ft = unsigned(type->num);
      if (ft >= kFxCount || float(ft) != float(type->num)) { err = ctx + ": pfxs fx type out of range"; return false; }
      auto field = [&](const char* key, float d) -> float {
        const Json* v = fx->find(key);
        return v && v->type == Json::Num ? float(v->num) : d;
      };
      snap.inserts[s * 4 + 0] = float(ft);
      snap.inserts[s * 4 + 1] = field("a", 0.5f);
      snap.inserts[s * 4 + 2] = field("b", 0.25f);
      snap.inserts[s * 4 + 3] = field("mix", 1.0f);
    }
    if (const Json* d = pfxs->find("delay")) snap.delay = float(d->num);
    if (const Json* r = pfxs->find("reverb")) snap.reverb = float(r->num);
    snap.disabled = 0;
    if (!snap.valid()) { err = ctx + ": PFXS snapshot fails PresetFxSnapshot::valid()"; return false; }
  }

  if (!valid_project(p)) { err = ctx + ": fails valid_project()"; return false; }
  return true;
}

// ---------------- identity manifest ---------------------------------------
// preset-guids.json: { "<preset name>": {"guid128": "<32 hex>",
// "revision": 1, "origin": "factory", ...}, ... }. Loaded once per run.
bool load_identities(const char* path, std::string& err) {
  std::vector<uint8_t> bytes;
  if (!read_file(path, bytes)) { err = std::string("cannot read identity manifest ") + path; return false; }
  JsonParser parser{reinterpret_cast<const char*>(bytes.data()),
                    reinterpret_cast<const char*>(bytes.data()) + bytes.size(), false};
  Json root = parser.value();
  if (parser.fail || root.type != Json::Obj) { err = "identity manifest is not a JSON object"; return false; }
  for (const auto& kv : root.obj) {
    const Json& e = kv.second;
    const Json* g = e.find("guid128");
    const Json* r = e.find("revision");
    const Json* o = e.find("origin");
    if (!g || g->type != Json::Str || g->str.size() != 32 || !r || r->type != Json::Num ||
        !o || o->type != Json::Str) {
      err = "identity manifest entry '" + kv.first + "' is malformed";
      return false;
    }
    PresetIdentity id;
    if (!parse_preset_guid(g->str.c_str(), id.guid)) {
      err = "identity manifest entry '" + kv.first + "': guid128 is not 32 hex chars";
      return false;
    }
    id.revision = uint32_t(r->num);
    if (o->str == "factory") id.origin = PresetOrigin::Factory;
    else if (o->str == "user") id.origin = PresetOrigin::User;
    else { err = "identity manifest entry '" + kv.first + "': unknown origin '" + o->str + "'"; return false; }
    if (!id.assigned() || !id.valid()) {
      err = "identity manifest entry '" + kv.first + "': identity is not valid/assigned";
      return false;
    }
    g_identities[kv.first] = id;
  }
  return true;
}

const Json* load_defs(const char* path, Json& root, std::string& err) {
  std::vector<uint8_t> bytes;
  if (!read_file(path, bytes)) { err = std::string("cannot read ") + path; return nullptr; }
  JsonParser parser{reinterpret_cast<const char*>(bytes.data()), reinterpret_cast<const char*>(bytes.data()) + bytes.size(), false};
  // Keep the byte buffer alive by leaking it (process-lifetime data).
  static std::vector<uint8_t>* keep = nullptr;
  keep = new std::vector<uint8_t>(std::move(bytes));
  parser.p = reinterpret_cast<const char*>(keep->data());
  parser.end = parser.p + keep->size();
  root = parser.value();
  if (parser.fail || root.type != Json::Obj) { err = "definition file is not valid JSON"; return nullptr; }
  const Json* presets = root.find("presets");
  if (!presets || presets->type != Json::Arr) { err = "definition file has no presets array"; return nullptr; }
  return presets;
}

std::string file_name_for(const Json& def) {
  const Json* f = def.find("file");
  if (f && f->type == Json::Str) return f->str;
  const Json* n = def.find("name");
  return (n && n->type == Json::Str ? n->str : std::string("unnamed")) + ".mgxp";
}

int cmd_gen(const char* defs_path, const char* outdir, const char* guids_path) {
  Json root; std::string err;
  if (!load_identities(guids_path, err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
  const Json* presets = load_defs(defs_path, root, err);
  if (!presets) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
  std::vector<std::string> names;
  auto* project = new ProjectState();
  unsigned ok = 0;
  for (const auto& def : presets->arr) {
    const Json* n = def.find("name");
    std::string name = n && n->type == Json::Str ? n->str : "?";
    if (std::find(names.begin(), names.end(), name) != names.end()) {
      std::fprintf(stderr, "error: duplicate preset name '%s'\n", name.c_str());
      return 1;
    }
    names.push_back(name);
    if (!build_preset(def, *project, err)) {
      std::fprintf(stderr, "error: %s\n", err.c_str());
      return 1;
    }
    ChunkBuffer out;
    if (!encode_project_buffer(out, *project)) {
      std::fprintf(stderr, "error: encode failed for '%s'\n", name.c_str());
      return 1;
    }
    std::string path = std::string(outdir) + "/" + file_name_for(def);
    if (!write_file(path.c_str(), out.data, out.size)) {
      std::fprintf(stderr, "error: cannot write %s\n", path.c_str());
      return 1;
    }
    std::printf("gen  %-28s %6u bytes  %s\n", name.c_str(), out.size, file_name_for(def).c_str());
    ++ok;
  }
  std::printf("generated %u presets\n", ok);
  delete project;
  return 0;
}

// ---------------- verification --------------------------------------------
// The decoded file is compared field-by-field against a freshly built
// in-memory preset from the same definition. Anything the codec fails
// to round-trip (a parameter, a leaf, the PFXS snapshot, an asset
// record) shows up as a mismatch here.
bool feq(float a, float b) { return a == b; }

template <typename S>
bool compare_state_arrays(const S& a, const S& b, const char* what, const std::string& ctx, std::string& err) {
  for (unsigned i = 0; i < sizeof(a.values) / sizeof(a.values[0]); ++i) {
    if (!feq(a.values[i], b.values[i])) {
      char buf[200];
      std::snprintf(buf, sizeof buf, "%s: %s.values[%u] round-trip mismatch: built %g, decoded %g",
                    ctx.c_str(), what, i, double(a.values[i]), double(b.values[i]));
      err = buf;
      return false;
    }
  }
  return true;
}

// Compares the sound payload of two track views (one side may be a
// layer leaf converted to TrackView by the caller when needed).
bool compare_sound_fields(const TrackView& a, const TrackView& b, const std::string& ctx, std::string& err) {
  if (a.instrument != b.instrument) { err = ctx + ": instrument mismatch"; return false; }
  if (a.plaits_model != b.plaits_model) { err = ctx + ": plaits model mismatch"; return false; }
  for (unsigned i = 0; i < 5; ++i) {
    if (!feq(a.engine_parameters[i], b.engine_parameters[i])) { err = ctx + ": macro parameter mismatch"; return false; }
  }
  if (a.sample_slot != b.sample_slot || a.sample_loop != b.sample_loop || a.sample_reverse != b.sample_reverse ||
      a.sample_slice != b.sample_slice || a.sample_slices != b.sample_slices) {
    err = ctx + ": sample slot/flags mismatch"; return false;
  }
  if (!same_sample_edit(a.sample_edit, b.sample_edit)) { err = ctx + ": sample edit mismatch"; return false; }
  if (!compare_state_arrays(a.virtual_analog, b.virtual_analog, "virtual_analog", ctx, err)) return false;
  if (!compare_state_arrays(a.additive, b.additive, "additive", ctx, err)) return false;
  if (!compare_state_arrays(a.phase_dist, b.phase_dist, "phase_dist", ctx, err)) return false;
  if (!compare_state_arrays(a.formant, b.formant, "formant", ctx, err)) return false;
  if (!compare_state_arrays(a.granular, b.granular, "granular", ctx, err)) return false;
  if (!compare_state_arrays(a.soundfont, b.soundfont, "soundfont", ctx, err)) return false;
  if (!compare_state_arrays(a.texture, b.texture, "texture", ctx, err)) return false;
  if (!compare_state_arrays(a.resonator, b.resonator, "resonator", ctx, err)) return false;
  if (std::memcmp(&a.wavetable, &b.wavetable, sizeof a.wavetable) != 0) { err = ctx + ": wavetable state mismatch"; return false; }
  if (!compare_state_arrays(a.fm6, b.fm6, "fm6", ctx, err)) return false;
  if (std::memcmp(a.fm6.patch, b.fm6.patch, 128) != 0) { err = ctx + ": fm6 patch mismatch"; return false; }
  for (unsigned i = 0; i < 4; ++i) {
    if (!feq(a.daisy_adsr[i], b.daisy_adsr[i])) { err = ctx + ": daisy ADSR mismatch"; return false; }
  }
  if (std::memcmp(&a.creative, &b.creative, sizeof a.creative) != 0) { err = ctx + ": creative state mismatch"; return false; }
  return true;
}

bool compare_leaf(const SoundLeaf& a, const SoundLeaf& b, const std::string& ctx, std::string& err) {
  if (a.instrument != b.instrument) { err = ctx + ": leaf instrument mismatch"; return false; }
  if (!compare_state_arrays(a.virtual_analog, b.virtual_analog, "virtual_analog", ctx, err)) return false;
  if (!compare_state_arrays(a.additive, b.additive, "additive", ctx, err)) return false;
  if (!compare_state_arrays(a.phase_dist, b.phase_dist, "phase_dist", ctx, err)) return false;
  if (!compare_state_arrays(a.formant, b.formant, "formant", ctx, err)) return false;
  if (!compare_state_arrays(a.granular, b.granular, "granular", ctx, err)) return false;
  if (!compare_state_arrays(a.soundfont, b.soundfont, "soundfont", ctx, err)) return false;
  if (std::memcmp(&a.wavetable, &b.wavetable, sizeof a.wavetable) != 0) { err = ctx + ": leaf wavetable mismatch"; return false; }
  if (!compare_state_arrays(a.fm6, b.fm6, "fm6", ctx, err)) return false;
  if (std::memcmp(a.fm6.patch, b.fm6.patch, 128) != 0) { err = ctx + ": leaf fm6 patch mismatch"; return false; }
  for (unsigned i = 0; i < 5; ++i) if (!feq(a.parameters[i], b.parameters[i])) { err = ctx + ": leaf macro mismatch"; return false; }
  if (a.model != b.model || a.sample_slot != b.sample_slot || a.sample_loop != b.sample_loop ||
      a.sample_reverse != b.sample_reverse) { err = ctx + ": leaf model/sample mismatch"; return false; }
  if (!same_sample_edit(a.sample_edit, b.sample_edit)) { err = ctx + ": leaf sample edit mismatch"; return false; }
  for (unsigned i = 0; i < 4; ++i) if (!feq(a.daisy_adsr[i], b.daisy_adsr[i])) { err = ctx + ": leaf ADSR mismatch"; return false; }
  return true;
}

bool verify_one(const Json& def, const std::string& path, std::string& err, std::string& info) {
  const Json* nj = def.find("name");
  std::string name = nj && nj->type == Json::Str ? nj->str : "?";
  std::string ctx = "preset '" + name + "'";
  std::vector<uint8_t> bytes;
  if (!read_file(path.c_str(), bytes)) { err = ctx + ": file missing: " + path; return false; }
  // (a) header: magic, version == kFormatVersion (MGXC v12), content
  // kind byte = Preset (3).
  if (bytes.size() < 16 || std::memcmp(bytes.data(), "MGXC", 4) != 0) { err = ctx + ": bad magic"; return false; }
  unsigned version = unsigned(bytes[4]) | (unsigned(bytes[5]) << 8);
  if (version != kFormatVersion) { err = ctx + ": header version is not the current MGXC version"; return false; }
  // First frame is TYPE: tag at bytes 10..13, sub-version byte 1 at
  // offset 18, content kind at offset 22 (the layout data_layout.h's
  // Browser classifier uses).
  if (bytes.size() < 23 || std::memcmp(bytes.data() + 10, "TYPE", 4) != 0 ||
      bytes[14] != 1 || bytes[18] != 1 ||
      bytes[22] != uint8_t(ContentKind::Preset)) {
    err = ctx + ": header TYPE frame kind is not Preset"; return false;
  }
  // (b) canonical decode + validity.
  auto* decoded = new ProjectState();
  Source source(bytes);
  bool ok = decode_project(source, *decoded) && source.at_end();
  if (!ok) { delete decoded; err = ctx + ": decode_project failed"; return false; }
  if (!valid_project(*decoded)) { delete decoded; err = ctx + ": decoded project fails valid_project"; return false; }
  if (decoded->content_kind != ContentKind::Preset) { delete decoded; err = ctx + ": decoded kind is not Preset"; return false; }
  // (f) re-encoding the decoded project must reproduce the file bytes
  // exactly (the codec is deterministic; any drift is a defect).
  {
    ChunkBuffer re;
    if (!encode_project_buffer(re, *decoded) || re.size != bytes.size() ||
        std::memcmp(re.data, bytes.data(), bytes.size()) != 0) {
      delete decoded; err = ctx + ": re-encode of the decoded project is not byte-identical"; return false;
    }
  }
  auto dt = track_at(*decoded, 0, 0);
  if (name != dt.preset_name) { delete decoded; err = ctx + ": decoded preset name mismatch"; return false; }
  // (i) identity: the decoded standalone IDEN must equal the manifest
  // allocation for this preset name-for-name (guid bytes, revision,
  // origin, empty parent).
  {
    auto idit = g_identities.find(name);
    if (idit == g_identities.end()) { delete decoded; err = ctx + ": no identity allocated in the manifest"; return false; }
    if (!same_preset_identity(decoded->tracks[0].preset_identity, idit->second)) {
      char got[33];
      preset_guid_text(decoded->tracks[0].preset_identity.guid, got);
      delete decoded;
      err = ctx + ": decoded identity does not match the manifest (guid " + got + ")";
      return false;
    }
  }
  // (c) engine mountability: the instrument must resolve to a real
  // engine with a contract, not fall back to Unknown.
  Instrument eng = dt.instrument;
  if (eng == Instrument::Unknown || !engine_descriptor(unsigned(eng))) {
    delete decoded; err = ctx + ": instrument does not resolve to an engine"; return false;
  }
  std::string guid = engine_guid(eng);
  const Json* gj = def.find("guid");
  const Json* lj = def.find("layer");
  if (!lj && gj && gj->type == Json::Str && guid != gj->str) {
    delete decoded; err = ctx + ": decoded engine guid mismatch (" + guid + ")"; return false;
  }
  if (lj && eng != Instrument::Layer) { delete decoded; err = ctx + ": expected a layer preset"; return false; }
  // (d) layer readiness + cost.
  unsigned cost = 0;
  if (eng == Instrument::Layer) {
    LayerReason status = layer_status(dt.layer, &cost);
    if (status != LayerReason::Ready) {
      delete decoded; err = ctx + ": decoded layer not Ready (" + layer_reason(status) + ")"; return false;
    }
  }
  // (e) field-by-field comparison against a fresh build.
  auto* expected = new ProjectState();
  if (!build_preset(def, *expected, err)) { delete decoded; delete expected; return false; }
  auto et = track_at(*expected, 0, 0);
  bool good = compare_sound_fields(et, dt, ctx, err);
  if (good && eng == Instrument::Layer) {
    if (et.layer.count != dt.layer.count) { err = ctx + ": layer part count mismatch"; good = false; }
    for (unsigned i = 0; good && i < et.layer.count; ++i) {
      const auto& ep = et.layer.parts[i];
      const auto& dp = dt.layer.parts[i];
      std::string pctx = ctx + " part " + std::to_string(i);
      if (dp.source != kNoPreset) { err = pctx + ": leaf source is not kNoPreset (not self-contained)"; good = false; break; }
      if (!feq(ep.level, dp.level) || !feq(ep.pan, dp.pan) || !feq(ep.detune, dp.detune)) {
        err = pctx + ": level/pan/detune mismatch"; good = false; break;
      }
      good = compare_leaf(ep.sound, dp.sound, pctx, err);
    }
  }
  if (good) {
    if (expected->preset_fx.present != decoded->preset_fx.present) {
      err = ctx + ": PFXS presence mismatch"; good = false;
    } else if (expected->preset_fx.present) {
      const auto& a = expected->preset_fx;
      const auto& b = decoded->preset_fx;
      if (std::memcmp(a.inserts, b.inserts, sizeof a.inserts) != 0 || a.delay != b.delay ||
          a.reverb != b.reverb || a.disabled != b.disabled) {
        err = ctx + ": PFXS content mismatch"; good = false;
      }
    }
  }
  if (good) {
    for (unsigned s = 0; s < kSampleAssets; ++s) {
      const auto& a = expected->assets[s];
      const auto& b = decoded->assets[s];
      if (std::strcmp(a.path, b.path) != 0 || a.metadata.rate != b.metadata.rate ||
          a.metadata.frames != b.metadata.frames || a.metadata.content_crc != b.metadata.content_crc ||
          a.metadata.bits != b.metadata.bits || a.metadata.channels != b.metadata.channels ||
          a.metadata.root != b.metadata.root) {
        err = ctx + ": asset slot " + std::to_string(s) + " mismatch"; good = false; break;
      }
    }
  }
  if (good) {
    // (g) every asset record's stored path must canonicalize against
    // the placement directory (the loader's project_paths() semantics;
    // PRESETS/ in shelf mode, FACTORY/PRESETS/LIBRARY/ in factory mode)
    // to a canonical device path declared in the asset table, with a
    // matching CRC; (h) every sample slot referenced by the sound or
    // by a layer leaf must point at a non-empty asset record, or the
    // device refuses the mount (load_preset / plan_preset_assets).
    auto canon = [](const std::string& base, const std::string& rel, std::string& out) -> bool {
      std::string joined = base + "/" + rel;
      std::vector<std::string> segs;
      for (size_t i = 0; i <= joined.size();) {
        size_t j = joined.find('/', i);
        if (j == std::string::npos) j = joined.size();
        std::string seg = joined.substr(i, j - i);
        if (seg == "..") { if (segs.empty()) return false; segs.pop_back(); }
        else if (!seg.empty() && seg != ".") segs.push_back(seg);
        i = j + 1;
      }
      out.clear();
      for (size_t k = 0; k < segs.size(); ++k) { if (k) out += "/"; out += segs[k]; }
      return true;
    };
    for (unsigned s = 0; good && s < kSampleAssets; ++s) {
      const auto& a = decoded->assets[s];
      if (!a.path[0]) continue;
      std::string resolved;
      if (!canon(verify_base(), a.path, resolved)) {
        err = ctx + ": asset slot " + std::to_string(s) + " path does not canonicalize"; good = false; break;
      }
      bool known = false;
      for (const auto& kv : asset_table()) {
        if (resolved == kv.second.canonical && a.metadata.content_crc == kv.second.crc) { known = true; break; }
      }
      if (!known) {
        err = ctx + ": asset slot " + std::to_string(s) + " resolves to undeclared device path '" + resolved + "'";
        good = false; break;
      }
    }
    auto ref_ok = [&](Instrument ei, uint8_t slot, const std::string& what) -> bool {
      if (ei != Instrument::Sampler && ei != Instrument::SoundFont &&
          ei != Instrument::Granular && ei != Instrument::SpectralFreeze) return true;
      if (slot >= kSampleAssets || !decoded->assets[slot].path[0]) {
        err = ctx + ": " + what + " references an empty asset record (slot " + std::to_string(slot) + ")";
        return false;
      }
      return true;
    };
    if (good && eng != Instrument::Layer) good = ref_ok(eng, dt.sample_slot, "sound");
    if (good && eng == Instrument::Layer) {
      for (unsigned i = 0; good && i < dt.layer.count; ++i) {
        const auto& part = dt.layer.parts[i];
        good = ref_ok(part.sound.instrument, part.sound.sample_slot,
                      "layer part " + std::to_string(i));
      }
    }
  }
  if (good) {
    char buf[160];
    if (eng == Instrument::Layer) {
      std::snprintf(buf, sizeof buf, "engine=layer parts=%u cost=%u/%u", dt.layer.count, cost, unsigned(VoiceBudget::layer_note));
    } else {
      std::snprintf(buf, sizeof buf, "engine=%s", guid.c_str());
    }
    info = buf;
  }
  delete decoded;
  delete expected;
  return good;
}

int cmd_verify(const char* defs_path, const char* outdir, const char* guids_path) {
  Json root; std::string err;
  if (!load_identities(guids_path, err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
  const Json* presets = load_defs(defs_path, root, err);
  if (!presets) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
  unsigned pass = 0, fail = 0;
  for (const auto& def : presets->arr) {
    const Json* n = def.find("name");
    std::string name = n && n->type == Json::Str ? n->str : "?";
    std::string path = std::string(outdir) + "/" + file_name_for(def);
    std::string verr, info;
    if (verify_one(def, path, verr, info)) {
      std::printf("PASS %-28s %s\n", name.c_str(), info.c_str());
      ++pass;
    } else {
      std::printf("FAIL %s\n", verr.c_str());
      ++fail;
    }
  }
  std::printf("verify: %u passed, %u failed, %u total\n", pass, fail, pass + fail);
  return fail == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 5 && argc != 6) {
    std::fprintf(stderr, "usage: packgen gen|verify <presets.json> <outdir> <preset-guids.json> [factory]\n");
    return 2;
  }
  if (argc == 6) {
    if (std::strcmp(argv[5], "factory")) { std::fprintf(stderr, "unknown option '%s'\n", argv[5]); return 2; }
    g_factory = true;
  }
  if (!std::strcmp(argv[1], "gen")) return cmd_gen(argv[2], argv[3], argv[4]);
  if (!std::strcmp(argv[1], "verify")) return cmd_verify(argv[2], argv[3], argv[4]);
  std::fprintf(stderr, "unknown mode '%s'\n", argv[1]);
  return 2;
}
