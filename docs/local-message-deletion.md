# Device-local message deletion

This implements stage 3 of [the shared-file design](local-message-deletion-design.md).
The initial scope is stored direct conversations in the SQLite backend, including
serverless direct chats. Public rooms and MUC private messages are excluded.

**Delete locally** removes a logical message and its known corrections from this
profile. It sends no message retraction and leaves the remote archive alone. Cached
files, user originals and exports are kept. This is logical deletion, not secure
erasure of SQLite pages, WAL files, backups or copies in other messages.

## Ownership and transaction

`EDB` exposes receiver-scoped asynchronous deletion and suppression checks;
`EDBSqLite` puts them in the existing ordered request queue. The existing history
worker is the only runtime SQL owner. No blocking connection, nested event loop or
second persistence worker is introduced.

`HistoryArchive::deleteLocalMessage` resolves the selected event's revision
component inside its account and direct conversation. A live row without a history
ID can use its unique scoped client ID; absent or ambiguous identity is refused.
ID-less stored messages use their actual EDB event ID, never text or timestamps.
Conflicting client, origin or authority-qualified stanza IDs cannot authorize
deletion of another component.

One transaction:

1. Preserves identity-only suppression aliases.
2. Removes the component's `message_media_refs`.
3. Clears plaintext, subject, language, protocol snapshot, resource and saved
   forwarded XML, retaining event/revision identities and archive topology.
4. Marks the events `MessageState::LocalDeleted`.

On commit, the facade notifies all materialized consumers and wakes publication
reconciliation. A failure rolls back aliases, text and attachment ownership
together; the displayed row remains. The operation is idempotent.

`FileSharingPublicationSync` already retries bounded orphan sweeps and reconciles
at startup. It removes non-serving publications after their history links vanish,
including after a crash. An extra cleanup journal would duplicate this ownership
state when bytes are retained. Stage 6 will need an outbox to remember the optional
byte-removal choice after the publication itself is gone.

## Identity and ingress

`message_suppressions` contains only account, conversation, direction, ID kind,
authority and opaque ID value. Client IDs, origin IDs, assigned stanza IDs and MAM
UIDs have separate namespaces; a peer cannot turn one kind into another by choosing
an equal string. An assigned stanza ID is used only under the account archive's
authority. UIDs are never sorted, and body/date similarity never proves identity.

Client and origin aliases reuse `message_state_refs`. `message_stanza_ids` is a
derived index of observed assigned IDs with their authorities, allowing scoped
lookup and collision checks without scanning message bodies. Previously observed
IDs survive a replay that omits the corresponding extension. These indexes retain
identity after payload scrubbing; they are not a second logical-message graph.

An ingress hit extends suppression with the newly learned aliases and scrubs
already stored deliveries discovered through them. Repairing an undecoded delivery
requires the same authenticated archive envelope. A correction reusing an unrelated
already decoded message's ID is dropped without learning its conflicting aliases or
certifying the cache position; it cannot erase that message. A correction therefore remains
hidden even if its original arrives later, and a replacement MAM UID cannot refill
the old text when another known identity proves the relationship.

The application checks ordinary live deliveries before display/logging, restored
unread events before presentation, and queued OpenPGP messages before decryption.
It checks MAM identities before archive decryption and checks the resulting
identity again afterward. The latter matters when encrypted corrections reveal
their targets only after decryption, including when logging is disabled. Iris
performs live OMEMO decryption before delivering a message to Psi; suppression runs
at that application boundary. Protocol parsing and storage policy remain separate.

Every durable append and MAM store repeats the check inside its transaction. A
message captured before deletion cannot bypass the committed decision by waiting
in a write queue. Normal ingress misses are read-only identity probes on the same
worker and do not flush a pending append batch. A positive probe crosses the
commit barrier before learning aliases or notifying the GUI.

Suppression persists independently of contact/event rows, so ordinary history
erasure does not silently restore a locally deleted message. As with every
identity-based policy, an entirely new delivery carrying no known identifiers
cannot be proven to be the deleted message. In particular, old-server ID-less
messages are not matched by equal text/time; unrelated deliveries remain separate.

## Reads and presentation

`HistoryArchive::Entry::suppressed` is an explicit archive position without a
renderable/decryptable payload. Cursor and indexed pages retain their UIDs, event
counts and coordinates. A fully suppressed page is a valid cache hit, including
offline; no fake stanza or empty-XML cache miss is needed. `nativeArchivePage`
passes every UID to the topology model and renders only visible entries.

Local pagination, legacy get/find/count, resolved revisions and plaintext/reply
reconstruction exclude deleted content. After commit, native views cancel stale
requests, remove matching rows in the correct direction, invalidate search caches
and refresh reply previews. The history dialog resets its search and refreshes;
the account also removes matching unread queue events and persists that update.
Quotes already contained in a different message remain that message's content.

The context action is offered only by a backend supporting this operation and
only in direct chats. Storage refusal is reported without prematurely hiding the
row. Message retraction continues to use its separate existing action.

## Migration and compatibility

History schema version 0.6 adds suppression records and the assigned-ID index.
The existing startup migration gate and worker progress remain in use. Existing
development `LocalDeleted` rows seed suppression from their surviving identity
indexes and archive links; plaintext is not needed to rebuild them. Protocol/JID
objects never cross the SQL worker boundary.

Qt 5 and Qt 6.4 API compatibility is retained. File-cache schema and serving
decisions keep their existing ownership: deleting a message does not revoke an
explicit own serving publication.

Stage 2's remaining lifetime limits also apply: runtime file items can retain
sources until restart, and historical obsolete descriptor fields may remain in
old cache metadata. This feature makes no claim of physical metadata/byte erasure.
The common leased file store, bounded item lifetime, optional byte reclamation
and share-management UI remain stages 4–6.

## Regression coverage

- Storage: future corrections, correction before original, new MAM UIDs,
  authority/kind/direction/account/conversation isolation, conflicting aliases,
  unavailable keys, idempotence, transactional attachment release and rollback.
- Worker: deletion commits earlier appends, late queued appends stay suppressed,
  search returns no deleted rows, and an ordinary ingress miss preserves batching.
- MAM: fully hidden offline pages, decryption avoidance for known identities and
  post-decryption suppression with logging disabled.
- Native widget: a pending/refused operation retains its row; the commit removes
  only the selected direction and never requests XEP-0424.
- Real application: `serverless_ui_local_deletion` drives the actual context
  action, receives a replay and a future correction from a separate Iris peer,
  checks both databases, and restarts the profile. The peer rejects any retraction.

Local validation:

| Check | Qt 5.15.18 | Qt 6.10.2 |
| --- | --- | --- |
| Full application and affected test-target builds | Passed | Passed |
| File-sharing/history/reply CTest selection | 19 suites passed | 19 suites passed |
| Native widget and SQL-worker suites | Both passed | Both passed |
| Real Jingle session/responder | Skipped: local DTLS/SCTP backend unavailable | Both passed |
| Real application/profile restart | Not enabled in this build | Passed |
| Real application local deletion/replay/restart | Not enabled in this build | Passed |

The history suite includes 71 passing checks, including old-draft migration and
its schema-version update. The application integration tests use temporary
profiles and localhost peers. The OpenPGP queue changes were built and inspected;
a real GPG decryption scenario was not exercised locally. Physical erasure,
power-loss recovery and the future file-store reclamation stages were not tested.

## Protocol references

Reviewed against [XEP-0308 1.2.1](https://xmpp.org/extensions/xep-0308.html),
[XEP-0359 0.7.0](https://xmpp.org/extensions/xep-0359.html),
[XEP-0313 1.1.3](https://xmpp.org/extensions/xep-0313.html), and
[XEP-0424 0.5.0](https://xmpp.org/extensions/xep-0424.html). Local suppression is
application storage policy and does not advertise or send a retraction protocol.
