-- Ephemeral CI-only XMPP server for Jingle integration.
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

modules_enabled = {
    "roster";
    "saslauth";
    "disco";
    "ping";
}
modules_disabled = {
    "tls";
    "s2s";
}

log = {
    debug = "*console";
}

VirtualHost "localhost"
