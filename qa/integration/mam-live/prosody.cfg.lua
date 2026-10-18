-- Ephemeral loopback-only CI server: exercise real XEP-0313 archive storage.
prosody_user = os.getenv("USER")
data_path = assert(os.getenv("PROSODY_DATA_DIR"), "PROSODY_DATA_DIR is required")
pidfile = assert(os.getenv("PROSODY_PID_FILE"), "PROSODY_PID_FILE is required")
allow_registration = false
authentication = "internal_hashed"
storage = "internal"

c2s_interfaces = { "127.0.0.1" }
c2s_ports = { 5222 }
c2s_require_encryption = false
allow_unencrypted_plain_auth = true

modules_enabled = { "roster"; "saslauth"; "disco"; "ping"; "mam"; }
modules_disabled = { "tls"; "s2s"; }
default_archive_policy = true
archive_expires_after = "1w"

log = { debug = "*console"; }
VirtualHost "localhost"
