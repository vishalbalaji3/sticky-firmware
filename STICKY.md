# reTerminal Sticky: hardware notes

The Sticky build of this fork (`pio run -e sticky`, macro `FREEINK_DEVICE_STICKY`). Product spec, open
issues and the device test checklist live in the cloud repo, `vishalbalaji3/sticky-cloud`
(`SPEC.md`, `ISSUES.md`, `TESTING.md`). Read this before touching boot, power, serial, I2C or the mic.
Every item below cost a debugging session.

## Build, flash, log

```sh
git checkout sticky-os && git submodule update --init   # SDK is our fork: vishalbalaji3/freeink-sdk, branch sticky-os
.venv/bin/pio run -e sticky -t upload                    # pioarduino core 6.1.19 lives in .venv
.venv/bin/pio device monitor -b 115200 --dtr 0 --rts 0   # DTR/RTS must stay off, see GPIO0 below
```

- In the SDK submodule, `origin` is upstream Free-Ink (read only). Push to the `mine` remote.
- A fresh flash boots, then goes straight back to sleep, because the power-button wake check fails. Click the third button to wake it.
- Host tests: `cmake -S test -B build/test && cmake --build build/test -j8 && ctest --test-dir build/test -j8`. `ParserStopTest` is flaky under `-j8` only.

## Gotchas

- **USB-C is a WCH CH343 bridge on UART0 (GPIO43/44), not the S3's native USB.** The Sticky builds with `ARDUINO_USB_CDC_ON_BOOT=0`, so `Serial` is UART0.
  - With CDC on boot, `Serial` went to the unused native USB. Library logs (SecureClient, wolfSSL) vanished.
  - The native USB PHY also kept GPIO19/20, which are the mic pins.
- **GPIO0 is the sensor I2C SCL and also the CH343 auto-download line.** A serial monitor that asserts DTR pulls SCL low while USB is connected. The battery gauge, RTC and IMU then fail (`ESP_ERR_INVALID_STATE`, "RTC not found", battery "--%"). Log with DTR and RTS off.
  - `HalPowerManager::recoverGaugeBus()` clocks a stuck bus free at boot and logs `Gauge I2C bus ...`. "SCL low" there means something external holds GPIO0.
- **Sleep holds pads with `gpio_hold_en`, and holds survive wake, reset and reflashing.** A held pad silently ignores writes. `PowerManager::powerDownRailsForSleep` holds the display, SD, touch and mic (GPIO38) enables off.
  - Anything that powers one of those back on must call `gpio_hold_dis` first. Missing this kept the mic unpowered: every recording was a flat -30935.
- **PDM mic: CLK GPIO19, DATA GPIO20, enable GPIO38 (active high), left slot, I2S0.**
  - `freeink::Microphone::begin` releases the enable hold and detaches the USB PHY pads (`USB_SERIAL_JTAG_USB_PAD_ENABLE`) before starting I2S.
  - A constant PCM value means no PDM bitstream, which points at power, pins or the PHY.
- **TLS (wolfSSL, pinned roots in `src/voice/TrustRoots.h`).** Convex (`*.convex.site`) chains to GTS Root R4 through WE1, which is ecdsa-with-SHA384 on P-384.
  - It needs `WOLFSSL_SHA384`, `WOLFSSL_SHA512` and `WOLFSSL_SP_384`. Without SHA-384 the handshake fails with `HASH_TYPE_E` (-232).
  - Requests use HTTP/1.0 to avoid chunked bodies.
- **SD and the e-paper share one SPI bus (SCK13/MOSI14).** Voice capture goes through a PSRAM ring on core 0, so display refreshes don't drop samples. 0 dropped has been verified.
- **Buttons: Up GPIO5, Down GPIO6, third button GPIO4 (power, AI and confirm in one).**
  - A click goes home, and wakes from sleep. Holding records. Up+Down locks.
  - `shortPwrBtn` is forced to IGNORE on the Sticky, so the stock ghost-wake hold check is skipped there.
- **Deep-sleep wake is a full reboot.** Anything that must react to a held button (hold-to-record from sleep) has to act early in `setup()`.
- **Battery gauge BQ27220 0x55 on Wire1 (SDA1/SCL0), shared with PCF8563 RTC 0x51, SHT40 0x44, LSM6DS3 0x6A.** Touch (GT911) is on its own bus (SDA3/SCL2).

## Sticky-only code

These are new files where possible:
- Voice notes: `src/voice/`, `src/activities/voice/`.
- TV remote: `src/activities/remote/`.
- Automatic reading sync: `src/activities/reader/AutoProgressSync.*` and `AutoSyncPolicy.h`.
- Picture viewer: `src/activities/reader/PictureViewerActivity.*` and `PictureViewport.h`.
- Sleep and lock: `SleepWakePolicy.h`, `SleepActivity::drawAsleepMarker`.

On-card config is `/.sticky/config.json` (`host`, device `token`, `tvHost`). `/.sticky/` is hidden from the web file manager.
