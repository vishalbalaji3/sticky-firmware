# CrossNext migration: first control increment

Date: 2026-11-14. This is an implemented preparation increment on sticky-os,
not a completed CrossNext base replacement. No flash, deployment, service
configuration change, commit, push, or PR was performed.

## Baseline and upstream inspection

- Firmware HEAD: e8afb8e13f475934da1a42c3ed6c8b8581160b4b plus the parent's
  uncommitted, installed revision-sync implementation and documentation.
- Cloud HEAD: ad719087f0bdc7d280e2a07ec7304ac62a447642 plus the parent's
  uncommitted revision service. No cloud file changed in this increment.
- Local SDK gitlink and checkout: fd26de8367c0b59d5f52c80290dc339331380399.
- CrossNext origin/xteink and inspected checkout, fetched again:
  e1174b339e9d4e0cc260c932c493e9fb1872bb44 (v1.4.0).
- CrossNext SDK is tracked tree 91736459f8fe7030da0be2b5dd64765df0aedd05,
  not a submodule commit. The research checkout is shallow and sparse;
  no common-history or mergeability claim is made.
- Both local branches match their origin tips (0 ahead, 0 behind). Dirty
  changes were explicitly authorized as the baseline; neither repo was pulled
  over them. Both skillset checks returned ok. The two linked Dell inventories
  had no running Sticky thread; their latest Sticky activity was November 10.

Tracked-file comparison against the local working tree during this increment:
src has 285 common paths, 192 different, 62 local-only, 799 CrossNext-only;
lib has 322 common paths, 166 different, 44 local-only, 60 CrossNext-only.
The SDK comparison has 888 common files, 146 different, 153 local-only,
2 CrossNext-only. These are file counts, not an estimate of port effort.
The six major seams differ substantially: main.cpp (+625/-456 upstream
relative to local), EpubReaderActivity (+926/-708), KOReaderSyncActivity
(+108/-142), MappedInputManager.cpp (+149/-76), KOReaderSyncClient.cpp
(+7/-37), and platformio.ini (+184/-447).

Reversible pre-edit staged/unstaged patches and SHA-256 file manifest are in
/tmp/sticky-crossnext-increment-baseline/. They include the existing sync work.
The manifest verifies no pre-existing files changed except STICKY.md,
the migration research document, main.cpp, MappedInputManager.h,
test/CMakeLists.txt, and test/test_remote_button_input.py.
New files are this report, StickyButtonInput.h, and its host-test directory.
The parent's sync implementation, SDK, translations, and cloud remain intact.

## Implemented behavior and ownership

Third-button short release now produces Confirm/Menu, including on Home.
Previously it injected Back away from Home and did nothing on Home. Existing
Home selection, reader menu opening, and menu selection consume Confirm;
Up/Down keep their existing navigation and page-turn mappings. Touch Back
and the picture viewer's X provide return navigation.

StickyButtonInput owns only two boolean gesture latches and returns None,
Confirm, or Record. It adds no allocation, task, setting, persistent record,
or backend request. main.cpp still owns microphone start/pump/stop and passage
capture, with the same 500 ms awake threshold. The shared SDK button is still
configured to emit Power so a held contact cannot leak native Confirm.
MappedInputManager uses the existing one-frame remote-input storage to deliver
the accepted short release, with held time zero and no held Confirm state.

Wake click is consumed. Early-boot capture consumes its release. A failed or
full recording cannot restart while the same contact stays held, or activate
a menu on release. A long release first observed after a blocked loop is not
treated as a click. File-transfer holds remain unavailable for capture and
are consumed rather than turning into menu selections. Up+Down lock and the
early-wake recording/anchor code remain unchanged.

This is the smallest useful control seam to carry into CrossNext's main loop.
It establishes the accepted click/hold split without mixing the much larger
reader and sync port into the review.

## Concrete controls still needing attention

- CrossNext main.cpp's touch PowerMenu policy must not also own Sticky's hold.
  Integrate this voice gesture owner in place of that policy for Sticky,
  preserving normal power behavior on other boards.
- Both trees handle Power+Down screenshots before voice handling. That chord
  can defer pumping/stopping a recording. Do not silently redefine hold-record:
  before the base port, decide whether Sticky should retain that screenshot
  chord or reserve the third button exclusively while recording.
- Native long-Confirm reader shortcuts are unavailable on Sticky because hold
  records. Required actions must remain reachable by an on-screen menu. Audit
  CrossNext's actual menus, including chapter/library return, after porting.
- This increment has source and host evidence for navigation; it does not
  prove every upstream app can be operated with Sticky's two navigation keys.

## Working sync and custom features that must survive

Keep Readest on iPhone and the current Convex endpoints/settings. The owner
verified backward handoff 72% → 2%, the reverse direction, and Hardcover Read.
The 40-byte sync-state.bin must remain bound to server/account/document and
the exact progress bytes. Preserve intentional-navigation dirty tracking,
conditional expected_revision writes, conflict handling, unknown-ancestry
explicit choice, cache preservation, and immediate pulls on reopening.
No furthest-position rule or 120-second pull cooldown may return.
Readest 0.12.20 can write unconditionally while Ask is pending; the owner
accepted protection for unsent Sticky edits, not a guarantee for both orders.

CrossNext's ReadingProgressSync has its own fetch/upload paths, and its manual
KOReaderSyncActivity compares positions. Neither may bypass revision handling.
Disable or adapt all-book progress sync before enabling it against this backend;
do not simply copy the client and assume every caller uses it.

Retain src/voice and activities/voice, reader and selection fillVoicePlace
hooks, wake-to-record, SD-first offline WAV/metadata, upload/idempotency,
transcription and transcript UI together. Keep custom Japanese ratings/cards,
Convex OPDS and cached home/weather APIs and the sole existing Hardcover writer.
Native CrossNext Notes and FSRS are separate models, not compatible replacements.
Prefer native launcher, menu, renderer, and ordinary reader features where their
contracts allow these custom services to plug in.

## Next reviewable increments

1. Settle SDK ownership before importing the base: recommended path is the
   CrossNext bundled SDK plus an explicit minimal Sticky hardware patch set.
   Alternative is updating the SDK fork/gitlink to an equivalent compatible
   tree, which adds another repository to maintain. Do not overlay the old SDK
   wholesale: 146 common SDK files differ. Retain GPIO hold release, USB mic-pad
   release, I2S0/left-slot capture, sensor-bus recovery, sleep rail handling,
   UART logging/CDC-off and TLS flags. Drop the development six-hour timer wake.
   Produce an isolated, non-flash build candidate and verify both targets.
2. Port the voice/anchor/sleep lifecycle and this input seam to that candidate.
   Integrate Notes/Japanese launcher entries with native CrossNext navigation;
   keep complete services rather than copying just their screens.
3. Port the installed sync baseline as one coherent unit: client, reconciliation,
   companion storage, reader dirty intent, manual/automatic sync and cache paths.
   Adapt or gate upstream bulk sync. Carry and run storage/reconciliation tests,
   then repeat the user's Readest rewind/conflict/Hardcover device matrix.
4. Complete the four review axes and owner handoff; test cold boot, wake,
   offline/online recording and transcript recovery, picture and menu controls
   in all four orientations, memory headroom and repeated sleep before flashing
   a migration build. Retarget or disable upstream OTA before distribution.

The migration direction and click/hold/lock behavior are already accepted.
Remaining owner choices concern SDK maintenance and the screenshot chord,
not whether to replace Readest or reconfigure the working backend.

## Verification and handoff

Host checks: six new StickyButtonInput tests; 34 selected CTest cases for input,
sleep, revision storage/reconciliation and Wi-Fi policy; 15 voice-format/ring/
notes tests; four existing navigation integration checks (one overlaps the
34-case set). All pass. The old build/test cache referenced the previous repo
path, so tests used /tmp/sticky-crossnext-host-tests without altering that cache.
Logs: /tmp/sticky-crossnext-host-{config,build,test}.log,
/tmp/sticky-crossnext-voice-test.log, and
/tmp/sticky-crossnext-navigation-test.log.

Formatting wrapper and git diff --check passed. Sticky build passed:
/tmp/sticky-crossnext-sticky-build.log. Static RAM 79,100 bytes, image 5,424,731
bytes; these are linker sizes, not runtime headroom measurements.
Default C3 build also passed: /tmp/sticky-crossnext-c3-build.log.
Static RAM 66,632 bytes, image 5,675,081 bytes.

Handoff is incomplete. The implementation-child brief forbids subagents;
the parent's four independent firmware-handoff reviews and explicit design/
maintenance acceptance still apply to this new increment. The prior sync
reviews do not cover it. No readiness or hardware-verification claim is made.
After those reviews, the owner should verify: click chooses/opens a book,
Up/Down turn pages and navigate open menus, click confirms exactly once,
touch Back returns, awake and wake holds record without a release action,
offline notes later upload with correct anchors, a full/failed recording does
not restart, and Up+Down locks. Repeat navigation in all four orientations.
