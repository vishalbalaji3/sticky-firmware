# CrossNext Sticky port

Base: manuelx900/CrossNext `e1174b339e9d4e0cc260c932c493e9fb1872bb44` (1.4.0).
This is a source-level port: the repositories have no common Git ancestor. Native runtime
source and build tooling come from that pinned tree. SDK source follows its bundled tree
`91736459f8fe7030da0be2b5dd64765df0aedd05`, kept in the existing Sticky SDK fork/submodule.

## Preserved custom behavior

- AI Notes and Japanese are native Apps entries, with Home shortcut support. Native text Notes
  stays separate from the voice-transcription service. Capture remains SD-first, works offline,
  retains passage/selection anchors, and starts during wake when the button is held.
- Third-button click confirms or opens a menu, hold records, and Up+Down locks. Sticky's
  competing screenshot chord and power-menu hold are removed. The native list reader menu
  is selected on Sticky because its controls work with the physical buttons.
- Readest and the custom Convex KOSync service retain revision/dirty-state reconciliation,
  backward handoffs, unknown-state choice, cache-state preservation and completed-Hardcover guards.
  Native bulk progress sync is unavailable on Sticky until it supports the same ancestry contract.
  Readest 0.12.20's unconditional-write conflict gap is explicitly accepted by the owner.
- Sticky UART logging, microphone power-pad/USB release, I2S0/left channel, gauge recovery,
  TLS hostname verification and SHA384/P384 support remain. Six-hour development wake is removed.
- Existing wallpaper/book-page sleep behavior, image viewer and protected /.sticky paths remain.
- Upstream OTA is disabled for Sticky; use an explicit USB/SD update built for this fork.

## Integration choices

Native CrossNext launcher, apps, renderer, settings and navigation replace their older equivalents.
Obsolete local LCP/OPDS/Bluetooth implementation units were removed where native contracts changed;
custom cloud OPDS and saved-book opening remain. No new AI functionality or new sync provider was added.
The CrossNext 16 MB partition layout retains NVS and two app slots, expanding each app slot to
0x7f0000; it replaces unused SPIFFS space, while books and settings remain on the SD card.

## Verification

Host checks cover sync state, controls, navigation intent, wallpaper bitmap row/cache handling,
and SDK battery/input/TLS behavior. Full firmware builds, final Sol review and device checks are
recorded in the shipping handoff; a compile is not a claim that every native app has been exercised.

## Shipping evidence — 2026-11-14

- Sticky and C3 release builds passed; 614 CTest checks, native navigation/input checks,
  SDK hardware-policy checks, and 91 cloud tests passed. Cloud typecheck/PWA build passed.
- Sol approved the port and the USB-boot correction in two review rounds. The stale
  fresh-flash documentation was corrected; existing English-only sync status text is deferred.
- USB flash was hash-verified. The real device reports `sticky 1.4.0-sticky.1`, loads its
  saved settings, renders Home/Library and Strange Pictures, turns pages and opens the
  native list menu using button input. Reader grayscale refresh completed in the device log.
- Holding the third button saved a 39,936-byte audio payload with zero dropped samples
  and returned to the reader. This verifies capture, not a new spoken-transcription trial.
- The custom production KOSync endpoint returned HTTP 200 with revision 8 and the device's
  74.9709% position after the reader exit. No hosted CrossPoint account is required.
- Optional diagnostic reporting is off on the physical device. Official 1.4.0 resource
  installation and final commit/artifact details are recorded in the shipping handoff.

Not exhaustive: every native game, all display orientations, wake-time audio and a new
spoken transcription have not been exercised during this device pass. The accepted
Readest unconditional-upload gap remains; completed-Hardcover protection is unchanged.

Suggested firmware Ship section: integrate directly on `sticky-os`, push the SDK fork
first, run host checks and Sticky/C3 builds, flash Sticky over MacBook USB, and verify
boot/version plus reading, controls and voice capture on the physical device.

### Published artifacts

- Firmware implementation: `e300705b40da6db53b4ebd5be4ba019abe036f2c`. Its tree is
  identical to the reviewed source snapshot; upstream source assets were published in
  bounded batches through Dell because large MacBook uploads disconnected.
- SDK: `9edea7c932306febc00e781b8ab58dbc9c8a8884`; cloud: `c63395903568d682609105cb36ea0a5bca1ea811`.
- Final installed firmware: 7,728,096 bytes, SHA256
  `1841768cace900271678b011ec5bedf947d6562020b1fcf916afb45089784212`.
  USB write/hash verification succeeded; device PING reports `sticky 1.4.0-sticky.1`.
- All 126 files from the official 1.4.0 resource pack are installed. Existing settings,
  books and Noto Serif Japanese font survived. Apps shows AI Notes and Japanese first.
- Production cloud deployment is Ready at
  https://sticky-cloud-ginfgc1ou-vishalbalaji3s-projects.vercel.app and the public site
  https://sticky-cloud.vercel.app returns HTTP 200.

Final smoke test: AI Notes displayed the cached list and opened a transcription with
third-button Confirm. Japanese displayed its 45-card deck and completed a TLS 1.3
backend handshake. Device was returned to Home; the USB test bridge was closed.
