# Serverless local-network accounts

Accounts → Serverless configures an opt-in XEP-0174 account alongside normal
XMPP accounts. Enabling it adds **Local network** to the existing roster; direct
chats use the normal chat windows, message handlers, plugin hooks and local EDB
history. It does not connect to an XMPP server or query MAM.

## Settings

- **Display name** is advertised in the DNS-SD TXT record. Changing it updates
  presence without changing the account's persistent ID or conversation history.
- **Local address** shows the `user@machine` identity. It is generated when the
  account is created, retained in the profile, and may receive a suffix if another
  participant owns the same DNS name. The display name is independent of it.
- **Network interface** defaults to automatic selection: a running multicast
  interface, preferring Ethernet/Wi-Fi and IPv4, excluding loopback and point-to-
  point links. An explicit selection never silently falls back to another network.
  Loopback is selectable explicitly for isolated testing. Refresh enumerates the
  currently available interfaces. Changing the interface reconnects the account.
- **Listening port** defaults to an ephemeral port selected by the OS. A fixed
  port can be used with a firewall rule. Outbound peer sockets bind to the same
  local address as the listener and multicast discovery.
- **Connect automatically** controls startup independently of whether the account
  is enabled. The ordinary account status menu connects and disconnects it.

Disabling the feature withdraws discovery records and closes peer streams. The
account configuration and local history are retained. Status changes are DNS-SD
TXT updates; peer disappearance makes the contact offline. Invisible status takes
this account offline because presence is publicly discoverable on the link.

## Security and current scope

This implementation currently uses unauthenticated, unencrypted TCP streams.
The settings explain that limitation before enabling the feature. DNS-SD names
are not proof of identity. A TLS checkbox would be misleading until TLS negotiation
and a usable certificate/trust policy are implemented, so it is deliberately absent.
Remote-control commands configured for authenticated server accounts are disabled
for serverless accounts. XML streams reject third-party stanza addresses.

OMEMO's PEP-based device discovery, server roster editing, subscriptions, blocking
lists, groupchat bookmarks and server administration are unavailable here. Other
plugin encryption methods retain the normal Client hooks. This feature covers one
selected link/address family at a time. Interface migration after a network change
requires reconnecting; it does not poll network interfaces or merge multiple links.
Buddy-icon DNS records and TLS/SASL negotiation remain future work.

## Verification

Iris `linklocal_service` uses real loopback multicast and TCP with two complete
`XMPP::Client` instances. It verifies discovery, names, presence, message addresses,
message corrections, goodbye and reuse of a Client after reconnecting. The existing
`linklocal_stream` and two-process `linklocal_mdns_tcp` tests cover stream validation
and interoperability with the command-line example. Run socket tests outside a
network-restricted sandbox.

`-DPSI_ENABLE_SERVERLESS_UI_TEST=ON` builds a test-only style plugin that drives
these settings in the actual Psi executable. `ctest -R '^serverless_ui$'` runs it
on two fresh-process launches with an isolated profile and loopback networking,
checking account creation, discovery, a round-trip chat with a separate Iris process,
SQLite records, enable/disable and persisted name/interface/account ID. The fixture
starts with all existing accounts disabled, guarding the profile history store
against the previous missing-EDB startup crash.
It runs in offscreen mode under a separate D-Bus session and does not modify the
user's profile or desktop. The Ubuntu Qt 6 workflow enables this test.

For a manual application smoke test use an isolated `PSIDATADIR`, select the loopback
interface, enable the feature in Accounts → Serverless, open a discovered contact,
and verify incoming/outgoing messages and EDB history after reopening the chat.
Never use an existing personal profile for automated GUI tests.
