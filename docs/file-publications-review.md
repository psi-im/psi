# Review of file publications and SFS attachments

Reviewed together: PR #1100 (`90e0c1dd`) and its dependent PR #1101
(`cdc26653`), based on master `3157a4f5`.

## Findings and corrections

### Authorization must identify the file, not just allow the request

The previous serving gate accepted a request if any supplied hash belonged to
an account's served file. The manager then searched the profile-wide cache
using every hash supplied by the requester. A request combining an authorized
hash with another file's hash could therefore select unrelated cached bytes.
This weakness predates these PRs, but their catalog boundary must not retain it.

`servedRecord` now resolves a request against one explicitly served publication
of the receiving account. All supplied hashes must belong to that publication.
Only its recorded hashes reach cache lookup; the fallback path belongs to that
same publication, without received hash aliases. Unknown, mixed and unauthorized
requests retain the same refusal behavior. This is application authorization;
no alternate XMPP signaling or transfer implementation was added.

### SFS parsing must not prepare JIDs on the SQL worker

`HistoryMediaRefs::link` restored SFS on the history worker. A Jingle publication
source constructs a JID through Iris's shared, unsynchronized StringPrep cache.
Consequently routine message appends could race normal GUI-side JID preparation.

The GUI now writes a versioned, value-only attachment index into the event's
existing protocol snapshot. It contains eligible ordinals and canonical
fingerprints, not another copy of descriptors or serving decisions. The worker
uses this index inside the event transaction without parsing SFS sources.
Legacy SIMS/OOB backfill remains worker-safe. Old development SFS records lacking
an index are prepared on the GUI during the startup upgrade, before the worker
runs. This compatibility step remains synchronous startup work.

### Migration failures must preserve the only legacy catalog

Opening or reading a damaged legacy database could be mistaken for a successful
empty import. The caller could then delete its input. Optional old tables are
now distinguished from failed queries, read failures roll back the import mark,
and an unreadable source remains retryable.

The global legacy directory previously used only the payload-import marker.
Retirement now requires separate payload and publication completion markers for
every profile. A successful payload import cannot retire an unimported catalog.

A profile-local completion marker outside `files.sqlite` also prevents replaying
old serving decisions after that rebuildable database is lost. History can
reconstruct received publications; it cannot silently grant serving permission.
Inputs are retired only after successful import and completion marking.

### Synchronization must retain failed work and avoid foreground history scans

The synchronizer advanced its link cursor even when an insert failed. Some
query errors looked like empty results, and deletion errors were ignored.
There was no retry until another full startup reconciliation. Repeated timer
restarts could also postpone synchronization under sustained message traffic.

History scans and linkage checks now use the existing ordered history worker,
returning bounded value snapshots. GUI-side reconstruction and the existing
file-database ownership remain. Missing catalog entries are inserted in one
transaction per batch, rather than one transaction per attachment. Failed reads,
inserts or sweeps retain their cursors and retry with a delay; repeated error
logging is suppressed. An erase invalidates older in-flight snapshots.

### A changed stored attachment needs a changed link

`INSERT OR IGNORE` kept the old publication when a stored projection changed
its attachment, removed it, or changed the descriptor at the same ordinal.
The catalog therefore continued describing the previous file/source.

Links now store canonical attachment fingerprints. Identical replay preserves
publication IDs without issuing inserts or advancing the scan sequence;
changed or removed attachments retire their old links inside
the history transaction. A committed retirement schedules reconciliation.
Explicit monotonically increasing scan positions avoid hidden-rowid reuse.
The localized live-history body is indexed as actually stored.

### Compatibility deduplication must preserve sources and retrieval policy

SIMS/SFS duplicates discarded the SFS sources and its manual-fetch disposition.
A shared weak hash could also suppress a distinct file despite conflicting
strong hashes. A hashless SFS HTTP source could bypass filtering through its
OOB compatibility copy.

Compatible announcements now union their sources and retain the stricter manual
policy. Deduplication requires a common strong digest, compatible sizes and
non-conflicting hashes. Insecure hashless SFS URLs do not become automatic OOB
attachments; their fallback body remains available. This follows the existing
extraction/rendering boundary rather than adding storage policy to Iris.

### Republishing must capture sources before replacing its publication

The initial serving update replaced the old own publication before the manager
looked up reusable upload URLs. This discarded the very URLs meant to be reused,
particularly when history logging was disabled. Sources are now captured first.
Successful HTTP-only publishing also records the explicit own publication.
A failed initial catalog write cannot register a locally served publication.

## Architecture and limits

- History owns message/attachment linkage and transactional changes. The catalog
  owns account-scoped announcements and explicit serving permission. The
  synchronizer is the single bridge between them.
- Protocol/JID reconstruction stays on the GUI. History SQL, including the new
  synchronization scans, shares the existing worker and operation ordering.
- Catalog/cache APIs still perform GUI-thread SQL on the existing file database.
  Batching reduces commit overhead; this is not a claim that all file I/O is
  asynchronous. Moving the physical file store belongs to the subsequent stage.
- Existing runtime items still retain sources until restart. Historical cache
  metadata/sidecars may also retain obsolete, now-ignored descriptor fields;
  this change stops using/writing them, not physically scrubbing old copies.
  These limits must be addressed before claiming complete local deletion.
- Encrypted SFS sources, sources attached by later messages, sending SFS and
  publication-addressed views remain deferred. No master merge is part of this
  review.

## Verification

Five initial regression cases failed against the submitted code: manual policy
and source loss, conflicting-file deduplication, unreadable legacy import,
changed stored attachments, and retry after a catalog write failure.

Additional coverage exercises mixed-hash authorization, real Jingle refusal,
legacy-directory retirement barriers, lost-database import behavior, canonical
link stability and monotonic positions, pending SFS metadata, removed attachments,
localized bodies, batch rollback, failed sweep recovery, stale callbacks, threaded SFS writes and
old-development SFS migration. The Qt-only history worker tests also cover SQL
errors and valid empty results for the new read operations.

Local validation after the fixes:

| Check | Qt 5.15.18 | Qt 6.10.2 |
| --- | --- | --- |
| Full application build | Passed | Passed |
| File-sharing/history/reply CTest selection, excluding the two socket tests | 19 suites passed | 19 suites passed |
| SQL-worker regressions (`mam_history_sql_read`) | Passed | Passed |
| Real Jingle session and responder | Skipped: no local DTLS/SCTP backend | Both passed |
| Real application, temporary profile, migration and restart (`serverless_ui`) | Not enabled in this build | Passed |

Socket integration tests were run with local socket access; the sandboxed run
could not bind ICE ports or start the test peer. These tests use temporary data.
Qt 6.4 API compatibility is preserved in the code; the locally installed Qt 6
version is 6.10.2.

Publishing orchestration was checked by code inspection and the existing
publication/GUI integration coverage; there is no dedicated simulated HTTP-upload
fixture for the new capture-before-replace ordering. Real disk latency, power
loss and physical erasure were not simulated.

## Protocol references

Reviewed against [SFS 0.3.1 (XEP-0447)](https://xmpp.org/extensions/xep-0447.html),
[SIMS (XEP-0385)](https://xmpp.org/extensions/xep-0385.html),
[Jingle file transfer (XEP-0234)](https://xmpp.org/extensions/xep-0234.html) and
[Jingle publications (XEP-0358)](https://xmpp.org/extensions/xep-0358.html).
