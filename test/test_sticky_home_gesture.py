"""Compile the production gesture methods for Sticky and other boards."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def method(source, name):
    start = source.index('bool MappedInputManager::' + name + '() const {')
    end = source.index('\n}', start) + 2
    return source[start:end]


class StickyHomeGesture(unittest.TestCase):
    def test_bottom_edge_routes_home_without_opening_text_settings(self):
        source = (ROOT / 'src/MappedInputManager.cpp').read_text()
        methods = '\n'.join(method(source, name) for name in
                            ['wasReaderTextSettingsGesture', 'wasHomeGesture'])
        fixture = '''
#include <cassert>
enum class HomeButtonAction { None, ReaderMenu, Home };
struct MappedInputManager {
  struct GPIO { bool homeKey = false; bool hasHomeKey() const { return homeKey; } } gpio;
  HomeButtonAction homeAction = HomeButtonAction::None;
  bool bottomEdge = false;
  bool wasBottomEdgeUpSwipe() const { return bottomEdge; }
  bool wasReaderTextSettingsGesture() const;
  bool wasHomeGesture() const;
};
''' + methods + '''
int main() {
  MappedInputManager input;
  assert(!input.wasReaderTextSettingsGesture() && !input.wasHomeGesture());
  input.bottomEdge = true;
  assert(input.wasHomeGesture());
  assert(input.wasReaderTextSettingsGesture() == !FREEINK_DEVICE_STICKY);
  input.bottomEdge = false;
  input.homeAction = HomeButtonAction::ReaderMenu;
  assert(input.wasReaderTextSettingsGesture() && !input.wasHomeGesture());
  input.gpio.homeKey = true;
  input.homeAction = HomeButtonAction::Home;
  assert(input.wasHomeGesture() && !input.wasReaderTextSettingsGesture());
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp = Path(temp) / 'gesture.cpp'
            cpp.write_text(fixture)
            for sticky in (0, 1):
                with self.subTest(sticky=sticky):
                    binary = Path(temp) / f'gesture-{sticky}'
                    subprocess.run(['c++', '-std=c++17', f'-DFREEINK_DEVICE_STICKY={sticky}',
                                    str(cpp), '-o', str(binary)], check=True)
                    subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
