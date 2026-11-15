# CrossNext / CrossPoint Sync migration research

> **Current decision (2026-11-14, after the hosted trial):** retain Readest on iPhone
> and the custom Convex revision/conditional-write service. The owner verified
> Readest → Sticky backward handoff (72% → 2%), Sticky → Readest, and Hardcover
> remaining Read with the installed custom build. Hosted forward-only progress
> failed rewinds. Older options and trial instructions below are historical,
> not instructions to reconfigure the working services. The first local control
> migration and remaining base-port work are recorded in
> [crossnext-increment-1.md](crossnext-increment-1.md).

Research date: **2026-11-14**. Source inspection only; no builds, device tests, account changes, or implementation changes. The parent supplied the clean/current repository and other-machine checks. Upstream sources were inspected in temporary checkouts. This report is the only project file written.

## Findings

**CrossNext is a plausible source-level base, not a verified drop-in Sticky firmware. CrossPoint Sync is a reading-state service and companion app, not an established replacement for Readest's phone reading experience or Sticky's custom backend.** Retaining AI voice notes requires retaining or porting the capture, anchoring, upload, transcription, and transcript UI together.

- CrossNext advertises/releases **X4 Pro only**, but its source retains an inherited `sticky` build environment and a FreeInk Sticky board profile. This substantially reduces board bring-up work, without proving that current CrossNext applications work on Sticky. [N1][N2][N3]
- Its microphone driver lacks the local fixes for held power pads and USB ownership of mic pins. Its inherited base enables USB CDC, unlike the local Sticky override. These are concrete differences behind previously observed local failures, not hypothetical feature gaps. [N2][N4][L1][L2]
- Sync supports **clippings, annotation text, bookmarks, rich positions and statistics**, beyond basic KOSync. However, its reader-format clipping response deliberately excludes annotation `note` text; CrossNext requests that format. A transcript stored as an annotation will not automatically appear on the reader. [S2][N5]
- The inspected companion app has library/statistics/clipping screens and native OPDS download/device-transfer flows. No EPUB reading screen was found in its routing or dependencies inspected. Replacing Readest for **phone reading** remains an unmet requirement unless another reader is retained. [S3][S4]
- No equivalent of Sticky's WAV ingestion/transcription, `nihongo-cards` proxy, or weather/home API was found in Sync's registered routes. A configurable firmware sync URL does not give users the ability to install custom endpoints on the operator's hosted server. [S5][L3]
- CrossNext's own Notes model is text/checklists, with file-based persistence; it is not the Sticky voice-note pipeline. [N12]

## Inspected revisions and evidence limits

| Component | Inspected revision |
|---|---|
| Local firmware, `sticky-os` | `e8afb8e13f475934da1a42c3ed6c8b8581160b4b` |
| Local cloud, `main` | `ad719087f0bdc7d280e2a07ec7304ac62a447642` |
| Local FreeInk SDK | `fd26de8367c0b59d5f52c80290dc339331380399` |
| CrossNext, default `xteink` branch | [`e1174b339e9d4e0cc260c932c493e9fb1872bb44`](https://github.com/manuelx900/CrossNext/commit/e1174b339e9d4e0cc260c932c493e9fb1872bb44), commit dated October 31 |
| CrossNext bundled SDK directory | Git tree `91736459f8fe7030da0be2b5dd64765df0aedd05` at the above commit; **a tracked tree, not a gitlink**. `.gitmodules` still names `ma-r-s/freeink-sdk`, branch `fix/sd-free-space`; that does not establish an SDK commit SHA. [N6] |
| CrossPoint Sync, `main` | [`a0dde1dff028372b9d9c579551091c981de2ad3d`](https://github.com/crosspoint-reader/crosspoint-sync/commit/a0dde1dff028372b9d9c579551091c981de2ad3d), commit dated November 14 |
| Hosted service | Public [`GET /healthz`](https://sync.crosspointreader.com/healthz) returned `{"status":"ok","version":"0.2.0"}`. This does **not** identify its deployed Git SHA or prove authenticated endpoint behavior. |

CrossNext describes its ancestry as CrossPlay 1.14.2 and CrossPoint 1.6.5 with FreeInk. Treat this as upstream's attribution, not proof of a clean merge base with the local fork. Its repository presents a source export with bundled SDK sources. No history-wide mergeability analysis was performed. [N1][N6]

## Actual Sticky feature inventory

“Implemented” below means present in the inspected source. It does not mean newly verified on hardware. Local SPEC/ISSUES contain overlapping, stale status claims: SPEC reports some device tests, while ISSUES still lists several as unverified. Preserve that uncertainty rather than treating either checklist as comprehensive. [L4][L5]

| Feature | Current implementation and limits | Migration consequence |
|---|---|---|
| Offline voice capture | Third-button hold; early capture after deep-sleep wake; SD-first WAV and metadata; PSRAM ring and core-0 capture task; interrupted-file recovery. 16 kHz mono 16-bit, firmware stops at 12 minutes. Sub-second recordings discarded. [L6][L7][L8] | Port application lifecycle and hardware fixes, not just the microphone API. |
| Sentence/page/inbox notes | Reader supplies title, author, spine and page; text selection adds quote and source offsets and saves a normal clipping. Outside reading, inbox. [L9][L10][L11] | Preserve note kind and capture identity; page numbers alone are not stable anchors. |
| AI processing | Two-step metadata/audio upload to Convex; Groq-compatible STT default `whisper-large-v3-turbo`; optional separately configurable cleanup. Successful processing deletes cloud audio; failed processing keeps it for retry; empty/non-alphanumeric transcripts are removed. SD sent audio has 30-day retention logic. [L3][L7][L12][L13][L14] | Requires this backend or a replacement worker/storage/API. No on-device Whisper or question-answering is implemented. Provider retention settings are operational, not verified by code. |
| Notes on device and PWA | Notes reads cached transcripts, syncs on open and rechecks pending work; PWA supports edit/delete/retry and Markdown export. Sentence highlights open clippings, **not transcripts**; page-note markers are not implemented. [L1][L13][L15] | CrossNext text Notes is a different application. Retain transcript UI and pending/error/offline states. |
| Reading sync | Custom automatic pull on opening and guarded push on exit, saved-network selection, skip push after failed remote read, preserve further remote position. Manual sync permits intentional replacement. Convex also implements KOSync. [L16][L3][L1] | Stock CrossNext's manual/smart and all-book sync are not proof of identical open/exit behavior. Preserve policy deliberately. |
| EPUB library | PWA uploads EPUBs; Convex stores original bytes and partial MD5; authenticated OPDS acquisition with storage redirect. Firmware Library opens catalog/local copy. [L17][L3][L18] | Sync's metadata library and native OPDS browser do not replace EPUB storage/catalog hosting. Keep an acquisition source. |
| Hardcover | Backend ISBN/title-author matching and manual override; progress push throttled to five minutes/1% change, finish at 99%. Implementation explicitly notes API assumptions; hardware/end-to-end verification remains incomplete. [L19][L20][L5] | Sync has its own connector, but matching, finish, privacy and read-session behavior require comparison. Avoid two independent writers. |
| Japanese | Offline due-card batch, furigana, reveal/rating controls, durable timestamped ratings, sync on open/exit through Convex to `nihongo-cards`. External scheduler is source of truth; earliest-rating policy is specified there, not independently audited here. [L21][L3][L4] | CrossNext Anki/FSRS does not implement this service contract; retain or adapt it. |
| Lock/sleep | Reader timeout preserves page with moon; explicit lock/non-reader sleep draws dashboard. Classic/Photo wallpaper, date via RTC, weather and due-card count from cached home data. PWA now has wallpaper upload/crop/tone/presets and detail toggles. [L1][L22][L23][L24] | Retain sleep/wake policy and dashboard API. Upstream wallpapers are not equivalent. |
| Other custom behavior | Continue Reading/Library/Notes/Japanese launcher, third-button Back/hold-record, Up+Down lock; hold-picture viewer with zoom/pan; password-protected hotspot; batched downloads. [L1][L18][L25] | Carry these seams forward and verify touch/orientation and SPI concurrency. |
| Inherited reader features | Settings/source retain OPDS, Calibre/file transfer and Bluetooth page-turner integration; dictionaries/fonts and normal reading remain part of the base. [L26][L4] | Regression scope, not newly provided by a migration. Disable/retarget updater before distributing a custom build. |

**Planned or incomplete:** clock while asleep, scheduled wakes/standby modes, auto-rotation, transcript popovers at selections, page-note markers. “Wallpaper upload not built” in SPEC is stale against current PWA source. TV remote is explicitly removed, not a feature to preserve. “Ask about this passage,” local Whisper and spoken sentence references are out of scope. [L4][L5][L23]

## Hardware and integration cost

CrossNext's board profile includes Sticky's 800×480 touch display, GT911, mic enable/clock/data, gauge and sensors. Its build enables S3 PSRAM and a 16 KiB render-task stack. Thus this is **an inherited hardware target requiring reconciliation**, rather than a new board from scratch. Source support and a successful compilation would still not establish panel refresh, touch hit targets, memory headroom or reliable sleep. [N2][N3]

The local fork has important divergences to preserve: CH343/UART logging with CDC disabled; sensor I2C GPIO0/DTR interactions and recovery; GPIO19/20 USB-pad release; GPIO38 hold release; mic slot/channel configuration; shared display/SD SPI buffering; early-boot recording and restoring the reading context. CrossNext's inspected `Microphone::begin` powers the mic without those local hold/pad fixes and requests `I2S_NUM_AUTO`. [L1][L2][N4]

**Relative effort, inferred from these seams:** hardware reconciliation is bounded but high-risk; voice/selection/lifecycle porting and sleep policy are substantial; pure backend reuse is comparatively contained if the existing bearer-token API remains intact. CrossNext's extra apps/themes introduce a broader memory and input regression surface. No credible person-day estimate or claim of greater polish can be made from this inspection. Its development Sticky environment also enables a six-hour timer-wake macro; do not inherit this unnoticed while local scheduled wakes remain deliberately deferred. [N2][L4]

## Note identity and cross-reader compatibility

There are separate identities today:

1. **Voice book ID:** FNV-1a-derived `b…` value from title and author. Stable under file moves, but metadata changes change it; different editions with identical title/author can collide semantically. [L7]
2. **KOSync document:** partial-binary MD5 of original EPUB bytes (or filename hash when that setting is chosen). CrossNext defaults to filename matching, so configure the same method explicitly. [N7][L17]
3. **Position:** Sticky voice metadata stores spine/page plus optional selected-text offsets and quote; it does not store a complete EPUB CFI or KOSync document ID. A page-only note is layout-dependent. [L9][L10][L11]

Sync clippings accept chapter-visible Unicode-codepoint offsets, quote, paragraph and layout hints. That is a useful mapping target, but matching field names does not prove the two EPUB parsers count hidden text, ruby, whitespace or entities identically. CrossNext's clipping synchronization includes offsets; test exact text after font, orientation and edition changes. Its rich-progress path is capability-gated separately from clippings. [S2][N5][N7][N8]

An adapter could publish a **sentence note's transcript** into a clipping's annotation field, with a stable mapping from Sticky `clientId` to document/clipping ID. It must handle the 4096-byte annotation limit, later transcript edits, tombstones and repeat uploads. Page/inbox notes lack a natural quote-backed clipping mapping; retain them as notes unless an explicit representation is chosen. CrossNext downloads `format=reader`, which excludes annotation text, so a device transcript UI still needs its own path. These are proposed mechanisms, not current integration. [S2][L11][N5]

Do not optimize/repackage EPUBs during the initial trial: Sync's native app can transform images/covers/fonts before device transfer. That can alter binary identity and content anchoring while the phone keeps the original. Start with byte-identical EPUBs and record both note ID and document hash. [S3][L17]

## Stock capability versus hosted-service boundary

| Area | Verified source capability | Constraint / unknown |
|---|---|---|
| Endpoint configuration | CrossNext supports custom KOSync server URL and server-type flags. [N7][N8] | This changes the reading-sync endpoint, not Sticky's bearer-token notes API. Hosted accounts must be configured separately; existing Convex data does not automatically migrate. |
| Annotations | Sync has authenticated clipping GET/PUT, deletion revisions and text notes; CrossNext probes sync version 2 and opt-in clippings. [S2][N5] | API compatibility is source-verified, not tested against the hosted account. Annotation support is not audio ingestion/transcription. |
| Progress resolution | Server stores per-device progress; rich API and KOSync semantics differ in timestamp handling. CrossNext all-book sync compares positions. [S6][N9] | Newest server write and furthest-reading policy are distinct. Unique device IDs, offline stale writes and intentional rewinds require explicit tests. |
| Mobile library | Native app downloads OPDS and transfers over reader Wi-Fi; web routing omits Browse/Send. [S3][S4] | No inspected ebook renderer. A library item is not evidence that the service hosts the EPUB or can display it for reading. |
| Hardcover | Dedicated server connector and credential flow exist. [S7] | Account matching, edition choice, progress/status and failure recovery are untested here. Current Convex push would no longer see new progress if devices only update hosted Sync. |
| Japanese | CrossNext has local FSRS-5 and Anki imports, plus a separate study bridge protocol. [N10][N11] | Not `nihongo-cards`; no existing due/review API compatibility found. Local scheduling could fork review history. |
| Sleep screens | Sync can render reading-stat sleep images and store a background; CrossNext has wallpaper facilities. [S8][N1] | Does not implement Sticky's weather/cards/date dashboard or its wake-to-record policy. |
| Hosting/customization | Open-source Node/SQLite service supports self-hosting. [S9] | No published hosted mechanism found for adding private endpoints or transcription jobs. A separate backend or self-hosted fork is required for custom server code. Hosted retention, quotas, backups/SLA and exact deployed revision were not established. |

The existing voice host/token and KOSync server are separate configurations. Therefore testing hosted reading sync does **not inherently require replacing Convex voice/Japanese/library services**. It does require deciding who owns reading progress and Hardcover updates, plus whether/how the PWA continues showing reading state. [L1][L3][L16]

## Migration options — no architectural decision

1. **Keep current firmware/backend; trial hosted Sync for reading state.** Smallest way to test account, annotation and mobile-companion value. Retain Readest or another reader for phone reading. Plan for the existing PWA/Hardcover reading data becoming stale unless bridged.
2. **Port Sticky custom functionality onto CrossNext, retain Convex custom APIs.** Gains CrossNext applications/navigation while preserving voice and Japanese services. Requires SDK reconciliation, launcher/input integration, anchors, sleep and resource testing; existing backend endpoints can remain unchanged.
3. **Keep Sticky firmware and selectively adopt CrossNext UI/apps.** Limits board/voice disruption, but each imported feature still brings source/API/resource dependencies. Compare this maintenance burden with a full base switch using a small representative feature.
4. **Self-host/fork Sync and consolidate custom services.** Gives endpoint ownership, but replacing Convex also requires audio storage/jobs, auth/device revocation, provider configuration, library acquisition, Japanese proxy, dashboard data and PWA migration. This is a backend rewrite, not merely changing a URL.

These options can be evaluated independently: firmware base, reading-sync server, custom backend and phone reader are four separate choices.

## Practical proof-of-concept acceptance criteria

No flashing or implementation is part of this research. A later authorized trial should pass the following before treating a migration as feature-preserving:

1. **Baseline and rollback:** archive exact firmware/SDK/config and SD contents; retain original EPUB bytes and note records. Establish clean builds for selected targets and prevent upstream OTA replacing the custom build.
2. **Sticky bring-up:** display and SD, all orientations/touch hit regions, physical buttons, gauge/RTC, power rails and serial logs work after cold boot and repeated deep-sleep wakes. Measure sleep current against the current firmware; prove no unintended periodic wake.
3. **Voice:** record from an open book, selection, inbox and sleep; verify first words, duration/sample format and zero dropped samples during display/SD load. Exercise short/silent notes, maximum duration, interrupted recording, offline queue and retry without duplicate notes.
4. **AI round trip:** metadata/audio idempotency, transcription success/failure/retry, optional cleanup failure, cloud-audio deletion after success, retained local WAV and transcript visibility on device/PWA. Test mixed English/Japanese. Prove provider credentials remain server-side.
5. **Anchors/identity:** same EPUB and matching hash method on every client; verify quoted range after font/orientation changes, Japanese ruby/entities and repeated passages. Explicitly test a changed EPUB edition. Preserve page/inbox notes without inventing false highlights.
6. **Hosted sync:** authenticate with a test account; verify progress and actual annotation payloads, deletion cursor/tombstones and long transcript limits. Test two unique devices, stale offline push, deliberate rewind and network failures. Demonstrate transcript retrieval separately from the reader-format clipping feed.
7. **Phone reading:** actually open and read an EPUB and resume the exact intended passage in both directions. If using only the inspected Sync companion cannot do this, retain a real reader or record the requirement as failed.
8. **Library/Hardcover:** download an existing and a large EPUB without duplicate local copies or changed hashes; validate ISBN/title/manual links, progress, finish and reread behavior, with only one Hardcover writer.
9. **Japanese/dashboard:** preserve due cards, furigana and offline reviews through repeated/reordered uploads; verify external scheduler state and due count. Preserve Classic/Photo wallpaper/presets, weather/date and offline cached screen, plus reader-page sleep and hold-to-record wake.
10. **Full interaction regression:** Back/hold/lock behavior, picture hold/zoom/pan, protected hotspot, saved-network selection, Calibre/OPDS, dictionary/font changes and BLE page turning. Measure memory and download speed on hardware; do not infer them from screenshots or simulator demos.

## Follow-up: settled requirements and completed-book protection

Follow-up inspection: **2026-11-14**, at the same upstream revisions above. This section refines the earlier options; it does not remove the original investigation. Tests below were **read, not run**. No authenticated hosted account, phone reader, or device integration was exercised.

### Confirmed requirements

- Preserve the existing AI voice-note workflow. Prefer stock CrossNext/CrossPoint Sync features where they satisfy the requirement; retain Sticky's custom backend for gaps.
- Readest is not an acceptable proposed phone solution. A real phone EPUB reader with progress sync is required; the Sync companion alone does not satisfy this.
- Sync behavior and physical-button ergonomics are reasons for considering the switch, not incidental features.
- **Hard requirement:** once Hardcover marks a book Read, opening/reading it on **any client linked to CrossPoint Sync** must not restart its progress, change its finished status, or automatically create a reread. Intentional reread semantics remain undecided.
- The current project explicitly names **Readest on iPhone** in `cloud/SPEC.md:30`. Treat iPhone as the strongest existing context, but confirm current phone platform/OS before selecting a reader. Android-only software is not an iPhone replacement. [F1]

### Exact protection in Sticky today

There are two guards, and neither is a device-specific Readest special case:

1. `reading.ts:79–91`: `maybeSchedulePush` returns when `pushedPercentage >= FINISHED`, where `FINISHED = 0.99`. This latches completion **per reading/document row**, regardless of the submitting client's name. The normal save still overwrites its KOSync position with the latest write.
2. `hardcover.ts:49–58`: before changing shelf/read records, `push` queries Hardcover's current `user_books.status_id`. If it is `3` (Read) **and incoming progress is below 99%**, it records `pushedPercentage: 1` locally and returns without a Hardcover mutation. Thus a paper book already marked Read is protected on its first ordinary digital opening, and later page turns do not even query Hardcover again. [F2][F3]

The finish path is deliberately different: at ≥99%, Read + **no open read** skips insertion/update; Read + **an open read** can update/close that read. This supports recovery after a partly applied finish. Therefore “current code never changes any already-Read record” is too strong. Manual linking clears the completion latch; another document hash has another latch. The normal push rechecks remote status, but the action itself has no unconditional early return on `pushedPercentage`, and external status changes can race its query/write sequence. This is an existing useful policy, not a proof of the new absolute invariant. [F2][F3]

`reading.test.ts` covers: finishing once; subsequent 100% pushes doing nothing; reading a finished book again at 5% from a different named client while leaving Hardcover alone; an already-Read Hardcover book producing only the `me` query; six failure/lost-response finish-recovery cases; and matching/manual-link behavior. The rereading test explicitly asserts that **local KOSync progress does change to 5%**. These are mocked Convex/GraphQL tests, not live API verification. [F4]

### Stock Sync connector: the guard already exists

At Sync `a0dde1d…`, `src/connectors/hardcover.ts:467–470` contains an **unconditional** `if (currentStatus === STATUS_READ) return { ok: true };` after fetching current Hardcover shelf/read context and before **all progress/status/read mutations**. It covers both low-progress events and finished events, even if an open read exists. No reread override is implemented there. Highlights branch off earlier to journal-quote publishing; leave that optional feature off if “leave the book alone” includes journal entries. [F5]

This guard is central server behavior, **not a per-device setting or an account checkbox**. Every progress event sent through this connector gets it. The connector options are only highlight publishing and its privacy; `capabilities.read` is false, so the connector does not import Hardcover's finished shelf into Sync's own library. A current-Read check is not an “ever completed” latch: if another writer changes Hardcover back to Reading, this guard no longer applies. [F5]

The source test `test/connectors-more.test.ts:640–657` asserts an already-Read book receiving a finished event makes no `InsRead`, `UpdRead`, or `UpdStatus` call. The low-progress case follows from the unconditional code, but that particular test uses 100%; do not describe it as a complete cross-client acceptance test. [F6]

### Progress, import and background path audit

| Entry path in inspected Sync source | What happens before Hardcover | Completed-book consequence |
|---|---|---|
| Standard `PUT /syncs/progress` | `writeDeviceProgress` canonicalizes identity, rejects percentage lower than current Sync progress, stores accepted writes, then fans out. | Independent forward-only device protection; accepted events still reach the central Hardcover guard. It knows Sync progress, not the Hardcover shelf. [F7] |
| `PUT /api/v1/progress/batch` | Uses the same device-write helper and only fans out stored items. | Same behavior for bulk/all-book device sync. [F8] |
| Single `PUT /api/v1/progress` | Direct upsert with client timestamp, then fan-out; deliberately permits moving backward. | Can rewind Sync; does **not** bypass Hardcover's guard. [F8] |
| Connector fan-in, including phone-provider positions | `pollConnector` stores imported progress as a provider device and fans out to **other** connectors. Exact positions are timestamp ordered; percentage-only providers can use sampled positions. | BookFusion/BookOrbit/etc. arrivals reach Hardcover's same queue/guard. Some imports can lower Sync progress; no global Hardcover-finished admission check exists here. [F9] |
| Progress GET refresh | Both progress APIs can pull provider state before returning the position; BookFusion sidecar matches are strict, other listed pullers best effort. | A read request can indirectly produce a Hardcover event through fan-in. Still guarded. [F10] |
| Background workers/initial provider sync | Fan-in workers and provider-specific polls call the same poll/apply machinery; outbound worker drains queued events. BookFusion and BookOrbit position pulls are on demand, not periodic. | Background origin does not bypass the guard. [F9][F10][F11] |
| Connector “Sync now”, per-book sync, manual match/backfill | `backfillConnector` queues latest per-document progress, and optional highlights. | Backfilled progress/finished events also use the guard. Fan-out/backfill classify ≥98% as finished, unlike Sticky's 99%; the Hardcover connector also recognizes explicit finished events and ≥99.9%. [F11][F12] |
| Mark finished / enter finish date in Sync UI | Document routes fan out a 100% event. | Same guard; manually editing Sync status is not authorization to reread on Hardcover. [F13] |
| Account restore, tracker CSV, KOInsight/history/stats imports; document merge/reset | These manipulate local library/history/progress state, without a separate direct Hardcover mutation path in the inspected code. Account restore clears queues/matches rather than restoring provider credentials. A later backfill or progress sync can export restored progress. | Subsequent exported events still pass through the connector. Local restore/reset/history is not governed by Hardcover's Read status. [F14] |

The outbound runner has one `connector.push(...)` dispatch; disabled/unlinked accounts are rejected, and retries re-enter `push`, re-fetching current shelf context. Catalog enrichment (`models/hardcover-catalog.ts`) queries public book metadata and is separate from authenticated shelf mutations. No second direct Hardcover progress writer was found in Sync's registered source paths. [F5][F11][F15]

### What can and cannot be promised for hosted Sync

**Source-level conclusion:** ordinary progress/finish propagation from every linked client is centrally protected when the connector successfully observes the correct book as Read. A custom bridge is **not inherently necessary just to get this behavior**. Prefer testing the stock connector before adding a new writer.

**Not yet a production guarantee:** the public hosted landing page advertises the connectors; `/healthz` reports `0.2.0`, not a deployed commit or an invariant contract. We did not verify the authenticated deployment includes this guard. In addition to matching the correct book, two concrete source limitations matter:

- The context response classifier checks HTTP/auth/GraphQL errors but does **not require a valid `data.me` structure**. A 200 empty/partial context with no recognized errors can be interpreted as “not on shelf,” allowing `insert_user_book` with Reading. This is a fail-open gap against an absolute requirement; returning success on missing context is not sufficient protection. [F5]
- Shelf read and subsequent mutations are separate API calls. If Hardcover is marked Read after the check, a pending mutation can still run. Queue serialization within one worker is not an atomic compare-and-set on Hardcover and cannot control other applications. [F5][F11]

There is no inspected **account-wide “never restart completed books everywhere” switch**. The stock guard covers this account's Hardcover connector, not other writers using the same Hardcover account, and not every service's own position/status. Standard device PUTs prevent backward movement relative to Sync's known position, but single rich PUTs and provider fan-in can move it backward. Sync's explicit Finished status is preserved by automatic pause/finish helpers, but a book finished only in Hardcover is not automatically marked Finished in Sync. [F5][F7][F8][F9][F16]

Thus distinguish (without weakening the confirmed requirement): **preserving Hardcover's progress/status/read history** is implemented for valid existing-Read responses; **freezing all clients' and Sync's positions because Hardcover says Read** is not. The latter would need central policy across ingress paths and cannot be achieved solely by disabling the Hardcover connector. No hosted extension hook for that policy was established.

**Conditional architecture:** if the host cannot demonstrate the deployed guard and fail-closed behavior, or the intended policy is stronger, retain a custom Hardcover bridge or self-host an amended Sync. A bridge should consume canonical hosted state across all clients, not just Sticky exit events; validate remote context, serialize by Hardcover book/account, latch completion by book identity across document hashes, retry without creating duplicate sessions, and reserve any latch reset for an explicit reread command. These are proposed requirements, not features of the current Convex code. Polling Sync's authenticated progress/change APIs is source-supported; a hosted webhook contract was not established. A bridge that only writes Hardcover still cannot freeze the hosted service's internal progress. [F2][F3][F8][F17]

**One writer only:** choose either (a) stock Sync → Hardcover and disable the Convex Hardcover job, or (b) bridge → Hardcover and unlink the stock Hardcover connector. Unlink removes account, matches and queued jobs; it cannot recall an HTTP mutation already in flight. Drain/verify the old writer is idle before activating its replacement, and revoke its dedicated credential if hard exclusion is required. Also disable any direct reader plugin/provider-to-Hardcover integration (BookOrbit has its own Hardcover integration). Source-connector echo suppression prevents routing a change straight back to its source; it is **not** deduplication between two independent Hardcover writers. No settings or credentials were changed in this research. [F9][F11][F18][F22]

### Non-Readest phone readers: actual compatibility

There are two distinct architectures: a phone app that directly speaks KOSync, and a real phone reader whose provider is bridged by Sync. Do not call the second a configurable KOSync phone app.

| Reader | Phone support from primary sources | Endpoint/auth and identity/position evidence | Assessment |
|---|---|---|---|
| **KOReader** | Official platform list includes Android, not iOS. | Built-in Custom sync server accepts hosted Sync URL; username/password-derived key uses `x-auth-user`/`x-auth-key`. Supports partial-binary MD5 or filename MD5, and EPUB XPointer progress. Hosted Sync accepts these formats; configure matching method identically and use identical EPUB bytes. CrossNext can consume KOSync positions, but parser/layout differences still require a passage round trip. | Strongest **direct KOSync Android** candidate. Not an iPhone option. Protocol/source compatibility and hosted advertisement, not an integration proven on this user's books. [F19][F20][F7][N7][N8] |
| **BookFusion** | Official iPhone/iPad and Android readers, plus web, with EPUB and offline reading. | Phone signs into BookFusion; no custom KOSync endpoint setting was established. Hosted Sync's **BookFusion connector** uses device authorization and bearer API access. Two-way exact conversion requires `bookfusion_id` metadata from the downloaded book's `.meta.json` sidecar (`source: sidecar`); manual/title match alone only pushes percentage and **does not pull**. Server downloads provider EPUB, translates XPath ↔ CFI, and preserves text-offset conventions. Pull occurs on reader progress GET, not background polling. Missing/unresolvable CFI produces failure instead of an estimated position. | Best first **iPhone trial** under the preference for stock services, conditional on BookFusion becoming the book source, sidecar transmission, and hosting/account acceptance. Hosted site explicitly advertises both directions; fixture tests exercise conversion and route→queue integration. **No actual phone↔Sticky passage round trip proven here.** [F21][F23][F24] |
| **BookOrbit** | Official native iPhone reader requires iOS 26+ and server 3.0+; web reader is available. Official Android app was in closed testing in the inspected current primary sources. | Phone/web reader talks to a BookOrbit server, not arbitrary hosted KOSync. Sync's beta connector uses BookOrbit URL plus API key (3.8+) or legacy login; REST matches book/file IDs by title/author or explicit match. It translates `koreaderProgress` XPath ↔ EPUB CFI using BookOrbit's EPUB. Its source explicitly rejects treating a generic KOSync mirror as equivalent: BookOrbit's KOSync identity is binary partial MD5 whereas CrossPoint defaults to filename MD5. Conversion failure can fall back to percentage/sample behavior. Pull on progress request/manual sync; no periodic BookOrbit position polling. | Credible second **iPhone** candidate if OS and extra server/managed hosting are acceptable. Hosted connector is advertised beta; exact-position source exists, but native iPhone output and round-trip behavior are unverified here. Android native availability needs rechecking before choosing. [F22][F25][F26][F9][F10] |

CrossNext already has generic catalog-sidecar writing and includes `<book>.meta.json` fields in sync metadata. This is encouraging for BookFusion without inventing new position fields, but an available/installable BookFusion plugin, its manifest, download path, metadata setting and real EPUB conversion still need a trial on the selected CrossNext build. Do not substitute a manually selected BookFusion match: it does not set `fromSidecar` in the current connector. [F23][F27]

Readwise Reader is **not an equivalent exact-resume recommendation**: the inspected connector pulls `reading_progress` percentage and archives finished documents; it does not provide the same two-way EPUB-position contract. Kindle/audio connectors likewise are not evidence for arbitrary EPUB exact passage synchronization. No verified custom-hosted-KOSync integration was established for Apple Books, Yomu, or PocketBook in this investigation; their generic sync/OPDS features are insufficient grounds to recommend them. [F28]

### Sticky ergonomics: concrete questions for a later trial

CrossNext's Sticky board declares Up GPIO5, Down GPIO6, shared Confirm/Power GPIO4, and no dedicated Back/Left/Right pins. The SDK's `DigitalConfirmPowerHold` normally emits Confirm on a short release and Power on hold; `main.cpp:988` can instead emit Power immediately when short-power is configured to Sleep. Touch boards then use the main power-menu policy. Therefore the board comment “hold sleeps” alone does not describe final application behavior. This conflicts directly with current Sticky's click-Back / hold-record behavior and must be consciously reconciled. [N3][F29][L1]

Reader touch supports configurable tap/swipe turns, edge tap zones (15% or at least 32 px), a left-third/remainder split when both directions accept taps, direction inversion/RTL, and long-press selection. `isTouchMenuTap` and `isTouchMenuGesture` return false; reading controls are reached from the bottom edge, not a presumed center tap. The reader deliberately avoids left-edge swipe-to-exit. Confirm opens reader controls; held page buttons can perform chapter skip/orientation functions. These are source mappings, not tested Sticky ergonomics. [F30]

Later settle: preferred short third-button action; how to enter Confirm/Menu if click remains Back; preservation of hold-to-record from awake and sleep; lock chord; acceptable touch versus button-only actions; gesture/hold thresholds and orientation. No button remapping is decided by this report.

### Remaining decisions and acceptance evidence

1. Confirm phone OS/version (existing context points to iPhone), then choose BookFusion source/sidecar workflow versus BookOrbit hosting, or KOReader if Android.
2. Confirm whether completed-book protection applies specifically to Hardcover records or also requires freezing Sync/provider/local reading positions. **Do not relax the Hardcover invariant either way.** Define intentional reread authorization and how a book completed elsewhere is recognized across editions/hashes.
3. Establish hosted deployed revision and fail-closed behavior, then test an already-Read book at 0%, mid-book and 100% from every chosen client, via offline replay, rich single PUT, bulk sync, provider pull, backfill and retry. Snapshot Hardcover status, progress, read IDs/count and dates before/after; include missing context and concurrent completion cases. No production account was changed to test this.
4. Prove the same passage resumes phone→Sticky and Sticky→phone, including non-ASCII text and font changes; do not accept percentages alone as exact-position evidence.
5. Choose the sole Hardcover writer and required bridge policy only after those results. Retain Convex for voice and other established gaps regardless.

**Conclusion:** stock Sync has substantially more completed-book protection and non-Readest phone integration than the initial broad feature inventory established. It is a justified trial candidate, but neither the hosted hard invariant nor a working iPhone reading loop is proven. A custom bridge is a conditional fallback, not a prerequisite inferred merely from Sticky's customization.

## Source index

Local links identify the inspected source lines; upstream links are pinned unless explicitly a live service endpoint.

[L1]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/STICKY.md:20
[L2]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/freeink-sdk/libs/hardware/Microphone/src/Microphone.cpp:19
[L3]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/convex/http.ts:25
[L4]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/SPEC.md:10
[L5]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/ISSUES.md:5
[L6]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/src/voice/VoiceNotes.cpp:25
[L7]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/src/voice/VoiceFormat.h:13
[L8]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/src/main.cpp:667
[L9]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/src/voice/VoiceNotes.h:12
[L10]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/src/activities/reader/EpubReaderActivity.cpp:1630
[L11]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/src/activities/reader/ClipSelectionActivity.cpp:366
[L12]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/convex/transcribe.ts:5
[L13]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/convex/notes.ts:59
[L14]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/src/voice/VoiceUpload.cpp:39
[L15]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/src/sections/Notes.tsx:104
[L16]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/src/activities/reader/AutoProgressSync.cpp:103
[L17]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/convex/books.ts:42
[L18]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/src/activities/home/HomeActivity.cpp:314
[L19]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/convex/reading.ts:7
[L20]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/convex/hardcover.ts:12
[L21]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/src/activities/voice/CardsActivity.cpp:65
[L22]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/src/activities/boot_sleep/SleepActivity.cpp:829
[L23]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/src/sections/LockScreen.tsx:253
[L24]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/convex/lockScreen.ts:163
[L25]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/src/activities/network/CrossPointWebServerActivity.cpp:28
[L26]: /Users/vishalbalaji/Documents/Projects/sticky/firmware/src/activities/settings/SettingsActivity.cpp:125
[N1]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/README.md
[N2]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/platformio.ini#L355-L409
[N3]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h#L1472-L1525
[N4]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/freeink-sdk/libs/hardware/Microphone/src/Microphone.cpp#L19-L77
[N5]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/src/clippings/ClippingSync.cpp#L130-L203
[N6]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/.gitmodules
[N7]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/lib/KOReaderSync/KOReaderCredentialStore.h#L8-L95
[N8]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/lib/KOReaderSync/KOReaderSyncClient.cpp#L125-L260
[N9]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/src/apps_local/reading/ReadingProgressSync.cpp
[N10]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/src/apps_local/study/StudyFsrs.h
[N11]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/src/apps_local/study/StudySync.cpp#L375-L523
[N12]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/src/apps_local/notes/NotesCore.h#L3-L39
[S2]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/routes/v1/clippings.ts#L11-L160
[S3]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/app/README.md#L1-L28
[S4]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/app/src/App.jsx#L513-L536
[S5]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/app.ts#L79-L103
[S6]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/docs/API.md#L187-L232
[S7]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/connectors/hardcover.ts
[S8]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/routes/v1/sleep-image.ts
[S9]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/README.md

### Follow-up sources

KOReader and phone-product links above are live primary sources inspected on the research date; Sync and CrossNext links remain pinned. Additional path-specific evidence for grouped audit rows:

[F1]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/SPEC.md:30
[F2]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/convex/reading.ts:79
[F3]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/convex/hardcover.ts:35
[F4]: /Users/vishalbalaji/Documents/Projects/sticky/cloud/convex/reading.test.ts:222
[F5]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/connectors/hardcover.ts#L348-L700
[F6]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/test/connectors-more.test.ts#L640-L657
[F7]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/routes/kosync.ts#L244-L405
[F8]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/routes/v1/progress.ts#L27-L83
[F9]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/connectors/fanin.ts
[F10]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/connectors/refresh.ts
[F11]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/connectors/runner.ts#L115-L283
[F12]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/connectors/fanout.ts
[F13]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/routes/v1/documents.ts#L123-L205
[F14]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/models/account-data.ts#L43-L140
[F15]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/models/hardcover-catalog.ts
[F16]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/models/pause.ts#L56-L77
[F17]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/docs/API.md#L300-L355
[F18]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/routes/v1/connectors.ts#L540-L612
[F19]: https://koreader.rocks/
[F20]: https://github.com/koreader/koreader/blob/master/plugins/kosync.koplugin/main.lua
[F21]: https://www.bookfusion.com/reading/apps
[F22]: https://github.com/bookorbit/bookorbit
[F23]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/connectors/bookfusion.ts#L128-L220
[F24]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/test/bookfusion-position.test.ts
[F25]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/connectors/bookorbit.ts
[F26]: https://github.com/bookorbit/bookorbit/issues/2001
[F27]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/src/activities/reader/KOReaderSyncActivity.cpp#L380-L400
[F28]: https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/connectors/readwise-reader.ts
[F29]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/freeink-sdk/libs/hardware/InputManager/src/InputManager.cpp#L470-L516
[F30]: https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/src/activities/reader/ReaderUtils.h#L94-L146

- [Tracker import (local metadata/dates)](https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/models/tracker-import.ts#L139-L178) and [KOInsight history import](https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/models/koinsight-import.ts).
- [KOReader auth headers and custom base URL](https://github.com/koreader/koreader/blob/master/plugins/kosync.koplugin/KOSyncClient.lua#L21-L34).
- [Hosted connector claims](https://sync.crosspointreader.com/) and [BookFusion exact-position API contract](https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/docs/API.md#L110-L140).
- [CrossNext application power policy](https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/src/main.cpp#L980-L990), [touch power handling](https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/src/main.cpp#L1132-L1165), and [reader input handling](https://github.com/manuelx900/CrossNext/blob/e1174b339e9d4e0cc260c932c493e9fb1872bb44/src/activities/reader/EpubReaderActivity.cpp#L690-L976).
- [Hardcover response decoding](https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/connectors/hardcover.ts#L179-L217), [merge path](https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/models/merge.ts), [progress deletion/reset](https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/routes/v1/progress.ts#L283-L294), and [stats import routes](https://github.com/crosspoint-reader/crosspoint-sync/blob/a0dde1dff028372b9d9c579551091c981de2ad3d/src/routes/v1/stats.ts).


## Approved trial and first checks (2026-11-14)

User confirmed the migration direction and authorized testing with Sticky connected to the MacBook. Readest on iPhone is retained; BookFusion is not selected. Prefer upstream features, retain custom voice/backend features where needed. Finished-book reading positions may sync, but Hardcover must stay unchanged until an explicit reread. Hold third button records, including from sleep; Up+Down locks; third-button click uses upstream Confirm. Buttons must choose/open books, turn pages, and navigate/select open menus. Touch may open menus and return/back.

Initial trial separates hosted sync from firmware replacement. User needs to create a CrossPoint Sync account and has the iPhone available. Keep hosted Hardcover disconnected while the existing Convex writer remains active.

- USB serial capture (115200, DTR/RTS false) identifies firmware `2.0.0-dev-sticky-os-e8afb8e1`, Sticky board, SD and RTC detected, display initialized, Home entered. Existing Convex KOSync endpoint and Binary matching remain configured. A core-dump checksum warning appears at boot; this capture does not establish a current crash. Log: `/Users/vishalbalaji/Documents/Projects/sticky/backup/testing-2026-11-14/baseline-serial.log`.
- Existing local firmware.bin, bootloader.bin, partitions.bin and firmware.elf preserved with SHA-256 manifest under `backup/testing-2026-11-14/build-artifacts/`. These are build artifacts, NOT a flash readback or SD backup. No flash performed.
- Upstream Sync source at `a0dde1dff028372b9d9c579551091c981de2ad3d`: installed locked dependencies in the temporary research checkout; ran `npm test -- test/kosync.compat.test.ts test/progress.test.ts test/progress-client-time.test.ts test/readest.test.ts test/connectors-more.test.ts`. Vitest matched six files (including a matching readest filename): 151 tests passed. Static frontend missing warnings are expected because app/dist was not built. Tests use fixtures/mocks and do not prove hosted or physical-device interoperability. Saved test output in the backup directory.
- Remaining live check: authenticate both readers against hosted Sync with the same account and byte-identical disposable EPUB, Binary/File Content matching, then verify exact passage in both directions. Do not enable both direct KOSync and a separate Readest service connector during this first test. Preserve original settings for rollback.
