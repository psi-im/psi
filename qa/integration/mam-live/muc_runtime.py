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

    def seed(self, room, protocol_events):
        self.send(f"<presence to='{room}/Seeder'><x xmlns='http://jabber.org/protocol/muc'>"
                  "<history maxstanzas='0'/></x></presence>")
        self.until(b"110")
        self.send(f"<iq to='{room}' type='set' id='config'><query xmlns='http://jabber.org/protocol/muc#owner'>"
                  "<x xmlns='jabber:x:data' type='submit'/></query></iq>")
        self.iq("config")
        for i in range(80 + protocol_events + 4):
            payload = ("<body>Runtime message %d</body>" % i
                       if i < 80 or i >= 80 + protocol_events
                       else "<received xmlns='urn:xmpp:receipts' id='fixture-target'/>"
                            "<store xmlns='urn:xmpp:hints'/>")
            self.send(f"<message to='{room}' type='groupchat' id='fixture-{i}'>{payload}</message>")
        # The last echo is a storage/order barrier; no arbitrary persistence sleep.
        self.until(f"fixture-{80 + protocol_events + 3}".encode())
        self.send(f"<iq type='set' id='bookmarks'><query xmlns='jabber:iq:private'>"
                  f"<storage xmlns='storage:bookmarks'><conference jid='{room}' autojoin='true' name='Runtime'>"
                  "<nick>Reader</nick></conference></storage></query></iq>")
        self.iq("bookmarks")

    def close(self):
        self.send("</stream:stream>")
        self.socket.close()


def profile(root, port):
    directory = root / "profiles" / "mam-runtime"
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
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    with args.psi.open("rb") as executable:
        digest = hashlib.file_digest(executable, "sha256").hexdigest()
    print(f"MUC_RUNTIME_BINARY={args.psi.resolve()} sha256={digest}", flush=True)
    room = "runtime-" + str(int(time.time())) + "@conference.localhost"
    client = FixtureClient(args.port)
    client.seed(room, args.protocol_events)
    client.close()
    profile(args.output / "profile", args.port)
    env = dict(os.environ, PSIDATADIR=str((args.output / "profile").resolve()),
               QT_QPA_PLATFORM="offscreen", QT_LOGGING_RULES="psi.history.debug=true",
               QT_MESSAGE_PATTERN="%{category}: %{message}")
    for run in ("cold", "cached"):
        log = args.output / (run + ".log")
        with log.open("w") as output:
            process = subprocess.Popen([str(args.psi.resolve()), "--profile=mam-runtime"],
                                       env=env, stdout=output, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 45
                while time.monotonic() < deadline and process.poll() is None:
                    text = log.read_text(errors="replace")
                    if "fill-stop target-reached" in text:
                        requests = text.count("room request")
                        time.sleep(1)
                        settled = log.read_text(errors="replace")
                        assert settled.count("room request") == requests, "unbounded MAM demand"
                        assert "history-demand-v4" in text, "an outdated Psi executable was started"
                        assert "room-joined" in text and "archiveReady= true" in text, "new GCMainDlg never joined"
                        checks = re.findall(r"messagePixels=\s*(\d+).*goal=\s*(\d+)", text)
                        assert any(int(p) >= int(g) for p, g in checks), "viewport is not filled"
                        assert "atEnd= true" in text.split("fill-check")[-1], "initial fill moved away from tail"
                        if run == "cached":
                            assert "network account=" not in settled, "verified EDB page was fetched again"
                            assert "fromCache= true" in settled, "EDB replay was not exercised"
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
