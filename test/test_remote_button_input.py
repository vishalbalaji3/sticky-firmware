"""Regression for Sticky's short-click injection into native CrossNext input."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/MappedInputManager.cpp").read_text()
header = (ROOT / "src/MappedInputManager.h").read_text()

def method(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

inject = next(line.strip() for line in header.splitlines() if "void injectConfirmClick()" in line)
harness = r'''
#include <cassert>
#include <cstdint>
#include <algorithm>
unsigned long millis() { return 1000; }
enum class HomeButtonAction { Ignore, Confirm };
struct SideEvent { int action = 0; };
struct HomeButtonInput {
  void setWindow(int) {}
  template<class... T> HomeButtonAction update(T...) { return HomeButtonAction::Ignore; }
};
struct HalGPIO {
  enum { BTN_UP, BTN_DOWN };
  bool released = false, pressed = false;
  void update() {}
  bool wasCapacitivePagePressed() const { return false; }
  bool hasHomeKey() const { return false; }
  bool hasTouch() const { return false; }
  bool wasHomeKeyTapped() const { return false; }
  bool wasHomeKeyLongPressed() const { return false; }
  bool wasHomeKeyPressed() const { return false; }
  bool wasReleased(uint8_t) const { return released; }
  bool wasPressed(uint8_t) const { return pressed; }
  bool wasAnyPressed() const { return pressed; }
  bool wasAnyReleased() const { return released; }
  unsigned long getHeldTime() const { return 900; }
};
struct HalHaptics {
  static void feedback(bool, bool, int) {}
  static void longPress(bool, int) {}
};
struct CrossPointSettings { enum { VIBRATION_OFF, VIBRATION_TOUCH_PAGE }; };
struct {
  int vibration = 0, hapticIntensity = 0, homeButtonDoubleTapTiming = 0;
  int homeButtonTapAction = 0, homeButtonDoubleTapAction = 0, homeButtonLongPressAction = 0;
  int sideLeftAction = 0, sideRightAction = 0;
} SETTINGS;
SideEvent sideReleasedEvent(int, int, bool, bool) { return {}; }
HomeButtonAction sideHomeAction(int) { return HomeButtonAction::Ignore; }
bool sideStandaloneAction(int) { return false; }
struct MappedInputManager {
  enum class Button { Back, Confirm, ScreenDown };
  enum class SwipeDir { None };
  HalGPIO& gpio;
  mutable bool stickyConfirmClickFrame = false;
  mutable HomeButtonAction homeAction = HomeButtonAction::Ignore, deferredHomeAction = HomeButtonAction::Ignore;
  mutable HomeButtonInput homeButtonInput;
  struct ReleaseGate { bool swallowsRelease(uint8_t) const { return false; } } releaseGate;
  mutable SideEvent sideEvent, deferredSideEvent;
  mutable uint16_t longPressFiredButtons = 0;
  mutable bool touchHeldOverrideValid = false;
  unsigned long touchHeldOverrideAt = 0, touchHeldOverrideMs = 0;
  static constexpr unsigned long TOUCH_HELD_OVERRIDE_WINDOW_MS = 250;
  void update(bool deferHomeButtonAction = false) const;
  bool wasPressed(Button) const;
  bool wasReleased(Button) const;
  unsigned long getHeldTime() const;
  SwipeDir wasSwipe() const { return SwipeDir::None; }
  bool isTouchscreenLocked() const { return false; }
  void settleReleaseGate() const {}
  bool isPressed(Button) const { return false; }
  bool wasBackGesture() const { return false; }
  bool mapButton(Button b, bool (HalGPIO::*fn)(uint8_t) const) const {
    return b == Button::Back && (gpio.*fn)(0);
  }
INJECT
};
''' .replace('INJECT', inject)
harness += '\n'.join(method(signature) for signature in (
    'void MappedInputManager::update(',
    'bool MappedInputManager::wasPressed(',
    'bool MappedInputManager::wasReleased(',
    'unsigned long MappedInputManager::getHeldTime(',
))
harness += r'''
int main() {
  HalGPIO gpio;
  MappedInputManager input{gpio};
  using Button = MappedInputManager::Button;
  input.update();
  assert(!input.wasPressed(Button::Confirm) && !input.wasReleased(Button::Confirm));
  input.injectConfirmClick();
  assert(input.wasPressed(Button::Confirm) && input.wasReleased(Button::Confirm));
  assert(!input.isPressed(Button::Confirm) && input.getHeldTime() == 0);
  assert(!input.wasPressed(Button::Back) && !input.wasReleased(Button::Back));
  input.update();
  assert(!input.wasPressed(Button::Confirm) && !input.wasReleased(Button::Confirm));
  assert(input.getHeldTime() == 900);
  // Native physical and home actions are still delivered independently.
  gpio.released = true;
  assert(input.wasReleased(Button::Back));
  assert(!input.wasReleased(Button::Confirm));
  input.homeAction = HomeButtonAction::Confirm;
  assert(input.wasReleased(Button::Confirm) && input.getHeldTime() == 0);
}
'''
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    cpp = directory / 'sticky_confirm.cpp'
    cpp.write_text(harness)
    executable = directory / 'sticky_confirm'
    subprocess.run(['c++', '-std=c++20', '-DFREEINK_CAP_TOUCH=0', str(cpp), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('Sticky Confirm frame regression passed')

# A USB/flash reset must reach the UI and serial test bridge on Sticky.
main_source = (ROOT / "src/main.cpp").read_text()
usb_branch = main_source.split("case HalGPIO::WakeupReason::AfterUSBPower:", 1)[1].split("case HalGPIO::WakeupReason::AfterFlash:", 1)[0]
assert "#if FREEINK_DEVICE_STICKY ||" in usb_branch
