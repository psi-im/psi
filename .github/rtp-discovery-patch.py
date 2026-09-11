from pathlib import Path

path = Path('src/avcall/psimediajingle.cpp')
text = path.read_text()
old = '''class Provider final : public RTP::MediaProvider {
public:
    std::unique_ptr<RTP::MediaSession> createSession() override { return std::make_unique<BackendSession>(); }
};
'''
new = '''class Provider final : public RTP::MediaProvider {
public:
    std::unique_ptr<RTP::MediaSession> createSession() override { return std::make_unique<BackendSession>(); }
    QStringList mediaTypes() const override { return { QStringLiteral("audio"), QStringLiteral("video") }; }
};
'''
if text.count(old) != 1:
    raise SystemExit(f'expected one Provider block, got {text.count(old)}')
path.write_text(text.replace(old, new))
