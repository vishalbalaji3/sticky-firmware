"""Exercise production Japanese-card font selection without an ESP32 toolchain."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/SdCardFontSystem.cpp').read_text()
method = source[source.index('int SdCardFontSystem::cardFontId('):].split('\n#if CROSSPOINT_VECTOR_FONTS\n\nvoid SdCardFontSystem::freeTtfSources()', 1)[0]
harness = r'''
#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#define LOG_ERR(...) ((void)0)
constexpr int NOTOSERIF_18_FONT_ID = 18;
struct { char sdFontFamilyName[32]{}; int fontPointSize = 14; int getReaderFontId() const { return 14; } } SETTINGS;
bool allocate = true, loadWorks = true;
int loads = 0;
struct TtfEpdFont {
  bool load(int pt, bool, int bytes, int glyphs) {
    assert(pt == 18 && bytes == 65536 && glyphs == 768); ++loads; return loadWorks;
  }
  int family() const { return 1; }
};
template<class T> std::unique_ptr<T> makeUniqueNoThrow() { return allocate ? std::make_unique<T>() : nullptr; }
int computeTtfFontId(const char*, uint8_t pt) { return 100 + pt; }
struct GfxRenderer {
  int inserts = 0, registrations = 0;
  void insertFont(int, int) { ++inserts; }
  void registerTtfFont(int, TtfEpdFont*) { ++registrations; }
};
struct FontFile { uint8_t pointSize = 16; };
struct FontFamily {
  bool vector = false, available = true;
  FontFile file;
  const FontFile* findNearestSize(int size) const { assert(size == 18); return available ? &file : nullptr; }
};
struct SdCardFontSystem {
  struct Registry {
    bool available = true;
    FontFamily family;
    const FontFamily* findFamily(const char*) const { return available ? &family : nullptr; }
  } registry_;
  struct Manager {
    int result = 116, point = 0;
    int loadFamilyExtraSize(const FontFamily&, GfxRenderer&, int size) { point = size; return result; }
  } manager_;
  std::unique_ptr<TtfEpdFont> ttf_, ttfCard_;
  std::string ttfFamily_;
  int ttfPointSize_ = 14, ttfFontId_ = 114, ttfCardFontId_ = 0;
  int sources = 0;
  void addTtfSources(TtfEpdFont&) { ++sources; }
  int cardFontId(GfxRenderer&);
};
''' + method + r'''
int main() {
  GfxRenderer renderer;
  SdCardFontSystem fonts;
  assert(fonts.cardFontId(renderer) == 18);
  std::strcpy(SETTINGS.sdFontFamilyName, "Japanese");
  assert(fonts.cardFontId(renderer) == 116 && fonts.manager_.point == 16);
  fonts.manager_.result = 0;
  assert(fonts.cardFontId(renderer) == 14);
  fonts.registry_.available = false;
  assert(fonts.cardFontId(renderer) == 14);
#if CROSSPOINT_VECTOR_FONTS
  fonts.ttf_ = std::make_unique<TtfEpdFont>(); fonts.ttfFamily_ = "Japanese";
  allocate = false;
  assert(fonts.cardFontId(renderer) == 14 && renderer.inserts == 0);
  allocate = true; loadWorks = false;
  assert(fonts.cardFontId(renderer) == 14 && renderer.inserts == 0);
  loadWorks = true;
  assert(fonts.cardFontId(renderer) == 118 && renderer.inserts == 1 && renderer.registrations == 1);
  const int previousLoads = loads;
  assert(fonts.cardFontId(renderer) == 118 && loads == previousLoads);
  fonts.ttfPointSize_ = 18;
  assert(fonts.cardFontId(renderer) == fonts.ttfFontId_ && loads == previousLoads);
#endif
  assert(SETTINGS.fontPointSize == 14 && std::strcmp(SETTINGS.sdFontFamilyName, "Japanese") == 0);
}
'''
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    cpp = directory / 'card_font.cpp'; cpp.write_text(harness)
    for vector in (0, 1):
        exe = directory / f'card_font_{vector}'
        subprocess.run(['c++', '-std=c++20', f'-DCROSSPOINT_VECTOR_FONTS={vector}', str(cpp), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
print('Card-font selection passed with and without vector fonts')
