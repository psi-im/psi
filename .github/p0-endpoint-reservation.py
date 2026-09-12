from pathlib import Path


def replace(path, old, new):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise SystemExit(f"{path}: expected exactly one match")
    p.write_text(text.replace(old, new), encoding="utf-8")


replace(
    "src/avcall/psimediajingle.cpp",
    '''    std::unique_ptr<RTP::MediaEndpoint> createEndpoint(const QString &, const QString &media) override
    {
        if (!mediaTypes_.contains(media))
            return {};
        return std::make_unique<Endpoint>(this, media);
    }
''',
    '''    std::unique_ptr<RTP::MediaEndpoint> createEndpoint(const QString &, const QString &media) override
    {
        if (!mediaTypes_.contains(media))
            return {};
        // psimedia exposes one RTP channel per media type. Reserve that channel
        // for the full Endpoint lifetime: stop()/packet-I/O detach must not let a
        // second Jingle content silently share and mutate the same backend state.
        for (auto endpoint : endpoints_) {
            if (endpoint && endpoint->media() == media)
                return {};
        }
        return std::make_unique<Endpoint>(this, media);
    }
''',
)

replace(
    "tests/avcall/backend-lifecycle/tst_backend_lifecycle.cpp",
    '''    void destroyBeforeStart()
    {
''',
    '''    void endpointReservationTracksLifetime()
    {
        auto provider = makePsiMediaJingleProvider(fullCapabilities());
        auto session  = provider->createSession();
        QVERIFY(session);

        auto audio = session->createEndpoint(QStringLiteral("audio-1"), QStringLiteral("audio"));
        QVERIFY(audio);
        QVERIFY(!session->createEndpoint(QStringLiteral("audio-2"), QStringLiteral("audio")));

        auto video = session->createEndpoint(QStringLiteral("video-1"), QStringLiteral("video"));
        QVERIFY(video);
        QVERIFY(!session->createEndpoint(QStringLiteral("video-2"), QStringLiteral("video")));

        audio->stop();
        QVERIFY(!session->createEndpoint(QStringLiteral("audio-after-stop"), QStringLiteral("audio")));

        audio.reset();
        audio = session->createEndpoint(QStringLiteral("audio-recreated"), QStringLiteral("audio"));
        QVERIFY(audio);

        video.reset();
        video = session->createEndpoint(QStringLiteral("video-recreated"), QStringLiteral("video"));
        QVERIFY(video);

        audio.reset();
        video.reset();
        session.reset();
        QCOMPARE(provider_.stats().startCalls, 0);
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void destroyBeforeStart()
    {
''',
)
