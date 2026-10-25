# Avatar cache centralization: correctness-first migration plan

**Status:** staged implementation in progress; **do not remove the native-chat memo yet**.
**Target:** option **C**, a unified, account-scoped `AvatarCache` with bounded negative-result memoization.
**Source:** detailed audit supplied 2026-10-24, performed against Psi `master` at `d709963e`.
**Audited report:** [English translation of the original Russian-language audit](avatar-cache-audit-source-2026-10-24.md), preserving its measurements and findings; the original source-language text is not stored in the repository.
**Not a performance promise:** the existing per-window QPixmap memo (option A) is demonstrably faster in isolated lookups; option C primarily improves correctness, ownership, bounded state and consistency between consumers.

## 0. Rules and safety constraints

1. **Correct invalidation and account/occupant identity before negative caching.** Never indefinitely memoize an arbitrary empty `QPixmap`.
2. **Do not infer avatar absence from a getter returning null.** Distinguish unknown, known absence, fetching, invalid data, unavailable source, a deleted source, and a URI-only PHOTO without embedded image bytes.
3. Do not treat XEP-0153 presence with an empty update element as the same event as an explicit empty `<photo/>`. Do not poll vCards to discover new avatars. See [XEP-0153 presence rules](https://xmpp.org/extensions/xep-0153.html#bizrules-presence) and [download rules](https://xmpp.org/extensions/xep-0153.html#bizrules-download).
4. **Never allow a late network response to revive an avatar belonging to a previous generation of an occupant, room or account.**
5. Cache decoded `QPixmap` on the GUI thread. Keep `iconset_` until all tooltip, WebEngine, theme and plugin callers have been audited and preserved.
6. Keep shared `FileCache` image **bytes by hash**, but scope JID bindings, decoded resolution state, negative memo and vCard request lifecycles to **account ID + contact/MUC identity + normalized bare/full JID**. The same JID on two accounts must never cross-contaminate an avatar.
7. Keep the semantics of ordinary contact vs MUC private chat; **do not move `isRoomOccupantChat()` into the per-message callback**. It scans the roster and is currently evaluated when configuring the peer (`psichatdlg.cpp`, around line 460 of the audited revision).
8. Ship compatible Qt5 and Qt6 builds and behavior-based tests against **real** `AvatarCache` and `VCardFactory`, not only `tests/account/nativechatavatars.cpp` doubles. Merge stages only after relevant checks are green.
9. Do not conflate the separate vCard4/vcard-temp **retrieval** project with avatar cache identity. vCard storage is server-owned, and client resource caps do not establish server vCard availability. The retrieval and first disk-cache fixes were **merged in [Psi PR #1034](https://github.com/psi-im/psi/pull/1034)**. The audit predates those fixes.

## Implementation progress (2026-10-25)

- **Merged:** [#1034](https://github.com/psi-im/psi/pull/1034) — independent modern/legacy vCard retrieval, PHOTO-only requests, preservation of modern fields, first disk-cache hit, real-Iris regressions.
- **In progress:** Phase 1 PHOTO consistency — self-avatar publication currently discards bytes, clearing PHOTO can leave a zero-valued URI property, and explicit XEP-0153 removal may notify before the cached PHOTO is erased. Correct these and test the real Iris mutators; do **not** treat that as completion of the full real-AvatarCache integration gate.
- **Next:** Phase 2 source invalidation (including PEP retractions and failed manual-avatar imports), followed by account identity scoping. Do not start centralized negative caching or remove per-window QPixmap caches beforehand.

### Network-budget invariants for this migration

- **Avatar getters must remain pure local lookups.** Neither a cache miss, an expired negative memo nor a new materialized chat message starts a network IQ.
- Use XEP-0084 metadata and XEP-0153 hashes to decide whether content-hash bytes are already cached. Do not download avatar blobs already present in FileCache.
- At most one outstanding avatar-data IQ per account/contact/SHA-1; stale blob replies must not restore superseded metadata. A failed request is retryable only on subsequent protocol-driven events, after a bounded cooldown, **never via polling**.
- XEP-0153 avatar refresh retrieves only the legacy vcard-temp store, never the full vCard4 PEP store; if PHOTO is removed, update existing sources locally with **zero** network requests.
- Full vCard4+vcard-temp IQs remain explicit, not a side effect of rendering; coalesce compatible requests within one account/session but never across accounts or incompatible query modes.
- These rules require deterministic tests counting network-request decisions, explicit checks that in-flight IDs remain unique, and scenario tests after disconnect/reconnect and stale replies. Unnecessary requests carrying the avatar **blob** are a release blocker.

### In-flight staged PRs

- [#1042](https://github.com/psi-im/psi/pull/1042): PHOTO mutation, empty-hash consistency, Silent notifications, and Iris vCard nullness.
- [#1044](https://github.com/psi-im/psi/pull/1044): stateful bounded avatar-data request coalescing, retry cooldown, hash verification, retractions and stale replies, with real Qt5/Qt6 policy tests.
- [#1045](https://github.com/psi-im/psi/pull/1045): account/session/mode-scoped vCard IQ queue, preserving coalescing within an account.
- [#1046](https://github.com/psi-im/psi/pull/1046): independent account-scoped vCard4 and vcard-temp source sidecars, preserving modern PHOTO when a legacy PHOTO is removed. The displayed merged-card cache remains globally keyed and **is not yet account-isolated**.

The preceding PRs are **stacked, not merged**. MUC epochs, full account-scoped bindings, bounded negative memo and removal of the per-window pixmap memo remain gated on real integration tests.

## 1. Audited architecture / baseline (2026-10-24)

| Layer | Contents and lifetime | Risk / constraint |
|---|---|---|
| `FileCache` (parent of `AvatarCache`) | Image bytes indexed by content hash, metadata of users; disk cache with approximately one-week avatar lifetime; in-use records protected against deletion for this session | Existing ~1 MiB RAM / ~50 MiB disk settings **do not bound** the decoded pixmaps in `iconset_` or NativeChatAvatars |
| `AvatarCache::jidToIcons` | JID → custom, PEP, vCard and derived-image bindings in one singleton | Key has **no account ID**; both contact and occupant identities may collide |
| `AvatarCache::iconset_` | Decoded successful image icons | No explicit upper bound/TTL or occupant-leave cleanup; tooltip `<icon name="avatars/...">` and WebEngine hash/data APIs depend on it |
| `VCardFactory::vcardDict_` | Small RAM vCard cache plus XML files on disk | `dictSize_=5` gives roughly six cached entries; disk misses repeat; original code's first disk hit loads then erroneously returns null |
| `VCardFactory::mucVcardDict_` | Room → nick → vCard | Three vCards retained per room by insertion order, **not LRU**; no room-count bound; nested QHash is **implicitly shared**, not deeply cloned on each lookup |
| `NativeChatAvatars` | Per-open-window successful **and null** pixmap copies | Until window close, peer change or matching avatarChanged; occupant map unbounded; `QPixmap` copies themselves are implicitly shared |

### Cold/negative lookup costs and consumers

`getMucAvatar()` creates a full JID and `avatars/<full-jid>` key, tests `iconset_`, then `jidToIcons`, then the room QMap and nickname QHash in `VCardFactory`, then PHOTO. It **does not decode anything when no photo exists**. Repeated decode is relevant when data is damaged, a derived image must be regenerated, or invalidation has occurred. `mucVcard()` copies the outer reference to an implicitly shared nested QHash; it does not deep-copy three cards.

For contacts, a `VCardFactory::vcard()` RAM miss can cause an XML disk read or repeated failed file-open. Measured failed file-open ~0.98–1.33 µs. `withResource()` alone ~0.21–0.29 µs. Image getters **do not trigger network requests**; presence/PEP handling and explicit vCard calls do.

Consumers beyond native chat: roster, MUC-user model, tooltip, notifications, InfoDlg, file transfer and plugins, plus WebEngine/theme hash and icon APIs. Centralization must preserve all of them.

### Null/result taxonomy

| Cause | What the existing system knows | Required interpretation |
|---|---|---|
| Not fetched / no vCard cache entry | Internal resolver sees miss; caller sees null | **Unknown**, retry eligible; not permanent NoPhoto |
| Successful fetched vCard without PHOTO, explicit source removal | Protocol/event confirms it | **NoPhoto** for that source, with event-based invalidation |
| Fetching | Request queue knows; AvatarCache does not fully track | **Fetching**, not negative absence |
| Corrupt/unreadable bytes | Detected at decode; API returns null | **InvalidImage**, controlled retry/invalidation |
| Explicit removal | Source event visible | Remove only that source, invalidate active result and memo |
| Temporary unavailable network/source | Error known to requester but not propagated as state | **Unavailable**, never permanent NoPhoto |
| PHOTO stored as URI but no embedded bytes | Legacy `QByteArray(photo)` may be empty | **Not confirmed absent**; preserve URI semantics |
| `OpResult::NoData` | Conflates missing content by hash and corrupt/unusable data | Not a reliable global negative state |

## 2. Audited invalidation matrix and target rules

| Event | Current behavior at audited revision | Required migration rule |
|---|---|---|
| First valid vCard photo after no photo | `setIcon()` transitions absent→present, clears `iconset_`, emits `avatarChanged` | Retain; invalidate central negative even if **no previous positive icon** |
| New vCard with no PHOTO | `removeIcon()` notifies only if active image changes | Record confirmed absence for vCard source and invalidate stale negative/decoded state |
| New hash, bytes cached | `appendUser()` and active-image change notification | Preserve |
| New hash, bytes not cached | Fetch starts; old image can remain | Mark fetching; never convert to definitive NoPhoto |
| Empty vCard photo hash | Removes AvatarCache binding | Also clear/update stale PHOTO in `VCardFactory`; prevent resurrection |
| Valid PEP data | Updates and notifies | Preserve |
| Empty PEP metadata | Removes PEP source; vCard fallback possible | Do **not** conclude no avatar overall |
| vCard/PEP failure | No full transition in AvatarCache | Record transient Unavailable/Unknown; bounded, retryable |
| PEP retraction | PEPManager has separate signal; AvatarFactory not subscribed | Handle source retraction explicitly |
| Manual-avatar import | Clears `iconset_`, emits even on failed import | Review ordering and signal on real state transition |
| Manual-avatar deletion | Signals when active image changes | Preserve; centralize invalidation |
| Own avatar publication; `Silent` vCard updates | Not every change notifies cache | Update cache/source **before** notifying UI; do not rely only on `vcardChanged` |
| Occupant leaves | `statusUpdate(Offline)` clears only `jid2hash` | Clear scoped bindings, decoded and negative entries and occupant vCard |
| Nick changes or is reused | No coordinated cross-layer invalidation | Drop old generation; never display previous occupant's photo |
| Leave/rejoin same room | Users model cleared; `NativeChatAvatars::setPeer()` may retain same-room memo | Advance room generation and refresh all relevant caches |
| Disconnect/reconnect account | Singleton/window caches remain | Reset one account's generation only |
| FileCache item removal | Rewrites bindings without iconset/UI invalidation | Invalidate decoded/negative state and notify as appropriate |

**Critical observation:** a first successful photo is *usually already able to invalidate a local null*. That success does not justify keeping *all other kinds of null* forever.

## 3. Defects to fix before enabling centralized negative cache

- [x] **First vCard disk read:** fixed in [#1034](https://github.com/psi-im/psi/pull/1034), with real-Iris Qt5/Qt6 regressions for modern vCard4 XML, legacy vcard-temp XML, URI-only PHOTO, and missing/corrupt files.
- [ ] **Deleted PHOTO resurrected:** clearing a hash in `avatars.cpp` while `VCardFactory` still retains PHOTO can cause the next getter to re-import old pixels. Clear the right source before publishing invalidation; test the real AvatarCache/VCardFactory and both hash/PEP paths.
- [ ] **Cross-account contamination:** `jidToIcons`, `iconset_`, MUC vCard cache and vCard request queue lack account identity. Queue merges requests from different accounts and executes on whichever is connected. Scope these without breaking the public API, shared bytes or existing consumers.
- [ ] **`VCardFactory::setPhoto()` bytes discarded:** at audited line ~275, compares to the supplied bytes but sets `UriValue{}` instead. The API is invoked on the **Silent self-avatar publication** path when the server supports avatar conversion. Fix the behavior with a targeted test and a carefully chosen media type (or document why null is intentionally required), not by silently changing caller behavior.
- [ ] **Separate unknown from fetched-without-photo**, including PEP URI-only PHOTO and unavailability, at the result-resolution boundary.
- [ ] **PEP retractions and Silent state changes** must invalidate the correct sources regardless of whether the last active image existed.

## 4. Migration workstreams and merge gates

Work in order; split large changes into reviewable, revertible PRs. Prefer tests **before or with** each behavior change.

### Phase 1 — Establish vCard correctness and provenance

**Files:** `src/vcardfactory.cpp/.h`, avatar-aware request flow, tests using real Iris/VCardFactory.

- [ ] Repair first disk hit; distinguish absent XML from parsed vCard with no PHOTO.
- [ ] Test PHOTO with embedded bytes, PHOTO URI-only, missing PHOTO, malformed payload, missing/corrupt disk file.
- [x] Reconcile vCard4/vcard-temp full-card and photo-only semantics in [#1034](https://github.com/psi-im/psi/pull/1034). PHOTO-only refresh preserves non-PHOTO fields. XEP-0054 IQ still fetches the complete legacy payload; only its PHOTO is applied.
- [ ] Fix/test `setPhoto()` and `deletePhoto()`, especially Silent publication and account avatar conversion.
- [ ] Record unknown vs successfully fetched-without-PHOTO vs URI-only vs failed fetch. Never treat a null pixel object as authoritative absence.

**Exit:** a real-factory test proves a single successful disk load works; PHOTO refresh preserves other data; no old photo returns after a removal; self-publish tests document the actual intended semantics. Qt5 and Qt6 green. Keep native-chat memo intact.

### Phase 2 — Correct change propagation, source invalidation and notification ordering

**Files:** `src/avatars.cpp`, VCardFactory callbacks, PEP itemPublished/retraction and AvatarFactory signals.

- [ ] Resolve stale VCardFactory PHOTO vs removed hash; model per-source removal vs absence across all sources. **Important:** an empty XEP-0153 legacy photo hash must not erase a separate vCard4 PHOTO; the existing merged vCard cache lacks PHOTO provenance, so a source-aware representation or equivalent restoration rule is still required before declaring this gate complete.
- [ ] Cover PEP metadata deletion/retraction (including vCard fallback), manual import failure/success, `Silent` mutation, own avatar publish/delete.
- [ ] Update authoritative source, bindings, decoded icons and memo **before** one coherent `avatarChanged` notification.
- [ ] Notify when a formerly **negative** result gains content, even if no old active icon existed.

**Exit:** real end-to-end tests verify first photo after null; explicit delete cannot resurrect; Silent updates and retractions invalidate; consumers see no stale image after notification.

### Phase 3 — Account-scoped identities and request queue

**Files:** AvatarFactory, AvatarCache, VCardFactory, FileCache binding metadata, callers where account context is already known.

- [ ] Introduce explicit scoped key: stable account ID + entity kind (contact vs MUC occupant) + normalized bare/full JID. Full occupant JID is not the same identity as private-contact bare JID.
- [ ] Maintain account scope in `jidToIcons`, positive decoded entries, vCard memory/disk records, metadata users, remembered hashes and queued network requests; keep raw image bytes shared by content hash.
- [ ] Avoid accidentally coalescing requests from two accounts with the same JID. Preserve priority and lifecycle semantics; audit any parallel photo/full requests.
- [ ] Define compatibility rules for legacy on-disk cache keys and `<icon name="avatars/...">` APIs, WebEngine hash/data access, tooltip and plugins. Public-facing API behavior must continue working; explicit account context should enter at the earliest reliable boundary.
- [ ] Test same JID with **different avatars on two accounts**, both online at once; removing/reconnecting one must not affect the other.

**Exit:** all relevant storages and requests are account-correct; no cross-account mixing; hash bytes remain deduplicated; native-chat memo still enabled.

### Phase 4 — Centralized invalidation of decoded/negative resolution

**Files:** `src/avatars.cpp`, AvatarCache operations, AvatarFactory lifecycle hooks.

- [ ] Add one invalidation entry point for scoped source, decoded icon and negative lookup.
- [ ] Trigger it for *every* relevant source change, **not just `UserUpdateRequired` or changes from a positive old icon**.
- [ ] Cover `FileCache::removeItem` cleanup, PEP retraction, manual override, vCard change and deletion.
- [ ] Keep UI notifications after state becomes consistent; minimize redundant expensive repaint.

**Exit:** unit and integration tests verify invalidation without a prior icon and without leaked decoded images; tooltips and plugins still resolve.

### Phase 5 — MUC occupant epochs and reconnect boundaries

**Files:** `src/avatars.cpp`, room presence in `src/groupchatdlg.cpp`, MUC vCard cache, account reconnect, native-chat peer setup.

- [ ] Define generation/epoch per account + room and per occupant identity where needed.
- [ ] On leave, nick rename/reuse, room exit/rejoin or reconnect, clear appropriate vCard, source bindings, decoded and negative entries.
- [ ] Mark pending requests with originating epoch and **discard stale completion** instead of reviving old avatars.
- [ ] Preserve occupant/avatar distinction in MUC private chats, including normal contact fallback.
- [ ] Invalidate only the relevant account, not all simultaneous accounts.

**Exit:** reproducible tests for nick reuse, fourth MUC-vCard eviction, same-room rejoin, disconnect/reconnect and out-of-order network responses; no stale avatar replay.

### Phase 6 — Bounded negative lookup memo in AvatarCache (option C)

**Files:** AvatarCache in `src/avatars.cpp`, dedicated state model and tests.

- [ ] Retain existing positive-cache / FileCache behavior. Add a separate **bounded `QCache`** for negative resolution.
- [ ] Start with **4096 entries** and lazy **30-second TTL** as tunable baselines, *not protocol constants*. Use `QElapsedTimer` or comparable monotonic clock; no sweep timer.
- [ ] Model `Unknown`, `NoPhoto`, `Fetching`, `InvalidImage`, `Unavailable` explicitly. Only proven absence may use confirmed-NoPhoto semantics; transient errors must expire/retry on demand.
- [ ] Validate generation on lookup and on asynchronous update; invalidate on source events even if prior lookup returned null.
- [ ] Avoid caching invalid/URI-only/unknown states indefinitely. TTL **must not** trigger network requests, periodic refresh or XEP-0153 polling.
- [ ] Keep empty placeholders **out of the unbounded `iconset_`**.

**Exit:** tests for expiry at 30s boundary, eviction above 4096 entries, null→new photo, corrupt data, temporary timeout→successful result and MUC generation isolation on Qt5/Qt6.

### Phase 7 — Remove per-window pixmap memo, preserve routing

**Files:** `src/nativechatavatars.cpp/.h`, `src/psichatdlg.cpp/.h`, `src/groupchatdlg.cpp/.h`, relevant widget tests.

- [ ] Make `NativeChatAvatars` responsible for selecting sender/JID, preserving normal contact vs MUC PM classification, and filtering `avatarChanged` events. It **must not** store QPixmap or null QPixmap per sender.
- [ ] Retain peer change handling, self-message routing and MUC occupant nick resolution.
- [ ] Keep `isRoomOccupantChat()` at peer setup, not in a per-message callback; do not reintroduce roster scans on hot render paths.
- [ ] Confirm the new centralized negative memo has been exercised by roster, MUC user model, tooltip, notifications and plugins before dropping the local one.

**Exit:** no pixel memo in native chat; correct routing and invalidation under mixed messages and repeated nickname use; no unexpected extra network operations.

### Phase 8 — End-to-end validation and cleanup

- [ ] Run **real** AvatarCache/VCardFactory integration tests on **Qt5 and Qt6**. Existing `tests/account/nativechatavatars.cpp` factory double is useful for routing only and **does not** count as integration coverage.
- [ ] Exercise roster, MUC user model, tooltip, WebEngine, plugins and file transfer, not just BASIC/MAM chat.
- [ ] Compare bounded-memory footprint and render/cache-lookup latency against A and B with identical workload; no claim of speedup unless **whole-timeline** tests substantiate it.
- [ ] Keep the changes rollback-friendly until hard cases pass: hash removal, malformed image, concurrent accounts, reconnect and late replies.

**Exit:** CI green across supported targets; source/invalidation contract documented; negative entries bounded; correct avatar behavior under real server tests and lifecycle churn.

## 5. Mandatory regression matrix (do not remove or downgrade)

1. First photo appears after a local null result: should invalidate and redraw.
2. Explicit PHOTO/hash removal without old-photo resurrection.
3. Fourth MUC vCard evicts the oldest of three; no false identity reuse.
4. Corrupt image bytes / format decode failure.
5. Timeout or temporary source failure then successful retry.
6. Silent self-avatar publish/update/delete with server avatar conversion.
7. Occupant nick change and reuse by a different person.
8. Account disconnect and reconnect, same room rejoined.
9. Late request response from a previous occupant/room epoch discarded.
10. Two simultaneously connected accounts owning an identical contact or MUC JID with different avatars.
11. Negative cache TTL expiry and capacity/eviction.
12. URI-only PHOTO must not be interpreted as confirmed NoPhoto.
13. PEP avatar metadata removal/retraction with valid vCard fallback.
14. Native-chat ordinary contact vs room-occupant private chat; own-message avatar.
15. Tooltip `<icon name="avatars/...">`, WebEngine hash/data API, plugins, roster, MUC user model, notifications and file transfer.
16. Qt5/Qt6 compile, integration and relevant UI behavior checks.

## 6. Benchmark baseline (model, **not an application benchmark**)

The audit measured real Qt containers, QPixmap and Iris Jid with 1000 senders and warm lookups, **median of seven runs**, excluding UI painting, network calls and image decoding. Times are **microseconds per lookup**:

| Scenario | A: per-window memo | B: only existing AvatarCache | C: central negative memo |
|---|---:|---:|---:|
| Qt5, no photo | 0.029 | 0.349 | 0.280 |
| Qt6, no photo | 0.030 | 0.815 | 0.681 |
| Qt5, cached photo | 0.033 | 0.289 | 0.283 |
| Qt6, cached photo | 0.033 | 0.710 | 0.713 |

At the current ~**160 materialized rows**, that difference is on the order of **tenths of a millisecond** per complete avatar-lookup pass in the synthetic model. Scaling, masking/cropping and painting remain separate costs. `QPixmap` sharing means per-window copies do not allocate independent full pixel buffers; a standalone 96 × 96 × 4 decoded buffer is ~**36 KiB**. The unbounded `iconset_` can already dominate pixel memory. These figures do not establish a real-chat performance improvement from option C.

**Reproducibility note:** the source report says its benchmark code and detailed outputs lived only under `/tmp`. They were **not attached to this plan or committed**; do not describe this dataset as a versioned, reproducible regression benchmark until a new harness is added.

## 7. Tracking and coordination

- **Source report:** external high-reasoning audit of `master d709963e`, supplied by project maintainer 2026-10-24. Its protocol conclusions and measurements are *audit findings*, not yet end-to-end server-verified facts.
- **Completed foundation:** [PR #1034](https://github.com/psi-im/psi/pull/1034) merged on 2026-10-25 with independent vCard4/vcard-temp retrieval, PHOTO-only refresh, first disk-cache read, and Qt5/Qt6 regression coverage. Its then-pending CI status in the original audit is historical.
- **First safe action:** preserve this plan, add/verify the one-call disk cache regression, and resolve test coverage before enabling negative memo or removing `NativeChatAvatars` caches.
- **Stage progress:** use the checkboxes in this document as the source of truth. A code-only fix without real-factory regression remains **in progress**, not “done”.
- **Review strategy:** one PR per coherent dependency-bound stage, explicit rollback point, Qt5 and Qt6 verification, source/package/ABI-compatible behavior.
