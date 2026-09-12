from pathlib import Path


def replace(path, old, new):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise SystemExit(f"{path}: expected exactly one match")
    p.write_text(text.replace(old, new), encoding="utf-8")


replace(
    "src/avcall/psimediajingle.h",
    "    bool available() const { return audio || video; }\n",
    "    bool available() const { return backendAvailable && probeComplete && secureRtp && (audio || video); }\n",
)

replace(
    "src/avcall/psimediajingle.cpp",
    '''bool PsiMediaJingleCapabilities::supportsMedia(const QString &media) const
{
    return (media == QLatin1String("audio") && audio) || (media == QLatin1String("video") && video);
}

QStringList PsiMediaJingleCapabilities::mediaTypes() const
{
    QStringList result;
    if (audio)
        result.append(QStringLiteral("audio"));
    if (video)
        result.append(QStringLiteral("video"));
    return result;
}
''',
    '''bool PsiMediaJingleCapabilities::supportsMedia(const QString &media) const
{
    return available()
        && ((media == QLatin1String("audio") && audio) || (media == QLatin1String("video") && video));
}

QStringList PsiMediaJingleCapabilities::mediaTypes() const
{
    if (!backendAvailable || !probeComplete || !secureRtp)
        return {};
    QStringList result;
    if (audio)
        result.append(QStringLiteral("audio"));
    if (video)
        result.append(QStringLiteral("video"));
    return result;
}
''',
)

replace(
    "tests/avcall/backend-lifecycle/tst_backend_lifecycle.cpp",
    '''    void capabilitySnapshotLimitsMediaTypes()
    {
        auto audioOnly = fullCapabilities();
        audioOnly.video = false;
        auto audioProvider = makePsiMediaJingleProvider(audioOnly);
        QCOMPARE(audioProvider->mediaTypes(), QStringList { QStringLiteral("audio") });

        auto disabled = audioOnly;
        disabled.audio = false;
        auto disabledProvider = makePsiMediaJingleProvider(disabled);
        QVERIFY(disabledProvider->mediaTypes().isEmpty());
        QVERIFY(!disabledProvider->createSession());
    }
''',
    '''    void capabilitySnapshotLimitsMediaTypes()
    {
        auto audioOnly = fullCapabilities();
        audioOnly.video = false;
        auto audioProvider = makePsiMediaJingleProvider(audioOnly);
        QCOMPARE(audioProvider->mediaTypes(), QStringList { QStringLiteral("audio") });

        auto receiveOnlyVideo = fullCapabilities();
        receiveOnlyVideo.audio      = false;
        receiveOnlyVideo.videoInput = false;
        auto videoProvider = makePsiMediaJingleProvider(receiveOnlyVideo);
        QCOMPARE(videoProvider->mediaTypes(), QStringList { QStringLiteral("video") });

        auto unavailable = fullCapabilities();
        unavailable.backendAvailable = false;
        QVERIFY(makePsiMediaJingleProvider(unavailable)->mediaTypes().isEmpty());
        unavailable                   = fullCapabilities();
        unavailable.probeComplete     = false;
        QVERIFY(makePsiMediaJingleProvider(unavailable)->mediaTypes().isEmpty());
        unavailable               = fullCapabilities();
        unavailable.secureRtp     = false;
        QVERIFY(makePsiMediaJingleProvider(unavailable)->mediaTypes().isEmpty());

        auto disabled = fullCapabilities();
        disabled.audio = false;
        disabled.video = false;
        auto disabledProvider = makePsiMediaJingleProvider(disabled);
        QVERIFY(disabledProvider->mediaTypes().isEmpty());
        QVERIFY(!disabledProvider->createSession());
    }
''',
)
