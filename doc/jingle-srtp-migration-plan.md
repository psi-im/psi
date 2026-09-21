# SRTP ownership migration and client interoperability plan

## Validation baseline

Reviewed against the active branches on 2026-09-21: Psi `8d02305`, Iris `e35eff1`
and psimedia `0816b2a`. These identify the current implementation heads, not a tested release
triplet. Phase 1 is now partially implemented on `ai/jingle-srtp-psimedia`; the status markers
below distinguish code that has passed provider regressions from work that is still in progress.

The ownership change is sound, but completion depends on group RTCP processing, an additive
plugin ABI, and a coordinated cutover. The concrete boundaries below are implementation gates.

## Scope and relationship to the previous calls plan

This document incorporates the relevant requirements from the workspace document
`../jingle-calls-implementation-plan.md` (relative to the Psi repository root, updated
2026-09-20). For this migration and its interoperability gates, the decisions here supersede
conflicting ownership, per-media RTP-session and peer-status statements in that older plan.
The older document remains historical context for the wider native-calls work.

| Previous-plan material | Disposition here |
| --- | --- |
| P1 bridge/provider and real-media checks | Preserve and rerun for the group bridge; do not rebuild the existing adapter/controller. |
| P2 BUNDLE registry, transactions, routing and member lifetime | Preserve the implemented Iris topology; move crypto and media routing to the new boundary. |
| P3 JMI | Existing MessageInitiation/AvCall integration and tests are present; audit missing behavior against actual peers, do not create a second manager. |
| P4 feedback/video | Carry forward as negotiated feature and external A/V gates, not assumed backend support. |
| P2/P5 mixed SCTP, active-call recovery and connection drain | Preserve existing DTLS-SRTP + SCTP coexistence coverage; group lifecycle, multiple streams and active recovery remain distinct gates. |
| P6 release, sanitizers, packaging and evidence | Retain exact-build evidence and runtime gates, adapted to psimedia-owned libSRTP. |
| Sol-only connector workflow, old PR/branch directives and CI success markers | Historical, not instructions for this workspace or evidence for new commits. |
| Per-media rtpsession and Iris SrtpSession ownership diagrams | Superseded by one RTP session per negotiated group and psimedia crypto ownership. |

## Goal

Move SRTP/SRTCP packet protection out of Iris and into psimedia while keeping signaling,
transport topology and DTLS ownership in Iris. Preserve working Conversations audio and
establish tested wire interoperability with clients using either libwebrtc or GStreamer
webrtcbin for audio/video. This is not a claim of complete WebRTC feature parity.

### Interoperability model

The primary compatibility target is the wire behavior of current Jingle peers whose media
engines are based on libwebrtc, GStreamer webrtcbin, or browser WebRTC implementations. Internal
psimedia structure does not need to match those engines, but negotiated RTP/RTCP/DTLS-SRTP
semantics must.

For BUNDLE this means one RTP session per negotiated BUNDLE group, not one RTP session per
audio/video m-line. RFC 8843/9143 explicitly defines all RTP-based media in one BUNDLE group as
one RTP session with one SSRC space, and RFC 8834 requires WebRTC endpoints to support multiple
media types in such a single RTP session. Therefore the group-owned rtpsession design is an
interoperability requirement rather than a GStreamer-specific optimization.

Compatibility validation must nevertheless use independent external peers. Passing the internal
group-session regressions proves the psimedia model can represent WebRTC/BUNDLE semantics; it
does not prove compatibility with libwebrtc/webrtcbin/Firefox until the external call matrix
passes. Conversations is a particularly important first peer because its call implementation
uses libwebrtc.

#### MID interoperability gate

RFC 8843/9143 requires the RTP MID header extension on every RTP-based m-line in a BUNDLE
group. Iris already parses/serializes XEP-0294 header extensions and its authenticated
BundleRouter understands the SDES MID URI, but the current Psi psimedia adapter rejects any
`Description::headerExtensions` in `hasUnsupportedAnswerFeatures()`. Therefore the current
working internal BUNDLE path is not yet sufficient for libwebrtc/webrtcbin interoperability.

Before the external BUNDLE gate can pass:

- Psi must offer/accept the SDES MID RTP header extension for bundled RTP contents instead of
  rejecting all header extensions;
- the negotiated MID extension ID and opaque content/endpoint MID must be passed into the new
  psimedia group route metadata;
- psimedia must stamp the negotiated MID extension on outgoing RTP before libSRTP protection
  (at least until the peer can reliably associate the SSRC), while authenticated incoming MID
  continues to participate in route learning;
- incompatible PT reuse across bundled media remains fail-closed. RFC 8843/9143 permits the
  same PT in multiple bundled m-lines only when the codec configuration is identical, which
  matches the shared rtpsession PT-map validation already implemented.

Do not mark libwebrtc/webrtcbin BUNDLE interoperability complete based only on SSRC or unique-PT
fallback routing.

The target boundary is:

```
Iris
  Jingle / ICE / BUNDLE
  RFC 7983 datagram classification
  DTLS through QCA
  fingerprint verification
  DTLS-SRTP profile negotiation and key export
          |
          | authenticated SRTP parameters + protected packet I/O
          v
Psi media adapter
          |
          v
psimedia
  one secure RTP association per DTLS association / BUNDLE group
  libSRTP protect / unprotect
  RTP/RTCP routing to the media session
          |
          v
GStreamer RTP session / codecs
```

Iris must no longer link to libSRTP after the migration. psimedia will use libSRTP directly;
the GStreamer `srtpenc` / `srtpdec` elements are not part of the initial implementation.

## Non-goals and invariants

- Do not link, embed or instantiate libwebrtc or webrtcbin in Psi/Iris/psimedia. They are
  external peer implementations only, including in independent test-peer processes.
  Continue using GStreamer RTP/codec elements and direct libSRTP in psimedia.
- Preserve the existing Iris Jingle stack and Psi AvCall/AvCallManager. Do not introduce a
  second signaling/call controller or replace QCA/ICE to match a peer library's internals.

- ICE, BUNDLE membership, transport replacement and DTLS lifetime remain owned by Iris.
- QCA remains the DTLS implementation boundary. Iris still verifies the peer fingerprint before
  exposing any SRTP keying material.
- There is no plaintext RTP fallback. If there is no common DTLS-SRTP profile or no secure media
  backend, native RTP capability is not advertised.
- One secure association belongs to one authenticated DTLS association/BUNDLE group, not to
  an individual audio/video endpoint. It owns directional libSRTP sessions with per-SSRC
  crypto/replay state; “one association” does not mean one replay window for all SSRCs.
- Audio, video, RTX, future VP9/AV1 payloads and additional SSRCs must be able to share that
  association without creating per-codec crypto contexts.
- Security epochs are explicit. Packets queued under an invalidated DTLS epoch must never be
  consumed by the replacement association.
- psimedia must not depend on QCA. libSRTP uses the crypto backend selected by its build. In
  bundled builds QCA and libSRTP should normally use the same OpenSSL build; system builds keep
  the distro-selected crypto stack.
- Keep Qt 5 and Qt 6.4 source/build compatibility.
- Do not change the existing psimedia Provider/RtpSessionContext 1.6 vtable in place. Secure RTP
  support is added through a separate optional interface so an old provider fails closed instead
  of becoming ABI-unsafe.

## Current state being replaced

Today Iris owns `SrtpContext` / `SrtpSession` and links libSRTP when
`IRIS_ENABLE_SRTP` is enabled. QCA DTLS exports verified keys into that session, Iris decrypts
incoming SRTP/SRTCP, `BundleRouter` routes authenticated RTP/RTCP, and the Psi adapter exchanges
plain `RtpPacket` objects with psimedia.

This also leaves a known BUNDLE gap: authenticated compound RTCP that spans several contents is
recognized as shared RTCP, but Iris has no group-level media ingress and drops it. The new API
must provide group-level secure ingress rather than reproducing this limitation as one SRTP
context per endpoint.

### Verified implementation touchpoints

- Iris `src/xmpp/xmpp-im/jingle-rtp-srtp.{h,cpp}` contains both packet crypto and the
  `PacketTransport` contract. `jingle-ice.cpp` supplies the DTLS profile list;
  `jingle-rtp.cpp` owns `Pad::RoutingPrivate` and the shared-RTCP drop. Updating only
  `jingle-rtp-media.cpp` or the ICE datagram callback is insufficient.
- psimedia `gstprovider/gstrtpsessioncontext.h` has separate audio/video bridges, and
  `gstprovider/rtpsessionbridge.cpp` creates a GStreamer `rtpsession` for each bridge.
  A shared crypto object alone therefore does not solve shared RTCP/session semantics.
- psimedia's root `CMakeLists.txt` selects demo/plugin/test builds and sets up pkg-config;
  `gstprovider/CMakeLists.txt` builds the static `gstprovidersrc` library and its tests.
  Neither currently discovers or links libSRTP.
- Provider headers exist in both psimedia `psimedia/psimediaprovider.h` and Psi
  `plugins/include/psimediaprovider.h`. With `BUILD_PSIPLUGIN=ON`, psimedia uses the
  external PsiPluginsApi include directory; with it OFF, it uses its local header.
  Both must contain the same additive interface before testing either build mode.
- Psi `src/psimedia/` wraps the provider; `src/avcall/psimediajingle.cpp` implements the
  adapter. The integration workflow currently checks out psimedia `master` in two places
  and explicitly forces `IRIS_ENABLE_SRTP` in a test harness.

## Phase 1: psimedia secure RTP API and libSRTP engine

Repository: `psi-im/psimedia`.

### Phase 1 implementation status

Current psimedia branch: `ai/jingle-srtp-psimedia`.

- **Phase 1 gate: CLOSED.** psimedia `bdb48d9` is fully green with the deterministic
  bounded-queue regression, and Psi cross-repository integration run `35648124713` is green
  against the matching psimedia branch with the real plugin API/subproject build. The secure
  provider/session IID boundary, direct libSRTP ownership, BUNDLE/unbundled multi-association
  model, Qt5 compatibility, no-SRTP build, queue fencing and plugin ABI are all covered.
- **Follow-up after the closed gate:** psimedia now accepts an empty endpoint table as a
  transactional "clear all routes" operation without destroying staged crypto state. This is
  required by the direct Iris group-level teardown path; a regression was added.

- **Passed:** additive provider/session secure-RTP IID scaffolding is mirrored in both public
  provider headers without changing Provider/RtpSessionContext 1.6 vtables.
- **Passed:** direct libSRTP association engine builds with `PSIMEDIA_ENABLE_SRTP=ON`; profile
  probing, RTP/SRTCP round trips, tamper/authentication failure, replay preservation across
  identical-key epoch advancement, invalid-key reset and SSRC bounds are covered.
- **Passed:** `PSIMEDIA_ENABLE_SRTP=OFF` configures and builds successfully.
- **Passed:** backend-neutral authenticated RTP BUNDLE router is ported to psimedia; RTCP is
  validated for group ingress rather than routed/split per endpoint.
- **Passed:** one GStreamer `rtpsession` can carry Opus 48 kHz and VP8 90 kHz in one group,
  consume compound RTCP once, and remove video while audio continues without replacing the
  RTP session. This closes the implementation gate in step 5.
- **Passed at psimedia `de697c2`:** the group-oriented secure public API, dual-interface
  secure media session, direct libSRTP two-peer/group regressions, outgoing MID stamping,
  SRTP-enabled provider suite, and explicit `PSIMEDIA_ENABLE_SRTP=OFF` build all pass. This is
  the current known-green Phase 1 baseline.
- **Implemented, CI pending on the current psimedia head:** `SecureRtpGroup` composes libSRTP
  with the shared RTP group boundary so plaintext RTP/RTCP stays inside psimedia. The public
  `SecureRtpSessionContext/1.0` is now protected-only and group-oriented: endpoint route
  metadata, association activation/invalidation, protected ingress and protected egress callback.
  The secure factory returns `GstSecureRtpSessionContext`, which is the same codec/device media
  context as Provider/1.6 plus the new secure IID. A normal legacy session does not advertise
  that IID. Encoder-thread RTP is queued onto the Qt owner thread before group routing/crypto.
- **Passed:** the group bridge stamps the negotiated MID RTP header extension on outgoing
  bundled RTP before the shared rtpsession/libSRTP boundary; the regression covers audio/video
  MID values and a fresh audio packet after video membership removal.
- **Passed through the Qt6/plugin stages at psimedia `e77531e`:** provider build/tests,
  `PSIMEDIA_ENABLE_SRTP=OFF`, and `BUILD_PSIPLUGIN=ON` against the matching Psi branch's
  real plugin API header all pass with the multi-association implementation. That run failed
  only in the final Qt5 compile because `securertpgroup.cpp` relied on a transitive QDebug
  include; `7a30ad0` adds the explicit include.
- **Passed in the Qt6 provider regressions at `e77531e`:** the secure media session owns an
  `associationId -> SecureRtpGroup` map. Bundled endpoints share one ID/group; unbundled
  audio/video use independent group/rtpsession/libSRTP state inside the same codec/device
  session. Endpoint metadata carries associationId, protected packets carry it, and the public
  regression activates/invalidates two associations independently.
- **Fully green at psimedia `490c79b`:** Qt6 provider regressions, `PSIMEDIA_ENABLE_SRTP=OFF`,
  `BUILD_PSIPLUGIN=ON` against the matching Psi header, and the Qt5 compatibility build all pass
  with the multi-association secure media implementation and authenticated route-drop semantics.
- **Implemented, CI pending:** the encoder-to-secure-group handoff is a bounded producer queue
  (256 packets, 512 KiB, 1 s age) with route-generation and crypto-epoch fencing. A deterministic
  regression now fills the queue without running the Qt event loop, verifies the byte bound,
  age eviction/accounting, and full clearing on route-generation change. Runtime errors carry
  associationId+epoch as well as the error code.
- **Passed at psimedia `490c79b`:** authenticated RTP/SRTCP which passes libSRTP but fails
  the negotiated group route is reported as nonfatal `InvalidPacket`, distinct from
  authentication/replay/fatal backend failures; the regression and explicit Qt5 QDebug fixes
  pass in the full matrix.
- **Still required before Phase 2:** finish plugin-unload/callback lifetime coverage and
  subproject/SDK packaging coverage. External libwebrtc/webrtcbin peer tests remain a
  cross-repository gate after Psi negotiates/passes the XEP-0294 MID extension instead of
  rejecting header extensions.

1. Add a new optional secure-RTP session interface with its own Qt interface IID instead of
   appending virtual methods to `RtpSessionContext/1.6`. Add provider-level discovery/factory
   access as an optional interface too, so profiles can be queried before opening a session.
   Discover via `qobject()`/`qobject_cast` and a new IID; freeze existing virtual methods,
   public value-type layouts and packet meaning. Define object ownership, thread affinity,
   callback disconnection and plugin-unload lifetime explicitly.
2. Expose backend SRTP profile capability independently of codec capability.
3. Model an explicit secure association object:
   - one object per authenticated DTLS association/BUNDLE group;
   - configure with negotiated profile plus directional master key/salt;
   - opaque association identity plus generation/epoch on every packet and callback;
   - define synchronous key-copy ownership and wipe temporary/backend-owned key buffers;
     avoid ordinary implicitly shared key copies and never log keys;
   - idempotent teardown and hard invalidation;
   - protected packet ingress and protected packet egress;
   - no XMPP/Jingle types in the psimedia API.
4. Implement the association with libSRTP directly.
   - separate inbound/outbound libSRTP state as required by the library;
   - support RTP and RTCP/SRTCP;
   - reject null encryption;
   - map authentication/replay/library failures to stable backend errors;
   - support multiple SSRCs in one association;
   - preserve ROC, replay windows and SRTCP indices on repeated activation with identical
     keys; media pause/restart or route changes must not recreate crypto with the same keys;
   - serialize libSRTP access, bound SSRC growth and packet queues, validate packet sizes
     and reserve protection trailer space; failed authentication must not teach routes;
   - define library initialization/lifetime across concurrent sessions and plugin unload;
   - distinguish dropped malformed/authentication/replay packets from fatal backend failure;
     handle key/index exhaustion without resetting counters under unchanged keys.
5. Add group-level post-authentication routing inside the media backend or immediately adjacent
   to the secure association. Routing metadata is supplied by the Psi adapter in backend-neutral
   terms; psimedia must not learn Jingle content names. Reuse/extract the existing tested
   `BundleRouter` logic rather than maintain two independent routing implementations.
   Pass opaque endpoint IDs, negotiated MID/header-extension IDs, directional SSRCs and
   payload types, with atomic route updates independent of crypto epochs.

   **Selected architecture:** one group-owned GStreamer RTP session for each negotiated
   BUNDLE group; unbundled media retains separate sessions. Extract session ownership from
   the per-media `RtpSessionBridge` into a group bridge, retaining endpoint codec branches.
   Feed authenticated RTCP to the group session once, before endpoint routing; do not require
   a unique content match for RTCP. Reuse the router's RTP mapping/validation and tests, but
   replace its per-content RTCP delivery policy with group ingress.

   A shared `rtpsession` fits the current bridge; `rtpbin` with one session index is the
   reference alternative if its SSRC demux/jitter/feedback facilities are needed. One `rtpbin`
   containing two independent session indices does not implement the selected grouping.
   Aggregate outgoing RTP with per-stream caps/clock rates and route incoming media by
   authenticated SSRC/MID and negotiated payload mapping. Keep a common RTCP source registry,
   scheduling, CNAME/synchronization policy and per-source statistics. Include SR/RR report
   blocks, SDES, BYE and supported negotiated feedback.

   **Gate status: passed in psimedia provider regressions.** The prototype uses one
   `rtpsession` for Opus at 48 kHz and VP8 at 90 kHz, accepts group compound RTCP once, and
   removes the video endpoint while audio continues on the same RTP session. Keep `rtpbin`
   as the fallback only if later SSRC demux/jitter/feedback requirements expose a real gap.
   If `rtpbin` is adopted, remove or bypass duplicate jitter buffering currently in
   `bins.cpp`. Do not instantiate `webrtcbin` in production: ICE/DTLS/Jingle remain in Iris.
6. Integrate libSRTP at the group bridge's network boundary. GStreamer continues to see plain
   RTP/RTCP; protection happens once per outgoing datagram and authentication once per incoming
   datagram, before any dispatch. Preserve bounded queues and thread/lifetime handling from
   `RtpSessionBridge`. Group RTP state and crypto state have distinct lifetimes: a transport
   replacement must not accidentally reset surviving streams' RTP sequence/timestamp state;
   stopping one endpoint must not destroy the shared association.
7. Build policy:
   - discover/link system libSRTP where the build is system-oriented;
   - allow the SDK/bundled build to link the project-selected libSRTP;
   - do not add a second crypto abstraction or QCA hooks;
   - do not require the GStreamer SRTP plugin;
   - add an explicit psimedia SRTP build switch (proposed `PSIMEDIA_ENABLE_SRTP`, default ON):
     ON requires a usable libSRTP 2.x dependency at configure time; OFF builds with no secure
     profiles. Test both modes and avoid silently advertising unavailable crypto;
   - discover `libsrtp2` via pkg-config for system builds, with SDK/config/find-library
     support for MSVC/cross builds; normalize to a target consumed by `gstprovidersrc`;
   - carry required link dependencies through the static provider library to both plugin
     targets and tests, without libSRTP types in public headers. Test standalone and
     subproject builds, both `BUILD_PSIPLUGIN` modes, and SDK runtime library deployment.
8. Tests:
   - profile/key validation;
   - RTP and RTCP protect/unprotect round trips;
   - tamper/authentication failure;
   - replay rejection;
   - multiple SSRCs sharing one context;
   - audio+video/BUNDLE routing;
   - compound/shared RTCP ingress;
   - rekey/epoch invalidation;
   - stop/restart and callback lifetime;
   - bounded queue/backpressure behavior remains intact;
   - identical-key reactivation retains replay rejection, RTP sequence rollover, independent
     SSRC state, invalid-key reconfiguration, SSRC limits and key/index exhaustion;
   - old plugin/new host and new plugin/old host loading with no ABI/layout changes;
   - profile probing reflects working RTP and RTCP contexts, including builds without GCM.

The first implementation may support only the profiles that both libSRTP and the selected build
backend report. Capability advertisement, not a plaintext fallback, handles missing profiles.

## Phase 2: Iris exports authenticated SRTP material instead of doing packet crypto

Repository: `psi-im/iris`.

1. Keep `Dtls`/QCA responsible for:
   - `use_srtp` negotiation;
   - selected profile;
   - fingerprint authentication;
   - directional SRTP keying material export (DTLS client/server roles, not Jingle
     initiator/responder roles, determine send/receive keys).
   Preserve RFC 5764 exporter label/layout and per-profile key/salt lengths; for example,
   AES-CM uses 16/14 bytes and AEAD AES-128/256-GCM uses 16/12 and 32/12 bytes.
   Preserve separate RTP/RTCP policies: the SHA1-32 profile still uses an 80-bit SRTCP tag.
   Keep the initial contract limited to no MKI unless MKI negotiation and packet handling
   are implemented end to end.
2. Introduce an Iris-owned, backend-neutral SRTP binding description containing:
   - negotiated profile;
   - local and remote master key/salt;
   - a security epoch/generation;
   - invalidation notification.
   QCA-specific TLS types must not leak into psimedia.
3. Change packet-capable RTP transport so Iris moves protected SRTP/SRTCP datagrams between ICE
   and the media backend. Iris must not call libSRTP `protect` / `unprotect`.
4. Keep RFC 7983 classification in Iris. STUN/TURN and DTLS continue to terminate at the existing
   transport/DTLS layer; SRTP/SRTCP is handed to the authenticated media binding as opaque
   protected media. Keep negotiated RTP/RTCP-mux validation and RFC 5761 discrimination
   (including payload-type restrictions) distinct from RFC 7983's first-byte classification;
   clear headers are not authenticated routing evidence. Preserve the current mux-only
   transport scope; separate-component RTCP support is not implied by this migration.
5. Preserve BUNDLE association identity: every content sharing one `IceConnection`/DTLS
   association must receive the same secure-association identity and epoch.
6. Rework the RTP media integration boundary so the media session has group-level packet ingress.
   The backend must receive enough negotiated route metadata to dispatch authenticated plain
   packets after libSRTP processing. Do not create one SRTP engine per `MediaEndpoint`.
7. Capability selection becomes:
   ```
   QCA DTLS-SRTP profiles
              intersect
   MediaProvider secure-RTP profiles
   ```
   Codec/media-type capability remains a separate check. Compute and snapshot this
   intersection before starting DTLS and pass it into the actual `use_srtp` configuration
   for both roles. Advertising the intersection while negotiating all QCA profiles can
   select a profile the backend cannot use. Validate the selected profile again at activation.
   Replace both the Iris manager capability gate and the ICE DTLS setup call sites; preserve
   DTLS-only SCTP/data-channel operation when secure RTP is unavailable.
8. As part of this Phase 2 refactor (not as a later compatibility cleanup):
   - remove `SrtpContext`/`SrtpSession` packet crypto from Iris;
   - remove direct libSRTP includes/linking;
   - remove `IRIS_ENABLE_SRTP`, `FindSRTP`, pkg-config/CMake SRTP consumer dependencies;
   - update `docs/jingle-rtp-design.md` to describe the new ownership.
9. Preserve supported transport-replace semantics: a successor DTLS association may be staged
   while the old one is alive, but no packet/key material may cross epochs. The inspected
   `RTP::Application::isTransportReplaceEnabled()` allows replacement only before `Connecting`.
   Retain this guard until the separate active-call recovery gate below is implemented/tested;
   successful pre-Connecting replacement is not evidence of live-call ICE restart.

### Phase 2 live implementation status

- **Iris packet crypto removed:** `SrtpContext`, `SrtpSession`, libSRTP linkage/configuration,
  `FindSRTP.cmake`, pkg-config SRTP metadata and the Iris crypto-vector test are gone.
- **Transport boundary implemented:** `SecureRtpAssociation` owns only verified DTLS key export,
  opaque association identity/epoch and RFC 7983/5761 classification. ICE sends/receives protected
  SRTP/SRTCP unchanged; BUNDLE shares the `IceConnection` token and unbundled transports do not.
- **Media boundary implemented:** per-endpoint plaintext packet callbacks are gone. `MediaSession`
  owns protected packet IO, transactional endpoint route metadata and association configure/invalidate.
  The old Iris plaintext `BundleRouter` and its tests were removed.
- **SCTP protected:** the DTLS-SRTP regression now routes encrypted DTLS records through
  `SecureRtpAssociation` while running an SCTP echo. The sanitizer transport suite was green on
  this architecture before the remaining stale RTP tests were ported.
- **Regression migration in progress:** `dtlssrtp`, `iceownership`, `bundleice`,
  `rtpcapsselection`, `rtpapplication`, `bundlesignaling`, `bundlemedia` and `icertp`
  have been moved off the removed Iris crypto/per-endpoint API. `bundlemedia` now verifies backend-
  owned BUNDLE routing using the association/endpoint metadata passed by Iris; `icertp` verifies
  opaque protected transport plus backend PT validation rather than reintroducing Iris packet parsing.
- **Current gate:** wait for the first complete qca3 Jingle compile/runtime pass on the fully ported
  tests, fix actual runtime/lifetime failures, then update the Psi adapter to the secure psimedia API.

### Phase 2 implementation shape

The chosen Iris boundary is a direct replacement; no compatibility layer for the short-lived
pre-migration Iris RTP API is required.

- `MediaEndpoint` is per-content codec negotiation only. Remove its packet-I/O methods.
- `MediaProvider` exposes secure-RTP profiles so the manager can intersect the actual backend
  with `Dtls::supportedSRTPProfiles()` before DTLS starts.
- `MediaSession`, not `MediaEndpoint`, owns the protected group packet API. It accepts
  transactional endpoint-route metadata, configures/invalidates multiple opaque associations,
  receives protected packets tagged with association+epoch, and exposes one protected egress
  callback for the whole media session.
- An opaque association token maps one-to-one to an Iris `IceConnection`/DTLS association.
  Bundled endpoints share the token; unbundled audio/video use distinct tokens. The token has
  session-local identity only and does not expose Jingle/BUNDLE classes to the backend.
- Iris keeps key material in secure storage through the DTLS/export boundary and the Psi adapter
  performs only the synchronous conversion required by the psimedia ABI. No key is logged or
  retained in ordinary signaling objects.
- Remove `SrtpContext`, `SrtpSession`, per-endpoint packet routing and direct libSRTP from Iris
  as part of Phase 2 rather than retaining a transitional production path.

Incoming flow becomes `ICE -> RFC7983/RFC5761 classification -> protected MediaSession ingress
-> psimedia/libSRTP authentication -> group router/rtpsession`. Iris must not route on unauthenticated
SSRC/PT/MID. Outgoing flow is the exact reverse; psimedia returns association+epoch with every
protected packet and Iris only validates that the association/epoch still names the active DTLS
transport before writing to ICE.

**SCTP/DataChannel invariant:** data channels remain entirely in Iris. `SecureRtpAssociation`
is an RFC 7983 front-door mux, not an owner of DTLS application data: DTLS records are always
forwarded unchanged into QCA `Dtls`; decrypted DTLS application records continue through
`Dtls::readyRead -> SCTP::Association::writeIncoming()`, while SCTP egress remains
`SCTP::Association -> Dtls::writeDatagram() -> ICE`. A DTLS-SRTP regression now runs an SCTP
echo while SRTP negotiation is active and routes the encrypted DTLS records through
`SecureRtpAssociation`, so moving SRTP crypto to psimedia cannot silently break datachannels.

Iris tests must continue to cover DTLS verification, key direction/profile selection, BUNDLE
association ownership, atomic transport replacement and stale-epoch rejection, but packet
cipher/authentication vector tests move to psimedia.

## Phase 3: Psi adapter wires the two sides together

Repository: `psi-im/psi`.

1. Mirror/consume the new optional psimedia secure-RTP interface without breaking loading of an
   older Provider 1.6 plugin.
2. Extend `PsiMediaJingleCapabilities` so native RTP is advertised only when:
   - a usable codec/media backend exists;
   - psimedia reports secure-RTP support;
   - Iris/QCA and psimedia have at least one common SRTP profile;
   - an Iris packet transport is available.
3. Replace the current plain packet contract:
   ```
   psimedia plain RTP -> Iris SrtpSession -> ICE
   ICE -> Iris SrtpSession -> psimedia plain RTP
   ```
   with:
   ```
   psimedia RtpSessionBridge -> psimedia libSRTP -> Iris protected transport
   Iris protected transport -> psimedia libSRTP -> RtpSessionBridge
   ```
4. The adapter creates/reuses one psimedia secure association for each Iris DTLS/BUNDLE
   association identity. Audio/video endpoints sharing BUNDLE bind to the same object.
5. Feed route metadata from negotiated Iris RTP descriptions into the psimedia association
   without exposing Jingle classes through the psimedia plugin ABI.
6. Teardown order:
   - stop/detach media callbacks;
   - invalidate the matching secure association epoch;
   - discard queued protected/plain packets from that epoch;
   - release the transport binding;
   - allow the old DTLS/ICE association to die.
   A stale callback must not attach itself to a replacement transport.
7. An old psimedia plugin with no secure interface means native Jingle RTP is unavailable. Do
   not fall back to sending plain RTP.
8. Update the psimedia integration workflow so it builds/tests the matching psimedia branch and
   exercises real Opus/VP8 packet flow through libSRTP.

## Compatibility decision and evidence

The user has verified **audio calls only** between Psi and Conversations. This is a positive
manual interoperability baseline; missing version/fixture metadata does not negate the result.
The tested Conversations/library versions, call directions and network paths are not yet recorded.
Video, audio+video BUNDLE, shared RTCP, restart and other libwebrtc/webrtcbin clients remain
unverified. Do not generalize this audio result to them or to every version of the library.
The older plan's blanket “Conversations interop open” status is superseded by this narrower result.

Internal session ownership is not signaled to the peer. The architecture change should preserve
negotiated codecs, payload numbers, extensions, ICE/DTLS setup and SRTP profiles. BUNDLE changes
where session state is held, not audio/video encoding or their independent RTP clock rates.
A protocol RTP session need not correspond to one C++ object in every implementation.

### What other implementations do

Source inspection on 2026-09-21 (implementation evidence, not a live interoperability result):

| Stack | Observed implementation | Consequence for this migration |
| --- | --- | --- |
| Conversations, `17f0fe07606dc09cf414105c516d39021e3638d7` | Android libwebrtc `149.0.0` in `build.gradle`; `WebRTCWrapper` uses Unified Plan and RTCP mux policy NEGOTIATE; `SessionDescription` preserves group metadata and emits rtcp-mux for grouped RTP | Keep Jingle/SDP semantics and the working negotiated feature subset. The inspected checkout is not necessarily the user's installed version. |
| libwebrtc, `3710345e71ad3a7c1f4ed4efef152fd4fc24869a` | `SrtpTransport` authenticates RTCP before delivery; `Call::DeliverRtcpPacket` distributes it to stream handlers; `RTCPReceiver` filters report blocks/feedback by registered/local SSRC | Coordinated RTCP fan-out is legitimate. A blanket ban on dispatch to multiple handlers would be incorrect; independently broadcasting into the existing PsiMedia sessions does not establish equivalent coordination. |
| GStreamer, `2059bdb118de50417c2911adcd1cb230f23b61e5` | `webrtcbin` BUNDLE uses a shared TransportStream/session index, funnels outgoing RTP into that rtpbin session and connects its common incoming RTP/RTCP path | Use this group-session organization as the PsiMedia model while retaining Iris transport and direct libSRTP. |

The libwebrtc source is the Chromium-family implementation reference, not proof of identical
internals in every browser or in Conversations' pinned Android library. Browser wire behavior
is governed by WebRTC RTP requirements; test Chromium and Firefox separately. Likewise generic
GStreamer pipelines need not implement WebRTC; `webrtcbin` is the relevant WebRTC reference.

### Handshake and packet compatibility contract

Compatibility is defined by a negotiated wire profile and a tested peer/version, not by the
peer's library name. Two clients using the same library may expose different codecs, extensions,
SDP/Jingle mappings or transport policies. Standards are the contract; peer captures supply
regressions, not permission to bypass authentication or accept inconsistent negotiations.

| Boundary | Required validation against external peers |
| --- | --- |
| Jingle/application signaling | Discovery, JMI where used, full-JID selection, offer/answer subset, content names/group references, senders and cancellation. Test native Jingle separately from an SDP-only peer harness. |
| ICE/STUN/TURN | Candidate/credential mapping, trickled candidates before/after answer, role/nomination behavior, connectivity and consent freshness, direct vs relay paths, IPv4/IPv6 where available. Keep application capture consent distinct from network consent. |
| DTLS | Negotiated setup roles, version/cipher/certificate-algorithm overlap in the actual QCA and peer builds, fingerprint/hash verification, retransmitted/fragmented handshake flights and bounded timeout. No media before verified keys; a fingerprint mismatch must fail. |
| DTLS-SRTP | Actual use_srtp overlap, selected profile/key direction and RTP/SRTCP policies. Test asymmetric profile lists, both DTLS roles and no-common-profile failure, rather than assuming one peer's default. |
| RTP | Codec payload format/fmtp, negotiated PTs and clock rates, SSRC/source changes, sequence rollover, header extension IDs where negotiated, and MTU after SRTP overhead. XML PT changes must reach the packetizer/depacketizer. |
| RTCP/SRTCP | Periodic SR/RR and report blocks, SDES/CNAME, BYE, shared compound packets and negotiated reduced-size feedback; verify applied statistics/encoder actions, not merely packet arrival. |
| BUNDLE/mux | One group transport/crypto/session, independent streams, accepted subsets, conflicting parameters, refusal and unbundled operation where supported. Do not infer negotiated grouping from discovery alone. |

Initial codec fixtures use Opus audio and VP8 video when supported by both peers. Additional
codecs require their own packet/fmtp checks (in particular H.264 profiles/packetization and
RTX apt/base-codec relationships). Simulcast/SVC, SFU and screen sharing are outside this first
one-to-one interoperability target. Unsupported optional offers may be declined without
breaking accepted audio; a required unsupported capability yields an explicit failure.

Use the existing ICE and ExternalServiceDiscovery paths. TURN-over-TCP/TLS connectivity is
not ICE-TCP candidate support. Record which paths work and which are unsupported; do not claim
all WebRTC transport requirements from a successful UDP call. No byte-stream IBB/S5B fallback
for this packet RTP backend.

### External peer matrix and evidence

All rows use Psi's own Iris/QCA + psimedia/libSRTP implementation. External peer libraries are
never dependencies of the implementation under test. A test harness must not terminate and
re-originate media/DTLS to hide an incompatibility: translation is limited to signaling.

| Peer | Current evidence | Required coverage |
| --- | --- | --- |
| Conversations | User-verified audio only; exact version/direction/network metadata pending | Repeat audio baseline; both initiation directions, A/V, BUNDLE where negotiated, direct and TURN. |
| Another client using libwebrtc | Not verified; select and pin client and library builds | Audio and A/V in both directions, handshake/profile/packet gates; do not reuse Conversations' result as proof. |
| Client using GStreamer webrtcbin | Not verified; select and pin client plus GStreamer/plugin builds | Same audio/A/V gates, including shared RTCP and negotiated feedback. |
| Chromium and Firefox test peers | Not verified | Additional independent wire tests via a signaling-only harness; not substitutes for native-Jingle client tests. |
| Psi <-> Psi | Older plan records audio CI evidence on historical commits | Rerun real provider audio/A/V on the migration triplet; no inference from old green jobs. |

For each execution record client/library versions, Psi/Iris/psimedia SHAs, QCA/provider,
libSRTP and GStreamer versions, actual loaded media plugin, initiator/DTLS roles, server/TURN,
negotiated profile/codecs/features, expected/observed results and artifacts. Use separate statuses
for pass, failure, unsupported, skipped and not tested. Keep sanitized stanzas and synthetic packet
fixtures; never retain exporter/private keys, credentials or user media in diagnostic artifacts.

Native-Jingle audio/A/V with pinned clients from **both library families**, plus the Conversations
audio regression, are required for the expanded interoperability claim. Library harness results
are useful independently but cannot prove a client's discovery/JMI/SDP-to-Jingle behavior.
A missing peer leaves that row pending; it does not prevent independent implementation/unit work.

### Wire compatibility and regression gates

- Capture the working Psi/Conversations baseline before implementation: exact app/build versions,
  audio-only result, actual initiator/network paths, negotiated group/mux/profile, codec/PT,
  SSRC/MID/extension and feedback mappings. Store sanitized signaling fixtures without secrets.
- Preserve audio-only and unbundled operation as well as negotiated BUNDLE. Do not force all
  calls into one session or require the peer to change its offer to match the internal refactor.
  The psimedia secure-session API must therefore support more than one simultaneous association
  inside the higher-level media session: one group object per negotiated BUNDLE group and one
  group object per standalone/unbundled RTP content.
- Do not make MID mandatory as part of moving crypto: the current Psi adapter rejects answer
  header extensions/feedback it cannot implement. Retain signaled SSRC and unambiguous PT
  routing for the existing subset. Enable MID or other extensions only with complete negotiated
  send/receive support; reject ambiguous mappings rather than guess audio vs video.
- Handle compound RTCP covering both media and standalone feedback when reduced-size RTCP is
  negotiated. Process known blocks when an otherwise valid compound contains unknown types;
  do not reject the whole packet merely because a block has no endpoint route.
- Advertise PLI/FIR/NACK/RTX or congestion-control extensions only when their complete paths
  work. GStreamer's `rtpfunnel` can route upstream events by SSRC, but PsiMedia's appsrc/appsink
  boundaries need explicit event/callback plumbing back to the correct encoder. A common
  RTCP session alone does not implement retransmission or bitrate adaptation.
- With mixed audio/video, verify independent clock rates and sequence spaces, RTCP statistics,
  synchronization, and that removing/muting video leaves audio and shared crypto alive.
  Check feedback under controlled packet loss for each feature actually negotiated.
- Require live Psi <-> Conversations tests in both call directions for audio and audio+video,
  including BUNDLE where negotiated, direct ICE and TURN relay. Compare the captured baseline;
  verify more than audible sound/visible video (ongoing RTCP and loss recovery where supported).
- Add the external client and browser/library peer tests in the matrix above. Browsers/webrtcbin
  do not provide Jingle signaling themselves: use a test SDP/Jingle bridge or suitable XMPP
  peer and identify what is covered by media-only vs full Jingle tests.
- Pin GStreamer/plugin versions in CI; features from current upstream are not automatically
  available in the supported runtime. Do not add a dependency on newer properties without
  runtime feature checks or an explicit supported-version decision.

### Existing SRTP + SCTP coexistence regression

Iris already has `tests/jingle/dtlssrtp.cpp` / CTest `jingle_dtlssrtp`; mixed use is not
starting from zero. With QCA3, `IRIS_ENABLE_SRTP` and `IRIS_ENABLE_JINGLE_SCTP` enabled,
`runPair()` establishes authenticated DTLS with exported SRTP keys, exercises SRTP/SRTCP,
then calls `runDataChannel()` on the same DTLS pair while the SRTP contexts remain alive.
A real SCTP DataChannel exchanges and echoes a 32768-byte payload; afterward the test
checks that an already received SRTP packet is still rejected as a replay. The same helper
also covers DataChannel operation on DTLS without SRTP negotiation.

This is in-process crypto/transport coexistence coverage with real QCA/libSRTP/SCTP, not
just a mock. It does not run a continuous concurrent audio/video stream, production ICE/Jingle
BUNDLE membership, multiple DataChannels or mixed content-removal/FT-drain scenarios.
The user recalls a successful run; source coverage was verified during this plan update,
but no new run or exact historical CI result is claimed here. Check enabled build options:
the mixed section is conditional on both SRTP and SCTP test macros and QCA3.

During migration, retain Iris's DTLS/SCTP-only coverage and port the mixed SRTP/SRTCP +
DataChannel/replay scenario to cross-repository integration using the psimedia secure
association. Do not delete the mixed test when removing Iris's `SrtpContext`, or introduce a
psimedia dependency into Iris unit tests. Rerun with both features enabled on the migration
triplet; add continuous interleaving and group-lifetime tests only for the uncovered behavior.

## Retained lifecycle and validation requirements

These are requirements carried forward from the older calls plan, not assertions that every
listed scenario is already implemented or tested on the migration branches.

- Keep `PRtpPacket::Type` semantics (`Rtp=0`, `Rtcp=1`) and synchronized public wrappers;
  no portOffset/ICE topology in media packet APIs. Per-content signaling Transport objects
  retain their ACK/action state while group ownership stays in Iris's session-local registry.
- Preserve GroupPlan/ConnectionRegistry/ConnectionMembership/ConnectionGroupTransaction.
  Sharing is never keyed only by peer JID. A replacement transaction changes only its group;
  unrelated groups and two simultaneous calls to the same peer remain independent.
- Membership lifetime, ICE generation, DTLS/crypto epoch, routing revision, media-operation ID
  and DataChannel stream lifetime are distinct. Check the relevant identities at each callback;
  a reused object address is not proof of identity.
- Activation requires negotiated/apply completion and authenticated media readiness, in either
  arrival order, exactly once. Preparation does not capture devices; local consent and negotiated
  senders gate capture/transmit. Mute/hold is local policy; peer status never grants consent.
  Receive/RTCP reporting may continue while the local RTP sender is disabled.
- Keep async operation cancellation/deadlines, device hotplug/source switching and provider
  runtime-error handling. Errors revoke callbacks/writers before notification; callbacks may
  synchronously delete their owner. Guard continuations and do not call a cleaned-up provider.
  No nested event loops or direct Jingle calls from media worker threads.
- Preserve owned XML lifetime across queued callbacks. Retain DOM-lifetime, direction-policy,
  teardown and sanitizer regressions; a codec/crypto refactor must not undo signaling hardening.
- Align bridge/producer/consumer clock, base-time and running-time. Copying GstBuffer PTS
  between pipelines does not by itself establish correct sender-report timing or A/V sync.
- Test actual outgoing RTP PT/SSRC, incoming probation, parsed periodic RTCP reports and updated
  statistics. A forced send-rtcp request, callback count or successful async pipeline start is
  not sufficient. Capture subsequent GStreamer bus errors through the existing error path.
- Run synthetic source -> encoder/payloader -> real RtpWorker/group bridge -> secure transport
  -> receive/decode; assert decoded output and queue bounds. Synthetic media is a test facility,
  never a fallback for a normal call's unavailable microphone/camera.
- Preserve ICE/SCTP, S5B and IBB file-transfer regressions, including transport replacement,
  truncated-source failure and buffered tail. Application completion/reference release is
  distinct from Connection destruction; the transport owner retains required async drain/close.
  Call hangup can discard media, but must not discard another application's required data.
- Reuse existing CI and report executed tests on exact SHAs. Keep source review, unit tests,
  real GStreamer/production synthetic tests, server-mediated calls and external live interop
  as separate evidence. Run relevant ASan/UBSan and separate race checks where applicable;
  ordinary compile/packaging success cannot replace runtime evidence.

### Follow-up features retained from the old roadmap

These need their own implementation and evidence before being advertised; they must not be
silently reported as delivered by the SRTP ownership change.

- **Active-call ICE restart/migration:** keep the current pre-Connecting guard until a shared
  restart coordinator, transactional accept/reject/timeout/glare/rollback and cancellation
  are tested. Decide explicitly whether DTLS is retained or recreated; retained keys require
  retained replay/index state. Test stale candidates/ACKs, network switch and TURN expiry;
  account disconnect and media-path loss have distinct policies and bounded recovery.
- **Mixed group lifecycle and multiple DataChannels, beyond the existing coexistence test:**
  independent content/stream removal and drain,
  final-member release, RTP hangup while FT drains and FT close while RTP continues. Reuse
  existing SCTP glue; do not rewrite the vendored mediasoup core without a demonstrated need.
- **JMI/multiple devices:** extend existing code only for verified gaps. Cover peer+proposal
  ID+state correlation, selected full JID, crossed proposals, cancel/late proceed, duplicate
  Carbons/MAM/reconnect messages, two responders, pending bounds and account cleanup. Pin the
  XEP-0353 profile and actual peer behavior rather than copying the old proposed class layout.
- **Expanded video/control:** camera add/accept/reject/remove without interrupting audio,
  both-to-audio-only answer, mute/hold and loss/keyframe recovery. Complete negotiated MID,
  feedback, retransmission and congestion behavior as needed by the selected peer profile;
  preserve the functioning minimal subset while adding features. Do not use content-modify
  or description-info as arbitrary codec renegotiation.
- **Broader network coverage:** relay-only/IP exposure policy, IPv4/IPv6, UDP-restricted paths,
  TURN credential refresh and candidate/gathering limits through existing discovery/ICE code.
  Each claimed network mode needs a real peer test or an explicitly recorded limitation.

## Phase 4: dependency and packaging cleanup

1. Iris-only builds/packages no longer require/link `libsrtp2`; SRTP is a psimedia dependency.
   Full Psi bundles still ship it when required by the media plugin. Retain the existing
   Windows SDK libSRTP payload and DLL collection until equivalent plugin packaging is verified.
2. psimedia CI/package jobs install the libSRTP development package where system linking is used.
3. Self-contained builds should use one crypto backend where practical:
   - QCA/OpenSSL and libSRTP/OpenSSL use the same OpenSSL build;
   - retain the crypto backend actually supported by the selected libSRTP version/package
     (for example internal crypto, OpenSSL or NSS); do not assume arbitrary QCA backends
     such as mbedTLS are also libSRTP backends;
   - do not introduce QCA-based libSRTP cipher hooks unless a future platform genuinely needs
     a crypto backend libSRTP cannot use directly.
4. Verify Windows, Linux and macOS packaging plus the supported Android build before removing the
   old Iris SRTP option from release documentation.

## Cross-repository execution order

Development happens on matching `ai/jingle-srtp-psimedia` branches in all three repositories.

1. **psimedia:** land the additive secure-RTP interface, libSRTP engine and tests.
2. **Iris:** add the key-export/protected-datagram binding and capability plumbing while
   retaining a buildable existing adapter path during development. Each association uses
   exactly one crypto owner; no double protection or plaintext network fallback.
3. **Psi:** update both provider header copies/wrappers and wire the new production adapter.
   After the new path passes integration, remove Iris packet crypto, old binding APIs and
   libSRTP dependencies, then update Psi's Iris pin and affected test harnesses together.
4. Run the cross-repository integration workflow against exact commit SHAs, including both
   psimedia checkouts and all generated harnesses. Move crypto vectors to psimedia without
   dropping transport/classifier tests from Iris. Record the tested triplet; rebuild Iris
   without libSRTP installed and check its exported CMake/pkg-config dependencies.
5. Live test at minimum:
   - Psi <-> Psi audio;
   - audio+video;
   - BUNDLE audio+video;
   - transport teardown/new-call restart and supported pre-Connecting replacement;
   - the mandatory Conversations baseline and external media-peer matrix described above.
   Missing peer/device access leaves the corresponding live gate unverified, not passed.
6. Merge only after the three branch heads are mutually compatible, all required CI is green,
   and the user explicitly instructs merging. Implementing this plan does not authorize a
   merge to `master`. Publish additive API headers before dependent plugin builds.

The final squash commit in each repository must describe the resulting behavior and tests rather
than preserving the intermediate development commit history.

## Completion criteria

The migration is complete when:

- Iris has no libSRTP link/runtime dependency and does no RTP packet encryption/decryption.
- Iris still owns Jingle, ICE, BUNDLE, DTLS authentication and SRTP key export through QCA.
- psimedia owns libSRTP packet protection and one secure context per DTLS/BUNDLE association.
- GStreamer receives/emits plain RTP/RTCP through the group bridge, with one RTP session per
  negotiated BUNDLE group and independent codec branches.
- Psi advertises native RTP only for an actually usable common secure profile.
- no plaintext fallback exists;
- shared BUNDLE RTCP is handled through group-level media ingress rather than intentionally
  dropped;
- rekey/transport replacement cannot consume packets or keys from the previous security epoch;
- all three repositories are green together and the user-verified Conversations audio path
  remains working on the recorded migration build;
- the external native-Jingle client matrix for libwebrtc and webrtcbin has passed before claiming
  expanded audio/video interoperability; pending rows and follow-up features remain explicit.

## Protocol references

Checked against the published texts during validation:

- [XEP-0320](https://xmpp.org/extensions/xep-0320.html): Jingle fingerprints and DTLS setup.
- [RFC 5764, sections 4–5](https://www.rfc-editor.org/rfc/rfc5764): profiles, exporter,
  key direction and SRTP/SRTCP processing.
- [RFC 7714](https://www.rfc-editor.org/rfc/rfc7714.html): AEAD profile parameters.
- [RFC 7983](https://www.rfc-editor.org/rfc/rfc7983) and
  [RFC 5761](https://www.rfc-editor.org/rfc/rfc5761.html): datagram and RTP/RTCP mux rules.
  [RFC 9443](https://www.rfc-editor.org/rfc/rfc9443.html) updates demultiplexing for QUIC;
  adding QUIC is outside this migration.
- [RFC 9143, sections 9 and 11](https://www.rfc-editor.org/rfc/rfc9143.html): current BUNDLE
  RTP/RTCP and DTLS reference (obsoletes RFC 8843).
- [libSRTP upstream](https://github.com/cisco/libsrtp): library/build model; select and test
  the actual packaged 2.x version rather than infer capabilities from installed headers alone.

Implementation references for the compatibility decision:

- [Conversations WebRTCWrapper](https://codeberg.org/iNPUTmice/Conversations/src/commit/17f0fe07606dc09cf414105c516d39021e3638d7/src/main/java/eu/siacs/conversations/xmpp/jingle/WebRTCWrapper.java),
  [SessionDescription](https://codeberg.org/iNPUTmice/Conversations/src/commit/17f0fe07606dc09cf414105c516d39021e3638d7/src/main/java/eu/siacs/conversations/xmpp/jingle/SessionDescription.java),
  [build.gradle](https://codeberg.org/iNPUTmice/Conversations/src/commit/17f0fe07606dc09cf414105c516d39021e3638d7/build.gradle).
- [libwebrtc Call](https://github.com/webrtc-mirror/webrtc/blob/3710345e71ad3a7c1f4ed4efef152fd4fc24869a/call/call.cc),
  [RTCPReceiver](https://github.com/webrtc-mirror/webrtc/blob/3710345e71ad3a7c1f4ed4efef152fd4fc24869a/modules/rtp_rtcp/source/rtcp_receiver.cc),
  [SrtpTransport](https://github.com/webrtc-mirror/webrtc/blob/3710345e71ad3a7c1f4ed4efef152fd4fc24869a/pc/srtp_transport.cc).
- [GStreamer webrtcbin source](https://github.com/GStreamer/gstreamer/blob/2059bdb118de50417c2911adcd1cb230f23b61e5/subprojects/gst-plugins-bad/ext/webrtc/gstwebrtcbin.c),
  [rtpfunnel](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpfunnel.html),
  [rtpbin](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpbin.html).
- [RFC 8834](https://www.rfc-editor.org/rfc/rfc8834.html): browser WebRTC RTP requirements,
  including RTCP extensibility and negotiated reduced-size feedback.

Additional references used when carrying forward the earlier calls requirements:

- [XEP-0176](https://xmpp.org/extensions/xep-0176.html): Jingle ICE-UDP signaling.
- [XEP-0338](https://xmpp.org/extensions/xep-0338.html): Jingle grouping.
- [XEP-0293](https://xmpp.org/extensions/xep-0293.html) and
  [XEP-0294](https://xmpp.org/extensions/xep-0294.html): negotiated feedback/header extensions.
- [XEP-0353](https://xmpp.org/extensions/xep-0353.html): JMI peer/device negotiation.
- [RFC 8835](https://www.rfc-editor.org/rfc/rfc8835.html) and
  [RFC 8827](https://www.rfc-editor.org/rfc/rfc8827.html): external WebRTC transport/security
  requirements; these references do not assert full implementation of every WebRTC feature.
