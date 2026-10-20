#!/usr/bin/env python3
"""Exercise a freshly created GCMainDlg against a loopback Prosody MUC.

Uses a disposable Psi profile and the real executable, not a widget/MessageView
mock. The test server must enable muc_mam, private XML storage and PLAIN on
loopback; the room history limit must be four (see muc-prosody.cfg.lua).
"""
import argparse
import base64
import hashlib
import os
from pathlib import Path
import re
import socket
import sqlite3
import subprocess
import time
from xml.sax.saxutils import escape


class FixtureClient:
    def __init__(self, port):
        for attempt in range(50):
            try:
                self.socket = socket.create_connection(("127.0.0.1", port), timeout=10)
                break
            except ConnectionRefusedError:
                if attempt == 49:
                    raise
                time.sleep(.1)
        self.buffer = b""
        self.send("<stream:stream to='localhost' xmlns='jabber:client' "
                  "xmlns:stream='http://etherx.jabber.org/streams' version='1.0'>")
        self.until(b"</stream:features>")
        auth = base64.b64encode(b"\0user01\0ci-pass-01").decode()
        self.send(f"<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='PLAIN'>{auth}</auth>")
        self.until(b"<success")
        self.buffer = b""
        self.send("<stream:stream to='localhost' xmlns='jabber:client' "
                  "xmlns:stream='http://etherx.jabber.org/streams' version='1.0'>")
        self.until(b"</stream:features>")
        self.send("<iq type='set' id='bind'><bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'>"
                  "<resource>mam-fixture</resource></bind></iq>")
        self.iq("bind")
        self.send("<presence/>")

    def send(self, text):
        self.socket.sendall(text.encode())

    def until(self, marker):
        while marker not in self.buffer:
            data = self.socket.recv(65536)
            if not data:
                raise RuntimeError("fixture stream closed")
            self.buffer += data
        end = self.buffer.index(marker) + len(marker)
        result, self.buffer = self.buffer[:end], self.buffer[end:]
        return result

    def iq(self, identifier):
        pattern = re.compile(rb"<iq\b[^>]*\bid=['\"]" + identifier.encode() + rb"['\"][^>]*>")
        while True:
            found = pattern.search(self.buffer)
            if found:
                end = found.end() if found.group().endswith(b"/>") else self.buffer.find(b"</iq>", found.end()) + 5
                if end >= found.end():
                    result, self.buffer = self.buffer[found.start():end], self.buffer[end:]
                    if b"type='error'" in result or b'type="error"' in result:
                        raise RuntimeError("fixture IQ failed: " + result.decode())
                    return result
            self.buffer += self.socket.recv(65536)

    def seed(self, room, protocol_events, body_count):
        self.send(f"<presence to='{room}/Seeder'><x xmlns='http://jabber.org/protocol/muc'>"
                  "<history maxstanzas='0'/></x></presence>")
        self.until(b"110")
        self.send(f"<iq to='{room}' type='set' id='config'><query xmlns='http://jabber.org/protocol/muc#owner'>"
                  "<x xmlns='jabber:x:data' type='submit'/></query></iq>")
        self.iq("config")
        for i in range(body_count + protocol_events + 4):
            payload = ("<body>Runtime message %d</body>" % i
                       if i < body_count or i >= body_count + protocol_events
                       else "<received xmlns='urn:xmpp:receipts' id='fixture-target'/>"
                            "<store xmlns='urn:xmpp:hints'/>")
            self.send(f"<message to='{room}' type='groupchat' id='fixture-{i}'>{payload}</message>")
        # The last echo is a storage/order barrier; no arbitrary persistence sleep.
        self.until(f"fixture-{body_count + protocol_events + 3}".encode())
        self.send(f"<iq type='set' id='bookmarks'><query xmlns='jabber:iq:private'>"
                  f"<storage xmlns='storage:bookmarks'><conference jid='{room}' autojoin='true' name='Runtime'>"
                  "<nick>Reader</nick></conference></storage></query></iq>")
        self.iq("bookmarks")

    def close(self):
        self.send("</stream:stream>")
        self.socket.close()


def profile(root, port, profile_name):
    directory = root / "profiles" / profile_name
    directory.mkdir(parents=True)
    jid, password = "user01@localhost", "ci-pass-01"
    encoded = "".join(f"{ord(c) ^ ord(jid[i % len(jid)]):04x}" for i, c in enumerate(password))
    values = dict(name=("QString", "Runtime"), id=("QString", "mam-runtime"),
                  jid=("QString", jid), password=("QString", encoded),
                  host=("QString", "127.0.0.1"), port=("int", str(port)),
                  resource=("QString", "psi-runtime"), ssl=("QString", "no"),
                  **{"allow-plain": ("QString", "always")})
    for key, value in {"enabled": True, "auto": True, "use-host": True, "log": True,
                       "automatic-resource": False, "require-mutual-auth": False,
                       "reconn": False, "keep-alive": True}.items():
        values[key] = ("bool", str(value).lower())
    entries = "".join(f'<{k} type="{typ}">{escape(value)}</{k}>' for k, (typ, value) in values.items())
    (directory / "accounts.xml").write_text(
        '<accounts xmlns="http://psi-im.org/options"><accounts><a0>' + entries + '</a0></accounts></accounts>')
    (directory / "options.xml").write_text('''<options xmlns="http://psi-im.org/options"><options>
<keychain><enabled type="bool">false</enabled></keychain>
<p2p><bytestreams><listen-port type="int">0</listen-port></bytestreams></p2p>
<ui><tabs><use-tabs type="bool">false</use-tabs></tabs><muc><hide-on-autojoin type="bool">false</hide-on-autojoin>
<size type="QSize"><width>720</width><height>900</height></size></muc></ui>
<muc><bookmarks><auto-join type="bool">true</auto-join></bookmarks></muc>
</options></options>''')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--psi", type=Path, required=True)
    parser.add_argument("--port", type=int, default=55323)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--protocol-events", type=int, default=400)
    parser.add_argument("--bodies", type=int, default=80)
    parser.add_argument("--probe", type=Path, help="Qt generic probe build directory")
    parser.add_argument("--mode", choices=("initial", "indexed", "fallback"), default="initial")
    parser.add_argument("--check-metadata-upgrade", action="store_true",
                        help="Reopen an old EDB cache without RSM metadata and verify one bounded bootstrap")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    with args.psi.open("rb") as executable:
        digest = hashlib.file_digest(executable, "sha256").hexdigest()
    print(f"MUC_RUNTIME_BINARY={args.psi.resolve()} sha256={digest}", flush=True)
    room = "runtime-" + str(time.time_ns()) + "@conference.localhost"
    client = FixtureClient(args.port)
    client.seed(room, args.protocol_events, args.bodies)
    client.close()
    profile_name = f"mam-runtime-{os.getpid()}"
    profile(args.output / "profile", args.port, profile_name)
    env = dict(os.environ, PSIDATADIR=str((args.output / "profile").resolve()),
               QT_QPA_PLATFORM="offscreen", QT_LOGGING_RULES="psi.history.debug=true",
               QT_MESSAGE_PATTERN="%{category}: %{message}")
    if args.probe:
        env["QT_PLUGIN_PATH"] = str(args.probe.resolve()) + os.pathsep + env.get("QT_PLUGIN_PATH", "")
        env["QT_QPA_GENERIC_PLUGINS"] = "mam-history-probe"
        env["MAM_RUNTIME_PROBE_MODE"] = args.mode
        env["MAM_RUNTIME_MESSAGE_COUNT"] = str(args.bodies + args.protocol_events + 4)
    runs = ("cold", "cached", "metadata-upgrade", "cached-upgrade") if args.check_metadata_upgrade else ("cold", "cached")
    for run in runs:
        if run == "metadata-upgrade":
            database = args.output / "profile" / "profiles" / profile_name / "history" / "history.db"
            with sqlite3.connect(database) as db:
                db.execute("DELETE FROM archive_snapshots")
                db.execute("UPDATE archive_pages SET result_count=NULL,first_index=NULL")
                db.execute("DELETE FROM archive_index_pages")
        log = args.output / (run + ".log")
        with log.open("w") as output:
            process = subprocess.Popen([str(args.psi.resolve()), f"--profile={profile_name}"],
                                       env=env, stdout=output, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 45
                while time.monotonic() < deadline and process.poll() is None:
                    text = log.read_text(errors="replace")
                    expected = {"initial": "MAM_PROBE_INITIAL_FILLED", "indexed": "MAM_PROBE_SEEK_FILLED",
                                "fallback": "MAM_PROBE_SEEK_FALLBACK"}[args.mode]
                    ready = expected in text if args.probe else "fill-stop target-reached" in text
                    if ready:
                        def demand_counts(log_text):
                            return tuple(log_text.count(marker) for marker in
                                         ("room request", "source-demand", "network account=", "snapshot-request"))
                        requests = demand_counts(text)
                        time.sleep(1)
                        settled = log.read_text(errors="replace")
                        assert demand_counts(settled) == requests, "unbounded MAM demand after navigation completed"
                        assert "history-demand-v4" in text, "an outdated Psi executable was started"
                        assert "room-joined" in text and "archiveReady= true" in text, "new GCMainDlg never joined"
                        checks = re.findall(r"messagePixels=\s*(\d+).*goal=\s*(\d+)", text)
                        assert any(int(p) >= int(g) for p, g in checks), "viewport is not filled"
                        if args.mode == "initial":
                            assert "atEnd= true" in text.split("fill-check")[-1], "initial fill moved away from tail"
                        if run != "cold" and args.mode == "initial":
                            assert "network account=" not in settled, "verified EDB page was fetched again"
                            assert "fromCache= true" in settled, "EDB replay was not exercised"
                        if args.mode == "indexed":
                            indexes = re.findall(r"source-demand.*kind=\s*3 index=\s*(\d+)", settled)
                            shown = re.search(r"MAM_PROBE_SEEK_FILLED messageNumber=\s*(\d+)", settled)
                            assert indexes and shown, "index seek did not show a real message"
                            # Sequence numbers match server event order in this fixture.
                            assert abs(int(indexes[-1]) - int(shown.group(1))) <= 80, "wrong archive area was displayed"
                        if run == "metadata-upgrade":
                            assert settled.count("snapshot-request") == 1, "old cache needs one bounded metadata query"
                            assert re.search(r"snapshot-result.*count=\s*[1-9]\d*", settled), "count was not discovered"
                            with sqlite3.connect(database) as db:
                                saved = db.execute("SELECT result_count,last_uid FROM archive_snapshots WHERE peer=?", (room,)).fetchone()
                                assert saved and saved[0] > 0 and saved[1], "UID/count snapshot was not persisted"
                        if run == "cached-upgrade":
                            assert "snapshot-request" not in settled, "saved snapshot was not reused"
                        print(f"MUC_RUNTIME_RESULT=success run={run} requests={requests} log={log}")
                        break
                    time.sleep(.05)
                else:
                    raise RuntimeError(f"actual Psi never filled room viewport (exit={process.poll()}); see {log}")
            finally:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


if __name__ == "__main__":
    main()
