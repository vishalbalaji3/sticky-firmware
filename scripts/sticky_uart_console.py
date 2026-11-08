# The Sticky's Serial is UART0 (STICKY.md), so it builds with ARDUINO_USB_CDC_ON_BOOT=0 and
# must drop base's =1 here. -U can't undo it: the core rebuild orders -U apart from the -D
# flags, and its -Werror components then fail on the redefinition.
Import("env")

env.Replace(BUILD_FLAGS=[f for f in env.get("BUILD_FLAGS", []) if f.strip() != "-DARDUINO_USB_CDC_ON_BOOT=1"])
