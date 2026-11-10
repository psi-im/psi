#!/usr/bin/env python3
"""Drive the real settings and roster in an isolated offscreen Psi process.

The test-only QStyle plugin sends ordinary UI actions; no product test hooks,
private account methods or fake network backend are used. Multicast stays on lo.
"""
import base64
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import uuid
import sqlite3
import signal
import xml.etree.ElementTree as ET

NS = "{http://psi-im.org/options}"
with tempfile.TemporaryDirectory(prefix="psi-serverless-ui-") as temporary:
    base = Path(temporary)
    profile = base / "profiles/default"
    profile.mkdir(parents=True)
    config = profile / "accounts.xml"
    config.write_text('''<accounts xmlns="http://psi-im.org/options" version="1.6"><accounts><a0>
<id type="QString">existing-xmpp-fixture</id><type type="QString">xmpp</type>
<name type="QString">Existing XMPP</name><jid type="QString">fixture@example.invalid</jid>
<enabled type="bool">false</enabled><auto type="bool">false</auto>
</a0></accounts></accounts>''')
    (base / "psirc").write_text("[General]\nlast_profile=default\nauto_open=true\nlast_lang=en\n")

    # The catalog of an older Psi. The fixture account shared one file, and merely received another.
    legacy_catalog = profile / "shares/shared-files.sqlite"
    legacy_catalog.parent.mkdir()
    def legacy_descriptor(name, peer):
        path = base / name
        path.write_bytes(name.encode())
        digest = hashlib.sha256(name.encode()).hexdigest()
        reference = ("<reference xmlns='urn:xmpp:reference:0' type='data' uri='https://example.invalid/{0}'>"
                     "<media-sharing xmlns='urn:xmpp:sims:1'><file xmlns='urn:xmpp:jingle:apps:file-transfer:5'>"
                     "<hash xmlns='urn:xmpp:hashes:2' algo='sha-256'>{1}</hash><size>{2}</size><name>{0}</name></file>"
                     "<sources><reference xmlns='urn:xmpp:reference:0' type='data' "
                     "uri='https://example.invalid/{0}'/></sources></media-sharing></reference>").format(
                         name, base64.b64encode(bytes.fromhex(digest)).decode(), len(name))
        return ("sha-256+" + digest, json.dumps({
            "version": 1, "reference": reference, "peers": [peer],
            "local": {"path": str(path), "size": str(len(name)),
                      "modified": str(int(path.stat().st_mtime * 1000))}}))
    with sqlite3.connect(legacy_catalog) as database:
        database.execute("CREATE TABLE shared_files(account_id TEXT, hash TEXT, descriptor TEXT)")
        for name, peer in (("shared", "fixture@example.invalid/desktop"), ("received", "other@example.invalid/phone")):
            database.execute("INSERT INTO shared_files VALUES('existing-xmpp-fixture',?,?)",
                             legacy_descriptor(name, peer))
    database.close()

    environment = dict(os.environ, PSIDATADIR=str(base),
                       XDG_CONFIG_HOME=str(base / "xdg/config"),
                       XDG_DATA_HOME=str(base / "xdg/data"),
                       XDG_CACHE_HOME=str(base / "xdg/cache"),
                       QT_QPA_PLATFORM="offscreen", QT_STYLE_OVERRIDE="serverless-ui-probe",
                       QT_PLUGIN_PATH=sys.argv[2], LANG="C.UTF-8",
                       PSI_UI_SCREENSHOT=str(base / "settings.png"))
    persistent_id = None
    deleting = bool(environment.get("PSI_UI_DELETE_TEST"))
    for run in range(2):
        environment["PSI_UI_EXPECTED_NAME"] = environment.get("USER", environment.get("USERNAME", "Psi")) if run == 0 else "UI Alice"
        peer = None
        with (base / "peer.log").open("w+") as log:
            try:
                if run == 0:
                    environment["PSI_UI_CHAT_TEST"] = "1"
                    peer = subprocess.Popen([sys.argv[3], "test@p-" + uuid.uuid4().hex[:8]],
                                            stdout=log, stderr=subprocess.STDOUT, env=environment)
                    deadline = time.monotonic() + 12
                    while "PEER-READY" not in (base / "peer.log").read_text():
                        assert peer.poll() is None and time.monotonic() < deadline, "Peer did not start"
                        time.sleep(0.05)
                else:
                    environment.pop("PSI_UI_CHAT_TEST", None)
                with (base / "app.log").open("w+") as output:
                    result = subprocess.Popen(["dbus-run-session", "--", sys.argv[1], "--profile=default"],
                                              env=environment, stdout=output, stderr=subprocess.STDOUT,
                                              start_new_session=True)
                    try:
                        result.wait(timeout=60 if deleting else 45)
                    except subprocess.TimeoutExpired:
                        os.killpg(result.pid, signal.SIGTERM)
                        try:
                            result.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            os.killpg(result.pid, signal.SIGKILL)
                            result.wait(timeout=5)
                        raise AssertionError((base / "app.log").read_text())
                app_log = (base / "app.log").read_text()
                assert result.returncode == 0 and "UI-PROBE PASS" in app_log, f"Psi exit={result.returncode}:\n{app_log}"
                if run == 0:
                    assert "UI-PROBE MESSAGES" in app_log, app_log
                    if deleting:
                        assert "UI-PROBE LOCAL-DELETION" in app_log, app_log
                    assert "PEER-MESSAGE-OK" in (base / "peer.log").read_text()
            finally:
                if peer is not None:
                    peer.terminate()
                    try:
                        peer.wait(timeout=8)
                    except subprocess.TimeoutExpired:
                        peer.kill()
                        peer.wait(timeout=5)
        accounts = ET.parse(config).getroot().find(f"{NS}accounts")
        existing = [item for item in accounts if item.findtext(f"{NS}id") == "existing-xmpp-fixture"]
        assert len(existing) == 1 and existing[0].findtext(f"{NS}jid") == "fixture@example.invalid"
        local = [item for item in accounts if item.findtext(f"{NS}type") == "serverless"]
        assert len(local) == 1
        account = local[0]
        assert account.findtext(f"{NS}enabled") == "false"
        assert account.findtext(f"{NS}serverless/{NS}name") == "UI Alice"
        assert account.findtext(f"{NS}serverless/{NS}interface") == "lo"
        if persistent_id is None:
            persistent_id = account.findtext(f"{NS}id")
        assert account.findtext(f"{NS}id") == persistent_id
        history = profile / "history/history.db"
        assert history.is_file(), "No SQLite history created"
        with sqlite3.connect(history) as database:
            count = database.execute("SELECT count(*) FROM events e JOIN contacts c ON c.id=e.contact_id "
                                     "WHERE c.acc_id=? AND e.m_text=?",
                                     (persistent_id, "serverless-ui-test")).fetchone()[0]
            assert count == (1 if deleting else 2), f"Expected incoming/outgoing EDB records, got {count}"
            # Of those two, the reply attaches two files, and history links only those to publications.
            links = database.execute("SELECT r.publication_id FROM message_media_refs r "
                                     "JOIN events e ON e.id=r.event_id JOIN contacts c ON c.id=e.contact_id "
                                     "WHERE c.acc_id=? ORDER BY r.ordinal", (persistent_id,)).fetchall()
            assert len(links) == (0 if deleting else 2), f"Expected the attachments of the reply to be linked, got {links}"
        database.close()
        with sqlite3.connect(profile / "files.sqlite") as database:
            publications = database.execute("SELECT id,serving,announcer FROM publications WHERE account=?",
                                            (persistent_id,)).fetchall()
            assert sorted(row[:2] for row in publications) == sorted((link[0], 0) for link in links), publications
            if deleting:
                with sqlite3.connect(history) as stored:
                    assert stored.execute("SELECT count(*) FROM events e JOIN contacts c ON c.id=e.contact_id "
                                          "WHERE c.acc_id=? AND (e.message_flags & 16)<>0 AND e.extra_data=''",
                                          (persistent_id,)).fetchone()[0] == 1
                    assert stored.execute("SELECT count(*) FROM message_suppressions WHERE account=?",
                                          (persistent_id,)).fetchone()[0] >= 1
                stored.close()
            assert all(row[2].startswith("test@p-") for row in publications), publications
            for link, url, content in (() if deleting else ((links[0], "https://example.invalid/peer-file.bin", b"serverless-ui-file"),
                                       (links[1], "https://example.invalid/shared-file.bin", b"serverless-ui-sharing"))):
                sources = database.execute("SELECT uri FROM publication_sources WHERE publication_id=?",
                                           link).fetchall()
                assert sources == [(url,)], sources
                digests = database.execute("SELECT algorithm,digest FROM publication_hashes WHERE publication_id=?",
                                           link).fetchall()
                assert digests == [("sha-256", hashlib.sha256(content).digest())], digests
            # Only what the account shared survives from the old catalog, and it serves that.
            imported = database.execute("SELECT serving,announcer FROM publications WHERE account=?",
                                        ("existing-xmpp-fixture",)).fetchall()
            assert imported == [(1, "fixture@example.invalid")], imported
        database.close()
        assert not legacy_catalog.exists(), "The old catalog was left behind"
    print("PASS: settings, discovered peer, two-process chat, file publications, roster, disable and profile restart")
