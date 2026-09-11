from pathlib import Path

path = Path('src/psiaccount.cpp')
text = path.read_text()
old = '''void PsiAccount::actionVoice(const Jid &j)
{
    Jid j2 = j;
    if (j.resource().isEmpty()) {
        UserListItem *u = find(j);
        if (u && u->isAvailable())
            j2 = j2.withResource((*u->userResourceList().priority()).name());
    }

    CallDlg *w = new CallDlg(this, nullptr);
'''
new = '''void PsiAccount::actionVoice(const Jid &j)
{
    Jid j2 = j;
    if (j.resource().isEmpty()) {
        UserListItem *u = find(j);
        if (u && u->isAvailable()) {
            const UserResource *bestCallResource = nullptr;
            for (const auto &resource : u->userResourceList()) {
                const auto features = d->client->capsManager()->features(j.withResource(resource.name()));
                const bool callCapable
                    = features.test(QStringLiteral("urn:xmpp:jingle:1"))
                    && features.test(QStringLiteral("urn:xmpp:jingle:transports:ice-udp:1"))
                    && features.test(QStringLiteral("urn:xmpp:jingle:apps:rtp:1"))
                    && features.test(QStringLiteral("urn:xmpp:jingle:apps:dtls:0"))
                    && features.test(QStringLiteral("urn:xmpp:jingle:apps:rtp:audio"));
                if (callCapable && (!bestCallResource || resource.priority() > bestCallResource->priority()))
                    bestCallResource = &resource;
            }
            if (bestCallResource)
                j2 = j2.withResource(bestCallResource->name());
            else if (auto priority = u->userResourceList().priority(); priority != u->userResourceList().end())
                j2 = j2.withResource(priority->name());
        }
    }

    CallDlg *w = new CallDlg(this, nullptr);
'''
if text.count(old) != 1:
    raise SystemExit(f'expected one actionVoice prologue, got {text.count(old)}')
path.write_text(text.replace(old, new))
