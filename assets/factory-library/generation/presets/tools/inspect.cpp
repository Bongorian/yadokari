// inspect.cpp — read-only MGXC inspector for the v12 factory-content
// submission checks. Uses the current repository canonical codec.
// usage: inspect <file.mgxp|file.mgxk> [...]
// Prints: size, format version, top-level chunk inventory, content kind,
// track-0 identity/name/layer, preset FX presence, non-default pool
// sounds count, and every non-empty asset record (stored path + CRC).
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include "project.h"
#include "project_codec.h"

using namespace groove;

namespace {
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
struct Source {
  const std::vector<uint8_t>& bytes;
  size_t pos = 0;
  explicit Source(const std::vector<uint8_t>& b) : bytes(b) {}
  bool read(uint8_t* p, size_t n) { if (n > bytes.size() - pos) return false; std::memcpy(p, bytes.data() + pos, n); pos += n; return true; }
  bool at_end() const { return pos == bytes.size(); }
};
std::string fourcc(uint32_t k) {
  char b[5] = {char(k & 0xff), char((k >> 8) & 0xff), char((k >> 16) & 0xff), char((k >> 24) & 0xff), 0};
  return b;
}
std::string guid_hex(const PresetGuid& g) {
  char buf[33];
  preset_guid_text(g, buf);
  return buf;
}
uint32_t u32at(const std::vector<uint8_t>& b, size_t at) {
  return uint32_t(b[at]) | (uint32_t(b[at + 1]) << 8) | (uint32_t(b[at + 2]) << 16) | (uint32_t(b[at + 3]) << 24);
}
uint16_t u16at(const std::vector<uint8_t>& b, size_t at) {
  return uint16_t(b[at]) | (uint16_t(b[at + 1]) << 8);
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: inspect <file> [...]\n"); return 2; }
  int rc = 0;
  for (int ai = 1; ai < argc; ++ai) {
    const char* path = argv[ai];
    std::vector<uint8_t> bytes;
    if (!read_file(path, bytes)) { std::printf("%s\tREAD-FAIL\n", path); rc = 1; continue; }
    std::printf("%s\tbytes=%zu", path, bytes.size());
    if (bytes.size() >= 10 && !std::memcmp(bytes.data(), "MGXC", 4))
      std::printf("\tformat=%u", unsigned(u16at(bytes, 4)));
    // Top-level chunk scan: 10-byte header, frames, trailing 4-byte CRC.
    std::printf("\tchunks=");
    size_t at = 10;
    bool first = true;
    while (at + 12 <= bytes.size() - 4) {
      uint32_t kind = u32at(bytes, at);
      uint32_t size = u32at(bytes, at + 8);
      if (at + 12 + size > bytes.size() - 4) { std::printf("!TRUNC"); break; }
      if (!first) std::printf(",");
      first = false;
      std::printf("%s:%u", fourcc(kind).c_str(), size);
      at += 12 + size;
    }
    std::printf("\n");
    Source src(bytes);
    ProjectState* p = new ProjectState();
    bool ok = decode_project(src, *p) && src.at_end();
    if (!ok) { std::printf("\tDECODE-FAIL\n"); rc = 1; delete p; continue; }
    std::printf("\tkind=%u", unsigned(p->content_kind));
    const auto& t = p->tracks[0];
    std::printf("\ttrack0_preset_name=\"%s\"\ttrack_name=\"%s\"", t.preset_name, t.track_name);
    std::printf("\tinstrument=%u", unsigned(t.instrument));
    const auto& id = t.preset_identity;
    std::printf("\tiden={guid=%s rev=%u origin=%u parent=%s prev=%u}",
                guid_hex(id.guid).c_str(), id.revision, unsigned(id.origin),
                guid_hex(id.parent).c_str(), id.parent_revision);
    std::printf("\tpreset_fx=%d", p->preset_fx.present ? 1 : 0);
    std::printf("\tlayer_parts=%u", unsigned(t.layer.count));
    unsigned pool_used = 0;
    const SoundState& empty = empty_sound();
    for (const auto& s : p->sounds)
      if (std::memcmp(&s, &empty, sizeof(SoundState))) ++pool_used;
    std::printf("\tpool_used=%u", pool_used);
    unsigned tracks_nonempty_name = 0;
    for (const auto& tr : p->tracks) if (tr.track_name[0]) ++tracks_nonempty_name;
    std::printf("\ttracks_with_name=%u\n", tracks_nonempty_name);
    for (unsigned i = 0; i < kSampleAssets; ++i) {
      const auto& a = p->assets[i];
      if (!a.path[0]) continue;
      std::printf("\tasset[%u] path=\"%s\" crc=%08x rate=%u frames=%u bits=%u ch=%u root=%u bpm=%u\n",
                  i, a.path, a.metadata.content_crc, a.metadata.rate, a.metadata.frames,
                  unsigned(a.metadata.bits), unsigned(a.metadata.channels),
                  unsigned(a.metadata.root), unsigned(a.metadata.bpm));
    }
    delete p;
  }
  return rc;
}
