# SRTP ownership migration plan

## Goal

Move SRTP/SRTCP packet protection out of Iris and into psimedia while keeping signaling,
transport topology and DTLS ownership in Iris.

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

- ICE, BUNDLE membership, transport replacement and DTLS lifetime remain owned by Iris.
- QCA remains the DTLS implementation boundary. Iris still verifies the peer fingerprint before
  exposing any SRTP keying material.
- There is no plaintext RTP fallback. If there is no common DTLS-SRTP profile or no secure media
  backend, native RTP capability is not advertised.
- One libSRTP state machine belongs to one authenticated DTLS association/BUNDLE group, not to
  an individual audio/video endpoint.
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

## Phase 1: psimedia secure RTP API and libSRTP engine

Repository: `psi-im/psimedia`.

1. Add a new optional secure-RTP session interface with its own Qt interface IID instead of
   appending virtual methods to `RtpSessionContext/1.6`.
2. Expose backend SRTP profile capability independently of codec capability.
3. Model an explicit secure association object:
   - one object per authenticated DTLS association/BUNDLE group;
   - configure with negotiated profile plus directional master key/salt;
   - explicit generation/epoch identity;
   - idempotent teardown and hard invalidation;
   - protected packet ingress and protected packet egress;
   - no XMPP/Jingle types in the psimedia API.
4. Implement the association with libSRTP directly.
   - separate inbound/outbound libSRTP state as required by the library;
   - support RTP and RTCP/SRTCP;
   - reject null encryption;
   - map authentication/replay/library failures to stable backend errors;
   - support multiple SSRCs in one association.
5. Add group-level post-authentication routing inside the media backend or immediately adjacent
   to the secure association. Routing metadata is supplied by the Psi adapter in backend-neutral
   terms; psimedia must not learn Jingle content names.
6. Integrate the secure association with the existing `RtpSessionBridge` packet boundary.
   GStreamer continues to see plain RTP/RTCP. libSRTP sits between the bridges and Iris network
   transport.
7. Build policy:
   - discover/link system libSRTP where the build is system-oriented;
   - allow the SDK/bundled build to link the project-selected libSRTP;
   - do not add a second crypto abstraction or QCA hooks;
   - do not require the GStreamer SRTP plugin.
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
   - bounded queue/backpressure behavior remains intact.

The first implementation may support only the profiles that both libSRTP and the selected build
backend report. Capability advertisement, not a plaintext fallback, handles missing profiles.

## Phase 2: Iris exports authenticated SRTP material instead of doing packet crypto

Repository: `psi-im/iris`.

1. Keep `Dtls`/QCA responsible for:
   - `use_srtp` negotiation;
   - selected profile;
   - fingerprint authentication;
   - directional SRTP keying material export.
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
   protected media.
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
   Codec/media-type capability remains a separate check.
8. Once the new path is exercised by Psi:
   - remove `SrtpContext` packet crypto from Iris;
   - remove direct libSRTP includes/linking;
   - remove `IRIS_ENABLE_SRTP`, `FindSRTP`, pkg-config/CMake SRTP consumer dependencies;
   - update `docs/jingle-rtp-design.md` to describe the new ownership.
9. Preserve transport-replace semantics: a successor DTLS association may be staged while the
   old one is alive, but no packet/key material may cross epochs.

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

## Phase 4: dependency and packaging cleanup

1. Iris packages no longer install/link `libsrtp2`; SRTP is a psimedia dependency.
2. psimedia CI/package jobs install the libSRTP development package where system linking is used.
3. Self-contained builds should use one crypto backend where practical:
   - QCA/OpenSSL and libSRTP/OpenSSL use the same OpenSSL build;
   - system distributions may use OpenSSL, LibreSSL, NSS, mbedTLS or another backend selected by
     their packages;
   - do not introduce QCA-based libSRTP cipher hooks unless a future platform genuinely needs
     a crypto backend libSRTP cannot use directly.
4. Verify Windows, Linux and macOS packaging plus the supported Android build before removing the
   old Iris SRTP option from release documentation.

## Cross-repository execution order

Development happens on matching `ai/jingle-srtp-psimedia` branches in all three repositories.

1. **psimedia:** land the additive secure-RTP interface, libSRTP engine and tests.
2. **Iris:** land key-export/protected-datagram media binding and remove its libSRTP packet
   implementation.
3. **Psi:** update Iris/psimedia references and switch the production adapter to the new path.
4. Run the cross-repository integration workflow against the exact branch/commit triplet.
5. Live test at minimum:
   - Psi <-> Psi audio;
   - audio+video;
   - BUNDLE audio+video;
   - transport teardown/restart;
   - Conversations interoperability where available.
6. Merge only after the three branch heads are mutually compatible and all required CI is green.

The final squash commit in each repository must describe the resulting behavior and tests rather
than preserving the intermediate development commit history.

## Completion criteria

The migration is complete when:

- Iris has no libSRTP link/runtime dependency and does no RTP packet encryption/decryption.
- Iris still owns Jingle, ICE, BUNDLE, DTLS authentication and SRTP key export through QCA.
- psimedia owns libSRTP packet protection and one secure context per DTLS/BUNDLE association.
- GStreamer receives/emits plain RTP/RTCP through the existing RTP session bridge.
- Psi advertises native RTP only for an actually usable common secure profile.
- no plaintext fallback exists;
- shared BUNDLE RTCP is handled through group-level media ingress rather than intentionally
  dropped;
- rekey/transport replacement cannot consume packets or keys from the previous security epoch;
- all three repositories are green together and live media interoperability remains working.
