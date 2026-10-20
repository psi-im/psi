-- Disposable loopback test server; never use these credentials for real accounts.
admins = {}
interfaces = { "0.0.0.0" }
c2s_ports = { 5222 }
s2s_ports = {}
c2s_require_encryption = false
allow_unencrypted_plain_auth = true
authentication = "internal_hashed"
storage = "internal"
data_path = "/var/lib/prosody"
pidfile = "/tmp/prosody.pid"
modules_enabled = { "roster"; "saslauth"; "disco"; "ping"; "private"; "pep"; "mam"; }
modules_disabled = { "tls"; "s2s"; }
default_archive_policy = true
archive_expires_after = "1w"
log = { info = "*console"; error = "*console"; }
VirtualHost "localhost"
Component "conference.localhost" "muc"
    modules_enabled = { "muc_mam"; }
    if os.getenv("MAM_TEST_INDEX") == "1" then
        modules_enabled = { "muc_mam"; "mam_test_index"; }
    end
    storage = { muc_log = "sql"; }
    sql = { driver = "SQLite3"; database = "/var/lib/prosody/muc-log.sqlite"; }
    storage_archive_item_limit = 100000
    mam_include_total = true
    muc_log_by_default = true
    muc_log_all_rooms = true
    muc_room_default_persistent = true
    muc_room_default_public = true
    muc_room_default_history_length = 4
    max_history_messages = 4
