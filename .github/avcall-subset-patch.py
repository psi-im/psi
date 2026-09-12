from pathlib import Path

path = Path("src/avcall/avcall.cpp")
text = path.read_text(encoding="utf-8")

replacements = [
    (
'''        mode = audio && video ? AvCall::Both : (audio ? AvCall::Audio : AvCall::Video);
        return true;
''',
'''        requestedAudio      = audio;
        requestedVideo      = video;
        captureAudioConsent = false;
        captureVideoConsent = false;
        mode                = audio && video ? AvCall::Both : (audio ? AvCall::Audio : AvCall::Video);
        return true;
'''),
    (
'''        const bool needAudio = mode == AvCall::Audio || mode == AvCall::Both;
        const bool needVideo = mode == AvCall::Video || mode == AvCall::Both;
        if ((needAudio && !manager->rtpManager->createOutgoing(session, QStringLiteral("audio")))
''',
'''        const bool needAudio = mode == AvCall::Audio || mode == AvCall::Both;
        const bool needVideo = mode == AvCall::Video || mode == AvCall::Both;
        requestedAudio      = needAudio;
        requestedVideo      = needVideo;
        captureAudioConsent = needAudio;
        captureVideoConsent = needVideo;
        if ((needAudio && !manager->rtpManager->createOutgoing(session, QStringLiteral("audio")))
'''),
    (
'''        const bool acceptAudio = mode == AvCall::Audio || mode == AvCall::Both;
        const bool acceptVideo = mode == AvCall::Video || mode == AvCall::Both;
        for (auto app : session->contentList()) {
''',
'''        const bool wantsAudio = mode == AvCall::Audio || mode == AvCall::Both;
        const bool wantsVideo = mode == AvCall::Video || mode == AvCall::Both;
        const bool acceptAudio = requestedAudio && wantsAudio;
        const bool acceptVideo = requestedVideo && wantsVideo;
        captureAudioConsent = acceptAudio;
        captureVideoConsent = acceptVideo;
        if (!acceptAudio && !acceptVideo) {
            errorString      = tr("No offered RTP media type was accepted.");
            localTermination = true;
            session->terminate(Jingle::Reason::Decline);
            emit q->error();
            return;
        }
        for (auto app : session->contentList()) {
'''),
    (
'''        connect(session, &QObject::destroyed, this, [this] {
            session         = nullptr;
            signalingActive = false;
            active          = false;
        });
''',
'''        connect(session, &QObject::destroyed, this, [this] {
            session         = nullptr;
            signalingActive = false;
            active          = false;
            acceptedAudio   = false;
            acceptedVideo   = false;
        });
'''),
    (
'''    void maybeActivateMedia()
    {
        if (!session || !signalingActive || active)
            return;

        const bool needAudio = mode == AvCall::Audio || mode == AvCall::Both;
        const bool needVideo = mode == AvCall::Video || mode == AvCall::Both;
        bool       haveAudio = !needAudio;
        bool       haveVideo = !needVideo;
        bool       audioReady = !needAudio;
        bool       videoReady = !needVideo;

        for (auto app : session->contentList()) {
            auto rtp = dynamic_cast<RTP::Application *>(app);
            if (!rtp)
                continue;
            if (rtp->media() == QLatin1String("audio") && needAudio && rtp->state() < Jingle::State::Finishing) {
                haveAudio  = true;
                audioReady = rtp->state() == Jingle::State::Active;
            } else if (rtp->media() == QLatin1String("video") && needVideo
                       && rtp->state() < Jingle::State::Finishing) {
                haveVideo  = true;
                videoReady = rtp->state() == Jingle::State::Active;
            }
        }

        if (!haveAudio || !haveVideo || !audioReady || !videoReady)
            return;

        active = true;
        startPsiMediaJingleTransmit(session, g_config->liveInput, needAudio, g_config->audioInDeviceId, needVideo,
                                    g_config->videoInDeviceId);
        emit q->activated();
    }
''',
'''    void maybeActivateMedia()
    {
        if (!session || !signalingActive || active)
            return;

        acceptedAudio       = false;
        acceptedVideo       = false;
        bool audioReady     = true;
        bool videoReady     = true;
        bool audioMaySend   = false;
        bool videoMaySend   = false;

        for (auto app : session->contentList()) {
            auto rtp = dynamic_cast<RTP::Application *>(app);
            if (!rtp || rtp->state() >= Jingle::State::Finishing)
                continue;

            const bool localMaySend
                = rtp->senders() == Jingle::Origin::Both || rtp->senders() == session->role();
            if (rtp->media() == QLatin1String("audio")) {
                acceptedAudio = true;
                audioReady    = audioReady && rtp->state() == Jingle::State::Active;
                audioMaySend  = audioMaySend || localMaySend;
            } else if (rtp->media() == QLatin1String("video")) {
                acceptedVideo = true;
                videoReady    = videoReady && rtp->state() == Jingle::State::Active;
                videoMaySend  = videoMaySend || localMaySend;
            }
        }

        if (!acceptedAudio && !acceptedVideo) {
            fail(tr("The peer did not accept any usable RTP media."));
            return;
        }
        if ((acceptedAudio && !audioReady) || (acceptedVideo && !videoReady))
            return;

        const bool transmitAudio = acceptedAudio && captureAudioConsent && audioMaySend;
        const bool transmitVideo = acceptedVideo && captureVideoConsent && videoMaySend;

        active = true;
        // A false return is valid for receive-only calls: activation is driven
        // by negotiated media readiness, while this call controls local capture.
        startPsiMediaJingleTransmit(session, g_config->liveInput, transmitAudio, g_config->audioInDeviceId,
                                    transmitVideo, g_config->videoInDeviceId);
        emit q->activated();
    }
'''),
    (
'''    bool                           incoming = false;
    bool                           signalingActive = false;
    bool                           active = false;
    bool                           localTermination = false;
''',
'''    bool                           incoming = false;
    bool                           signalingActive = false;
    bool                           active = false;
    bool                           localTermination = false;
    bool                           requestedAudio = false;
    bool                           requestedVideo = false;
    bool                           acceptedAudio = false;
    bool                           acceptedVideo = false;
    bool                           captureAudioConsent = false;
    bool                           captureVideoConsent = false;
'''),
]

for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"expected exactly one match, found {count}: {old[:80]!r}")
    text = text.replace(old, new)

path.write_text(text, encoding="utf-8")
