# Avatar cache audit — English translation of the original analysis (2026-10-24)

> This document translates the complete Russian-language audit supplied by the maintainer into English. It preserves the original findings, qualification of evidence, measurements, and recommendations. The original audit examined `master` at `d709963e`; the benchmark source and detailed outputs were available only under the original auditor's `/tmp` and are not included in this repository. For the actionable, maintained sequence of work, see [the avatar-cache migration plan](avatar-cache-migration-plan.md).

**I recommend moving to option C: a single `AvatarCache` with negative caching. However, invalidation and account separation must be fixed first. Indefinitely caching every null `QPixmap` is unsafe.**

After the transition, `NativeChatAvatars` should be simplified to selecting the sender and chat context, without storing `QPixmap` values. Its useful role goes beyond caching: it distinguishes an ordinary contact from a MUC private chat, filters events, and prevents repeated roster scans.

The audit was performed on `master` at `d709963e`, which matched the remote `master` at that time. The repository was left clean: no source changes, commits, or PRs were made. The benchmark and migration plan produced by the original analysis existed only in `/tmp`.

## Current caching architecture

| Layer | Stored data | Lifetime and limits |
|---|---|---|
| `FileCache`, inherited by `AvatarCache` | Image bytes indexed by hash; binding metadata | Disk cache; avatars have a one-week lifetime. Records used in the current session are protected from deletion |
| `AvatarCache::jidToIcons` | JID bindings to custom, PEP, and vCard images | Shared singleton; the key does not include an account ID |
| `AvatarCache::iconset_` | Successfully decoded images | No explicit limit, TTL, or cleanup when leaving a MUC |
| `VCardFactory` for contacts | Small RAM cache and disk XML | RAM cache of approximately six entries; misses on disk are not remembered |
| `VCardFactory` for MUC | vCards indexed by room and nickname | **Three vCards per room**; eviction in insertion order, not LRU. No global room-count limit |
| `NativeChatAvatars` | Copies of both successful and null `QPixmap` results | Until the window closes, peer changes, or a matching `avatarChanged` signal. The MUC dictionary is unbounded |

The principal implementations are `AvatarCache` and its structures, `FileCache` and its limits, `VCardFactory`, and `NativeChatAvatars`.

Important: `FileCache` limits of **1 MiB RAM / 50 MiB disk do not bound the memory used by `iconset_` or `NativeChatAvatars`**. Decoded pixels are retained separately.

## What is actually repeated when the result is null

`getMucAvatar()` performs:

1. Construct the full JID and the `avatars/<full-jid>` string.
2. Look up `iconset_`.
3. Look up `jidToIcons`.
4. If no image exists, find the room in `VCardFactory`'s `QMap` and the nickname in its nested `QHash`.
5. Inspect the vCard and its photo.

`mucVcard()` itself copies the nested `QHash`, but the container is **implicitly shared**: reading does not deeply copy the three vCards. This is an unnecessary operation involving a shared object, not a copy of all its contents. See the implementation.

When a photo is genuinely absent, **no image decoding occurs**. Repeated decoding occurs with damaged data, regeneration of a derived image, or after invalidation.

Ordinary contacts can be more expensive: after a RAM miss, `vcard()` opens an XML file. An absent file is checked again on the next call.

These getters **do not themselves initiate network requests**. Fetches are initiated by presence/PEP processing and explicit vCard requests.

Consumers beyond native chat include the roster, MUC user model, tooltips, notifications, `InfoDlg`, file transfer, and plugins (for example, roster and MUC user model code and `pluginmanager`). A centralized negative cache would benefit all of them.

## A null QPixmap currently conflates different states

| State | What the existing code can determine |
|---|---|
| Information not yet known | A vCard-cache miss is visible inside the resolver, but callers only receive null |
| Confirmed absence of a photo | A successfully fetched vCard without PHOTO, or explicit removal of a source |
| Fetch in progress | Known to the request queue; `AvatarCache` does not track a complete fetching state |
| Damaged data | Detected during decoding; callers again receive null |
| Photo removed | Visible from an incoming event, but absence is not stored as a separate state |
| Source temporarily unavailable | The request knows about the error; it is not propagated to `AvatarCache` as a separate result |

Another case is **PHOTO present as a URI, without embedded bytes**. This must not be treated as proof that no photograph exists.

`OpResult::NoData` does not solve the distinction: it is used both for image bytes missing from the content-hash cache and for unsuitable or damaged data.

The protocol also distinguishes unknown state from absence: an empty XEP-0153 update element means “not ready to announce an image yet,” whereas an explicit empty `<photo/>` means no image is being announced. They must not be collapsed into a single permanent negative result. See [XEP-0153 presence business rules](https://xmpp.org/extensions/xep-0153.html#bizrules-presence).

## Current invalidation is incomplete

| Event | Current behavior | Conclusion |
|---|---|---|
| First valid photo received via vCard | `setIcon()` changes from no active image to an image; the handler clears `iconset_` and emits `avatarChanged` | **Works even when there was no previous image** |
| New vCard without a photo | `removeIcon()` only notifies on an active-image change | Confirmed absence is not recorded separately |
| New hash, bytes already cached | `appendUser()` and notification if active image changes | Main path covered |
| New hash, bytes not fetched | Fetch starts; old result may be retained | Cannot be considered confirmed absence |
| Empty vCard photo hash | Image removed from `AvatarCache` | An old photo may reappear from `VCardFactory` |
| New valid PEP avatar data | Update and notification | Main path covered |
| Empty PEP metadata | PEP source removed; vCard fallback may be available | Does not prove that all avatar sources lack an image |
| vCard/PEP request error | No complete `AvatarCache` state transition | Must not be stored as permanent `NoPhoto` |
| PEP retraction | `PEPManager` provides a separate signal, but `AvatarFactory` is not subscribed | The event is not explicitly handled |
| Manual avatar import | `iconset_` is cleared and a signal emitted even if import fails | Invalidation exists |
| Manual avatar removal | Signal when active image changes | Works for visible changes |
| Own publication, `Silent` updates | No general guarantee that all photo changes notify the cache | Cannot rely on `vcardChanged` alone |
| MUC occupant leaves | `statusUpdate(Offline)` removes only `jid2hash` | Image, vCard, and locally cached null remain |
| Nickname change or reuse | No coordinated reset across caching layers | An old avatar belonging to someone else can appear |
| Rejoining the same room | User model is cleared; `NativeChatAvatars::setPeer()` retains the same-room cache | Stale results remain |
| Account disconnect/reconnect | Singleton and caches belonging to open windows survive | No shared freshness/generation boundary |
| `FileCache` record deletion | Bindings are adjusted, but `iconset_` and UI signals are not | Active entries are usually protected; account for this during centralization |

Key areas: vCard handling, PEP, `statusUpdate`, MUC presence, and rejoining rooms.

Therefore, the answer to the important question is: **the first successful arrival of an image usually already invalidates a local null correctly. That does not make it safe to cache every possible null indefinitely.**

## Defects to fix before implementing negative caching

- **The first vCard read from disk returns an empty object.** After successful parsing, `checkLimit()` is called, followed by an unconditional `return {}`. The next call finds the object in RAM. `NativeChatAvatars` may remember the initial null result until another signal is emitted. `vcardfactory.cpp:220`.
- **A removed photo can be restored from an old vCard.** An empty photo hash removes the image from `AvatarCache`, but `VCardFactory` may retain the old PHOTO. The next getter imports it again. Reproduction requires the old vCard still to be cached and the image not to be superseded by another source. Relevant operations: deletion and re-import.
- **Accounts are not separated.** `jidToIcons`, `iconset_`, the MUC-vCard cache, and the vCard request queue do not include an account ID. The queue even merges requests from different accounts and selects a connected account to execute them. This is an existing architectural property, not a new problem introduced by the proposed negative cache. See `AvatarCache` keys and the queue.
- **`setPhoto()` needs a separate fix or an explanation backed by a test:** the supplied bytes are not installed; the code calls `setPhoto(UriValue{})` instead. `vcardfactory.cpp:263`.

These are findings from examining concrete code paths. Deletion/reconnect scenarios against a real server were **not** executed during the audit.

## Performance comparison

| Option | Repeated lookup | Additional state |
|---|---|---|
| A: current approach | MUC: one `QHash` lookup by nickname; one-to-one chat: check an optional | Copies of results in each window, with separate invalidation |
| B: existing `AvatarCache` only | Positive result: `iconset_` lookup; null result: extra vCard lookups | No local memo; repeated misses |
| C: centralized negative cache | Positive path can remain unchanged; null path can be reduced to a negative entry lookup | One owner for results and their freshness |

In option B, the negative path includes up to three `QHash` lookups, one room lookup in a `QMap`, and JID construction/formatting operations. Early returns reduce the number of operations.

`QPixmap` is implicitly shared: `NativeChatAvatars` does not allocate a separate pixel buffer for each message. Its overhead is primarily keys, container entries, and additional references. The main potential pixel-memory cost is already the unbounded `iconset_`: one independent **96 × 96 × 4** buffer occupies about **36 KiB**.

A separate **synthetic/model** benchmark was performed using actual Qt containers, `QPixmap`, and the current Iris `Jid` implementation. It used 1,000 senders, warm lookups, and the median of seven runs. No painting, network requests, or decoding were included.

Microseconds per lookup:

| Scenario | A | B | C |
|---|---:|---:|---:|
| Qt 5, no photo | 0.029 | 0.349 | 0.280 |
| Qt 6, no photo | 0.030 | 0.815 | 0.681 |
| Qt 5, photo cached | 0.033 | 0.289 | 0.283 |
| Qt 6, photo cached | 0.033 | 0.710 | 0.713 |

`withResource()` alone took about **0.21–0.29 µs**. Attempting to open an absent file alone took about **0.98–1.33 µs**.

**The local cache really is faster than C.** However, with the current limit of **160 materialized rows**, the synthetic difference is on the order of tenths of a millisecond for a complete avatar-lookup pass. This is not a measurement of end-to-end chat-log performance. Avatar scaling, cropping and painting are separate operations that remain after removing the local memo.

The original audit referenced its benchmark source and Qt 5/Qt 6 results. The benchmark does not exercise a complete `AvatarCache`, and the source/outputs were not committed to this repository.

## Recommended architecture

Retain the shared `FileCache` for image contents by hash. Store JID bindings and resolution results inside `AvatarCache`, keyed by:

**Account ID + contact/MUC entity type + normalized bare/full JID.**

Within the same `AvatarCache`:

- Preserve the existing positive cache.
- Add a bounded `QCache` for negative results.
- Track the reason for a null result: `Unknown`, `NoPhoto`, `Fetching`, `InvalidImage`, `Unavailable`.
- Invalidate on source events even if the previous result had no image.
- Reset the vCard, bindings, and memo coherently when the person represented by a MUC nickname changes.
- Reject late request responses belonging to a previous room/occupant generation.

To bound memory, a reasonable starting point is **one limit, for example 4,096 negative entries, and one lazy TTL, for example 30 seconds**. These are initial tuning parameters, not protocol guarantees.

The TTL is checked during lookup. It neither starts requests nor triggers periodic redraws. Normal updates are event-driven; TTL only limits the lifetime of a potentially incorrect local assumption. No separate polling is needed; XEP-0153 explicitly forbids polling vCards to detect a new avatar. See [XEP-0153 download rules](https://xmpp.org/extensions/xep-0153.html#bizrules-download).

Do **not** add empty `PsiIcon` objects to the existing unbounded `iconset_`: doing so does not solve its size limit and complicates correct removal of negative entries.

`iconset_` itself cannot be removed yet: tooltips generate `<icon name="avatars/...">`, and WebEngine uses hash/data APIs. Existing consumers rely on the icon set.

## Migration order

1. **`vcardfactory.cpp` and tests:** fix the first disk read; distinguish unknown from a successfully fetched vCard without PHOTO; test URI-only PHOTO.
2. **`avatars.cpp` + `VCardFactory`:** prevent restoration of removed photos and correct `Silent` paths. Update sources and caches **before** notifying the UI.
3. **`AvatarFactory`/`AvatarCache`/`VCardFactory`:** introduce account scope into bindings and request queues. Keep image bytes shared by hash and retain the public API.
4. **`avatars.cpp`:** centralize invalidation of decoded/negative lookups. Do not condition it on the existence of a previous image or solely on `UserUpdateRequired`.
5. **MUC presence, join/leave/reconnect:** define freshness/generation boundaries and reject late responses. Resetting one account must not affect another.
6. **`AvatarCache`:** enable a bounded negative memo with lazy TTL and explicit result reasons.
7. **`nativechatavatars.*`, `psichatdlg.*`, `groupchatdlg.*`:** remove pixel caching while preserving sender selection and event filtering.
8. Verify real behavior on **Qt 5 and Qt 6** before completing the removal of duplicated caching.

Particularly important: **do not move `isRoomOccupantChat()` into the per-message callback.** It scans the roster; it is currently evaluated once while setting the peer. See `psichatdlg.cpp:460`.

For a more detailed implementation document with constraints, files, and acceptance criteria for another coding agent, use the [migration plan](avatar-cache-migration-plan.md).

Mandatory regressions: first photo after null; deletion without resurrection; eviction of a fourth MUC vCard; damaged data; timeout followed by successful fetch; `Silent`; nickname change/reuse; reconnect; stale late response; two accounts with identical JID; TTL and capacity limit; tooltips, WebEngine, and plugins. Tests must use the **real `AvatarCache`/`VCardFactory`**, because the existing `NativeChatAvatars` tests use a factory double.

There are no major Qt 5/Qt 6 blockers: `QCache` and `QElapsedTimer` are available. `QPixmap` work must stay on the GUI thread, and `qHash` signatures must support both versions if a composite key is introduced.

**Final decision: C is the target architecture. Keep A until the listed regressions pass. Remove the second cache to establish a single, correct freshness model—not because it promises faster native chat. The benchmark does not show that speedup.**
