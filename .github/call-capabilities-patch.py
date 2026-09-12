from pathlib import Path


def replace(path, old, new, count=1):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    found = text.count(old)
    if found != count:
        raise SystemExit(f"{path}: expected {count} matches, found {found}: {old[:100]!r}")
    p.write_text(text.replace(old, new), encoding="utf-8")


def replace_all(path, old, new):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    if old not in text:
        raise SystemExit(f"{path}: missing {old!r}")
    p.write_text(text.replace(old, new), encoding="utf-8")


# Build policy: native calls require the secure RTP backend only when requested.
replace(
    "CMakeLists.txt",
    'option( ENABLE_PLUGINS "Enable plugins" OFF )\n',
    'option( ENABLE_PLUGINS "Enable plugins" OFF )\n'
    'option( ENABLE_AVCALL "Enable native secure audio/video calls" ON )\n',
)
replace(
    "CMakeLists.txt",
    '''if(IRIS_BUNDLED_QCA)
    add_definitions(-DQCA_STATIC)
endif()
''',
    '''if(BUNDLED_IRIS AND ENABLE_AVCALL)
    # Native RTP calls are DTLS-SRTP only. Keep SRTP optional for Iris itself,
    # but require it for Psi builds that expose the call feature.
    set(IRIS_ENABLE_SRTP ON CACHE BOOL "Enable libSRTP packet protection for native Jingle RTP" FORCE)
endif()
if(ENABLE_AVCALL)
    add_compile_definitions(PSI_ENABLE_AVCALL)
endif()

if(IRIS_BUNDLED_QCA)
    add_definitions(-DQCA_STATIC)
endif()
''',
)

# Immutable per-session media capability snapshot.
Path("src/avcall/psimediajingle.h").write_text(r'''/*
 * Copyright (C) 2026  Psi Project
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PSIMEDIAJINGLE_H
#define PSIMEDIAJINGLE_H

#include <QStringList>

#include <memory>

class QString;

namespace PsiMedia {
class VideoWidget;
}

namespace XMPP::Jingle {
class Session;
namespace RTP {
class MediaProvider;
}
}

struct PsiMediaJingleCapabilities {
    bool backendAvailable = false;
    bool probeComplete    = false;
    bool secureRtp        = false;
    bool audio            = false;
    bool video            = false;
    bool audioInput       = false;
    bool audioOutput      = false;
    bool videoInput       = false;

    bool operator==(const PsiMediaJingleCapabilities &) const = default;
    bool available() const { return audio || video; }
    bool supportsMedia(const QString &media) const;
    QStringList mediaTypes() const;
    QString unavailableReason() const;
};

std::shared_ptr<XMPP::Jingle::RTP::MediaProvider>
makePsiMediaJingleProvider(const PsiMediaJingleCapabilities &capabilities);

// Psi policy/control surface for the psimedia backend owned by an Iris-native
// Jingle RTP session. Negotiation never enables capture by itself.
bool configurePsiMediaJingleSession(XMPP::Jingle::Session *session, const QString &audioOutputDevice,
                                    const QString &fileInput, bool loopFile, int maximumSendingBitrate);
bool setPsiMediaJingleVideoOutput(XMPP::Jingle::Session *session, PsiMedia::VideoWidget *widget);
bool startPsiMediaJingleTransmit(XMPP::Jingle::Session *session, bool liveInput, bool audio,
                                 const QString &audioInputDevice, bool video, const QString &videoInputDevice);
void stopPsiMediaJingleTransmit(XMPP::Jingle::Session *session);

#endif // PSIMEDIAJINGLE_H
''', encoding="utf-8")

replace(
    "src/avcall/psimediajingle.cpp",
    '''namespace {
namespace RTP = XMPP::Jingle::RTP;
''',
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

QString PsiMediaJingleCapabilities::unavailableReason() const
{
    if (!backendAvailable)
        return QStringLiteral("The psimedia provider is unavailable.");
    if (!probeComplete)
        return QStringLiteral("Media codec capabilities are not available yet.");
    if (!secureRtp)
        return QStringLiteral("DTLS-SRTP is unavailable in the current Iris/QCA/libSRTP configuration.");
    if (!audio && !video)
        return QStringLiteral("The media backend reported no usable RTP audio or video codecs.");
    return {};
}

namespace {
namespace RTP = XMPP::Jingle::RTP;
''',
)
replace(
    "src/avcall/psimediajingle.cpp",
    '''    BackendSession()
    {
''',
    '''    explicit BackendSession(QStringList mediaTypes) : mediaTypes_(std::move(mediaTypes))
    {
''',
)
replace(
    "src/avcall/psimediajingle.cpp",
    '''    std::unique_ptr<RTP::MediaEndpoint> createEndpoint(const QString &, const QString &media) override
    {
        if (media != QLatin1String("audio") && media != QLatin1String("video"))
            return {};
        return std::make_unique<Endpoint>(this, media);
    }
''',
    '''    std::unique_ptr<RTP::MediaEndpoint> createEndpoint(const QString &, const QString &media) override
    {
        if (!mediaTypes_.contains(media))
            return {};
        return std::make_unique<Endpoint>(this, media);
    }
''',
)
replace(
    "src/avcall/psimediajingle.cpp",
    '''    PsiMedia::RtpSession         rtp_;
''',
    '''    QStringList                   mediaTypes_;
    PsiMedia::RtpSession         rtp_;
''',
)
replace(
    "src/avcall/psimediajingle.cpp",
    '''class Provider final : public RTP::MediaProvider {
public:
    std::unique_ptr<RTP::MediaSession> createSession() override { return std::make_unique<BackendSession>(); }
    QStringList mediaTypes() const override { return { QStringLiteral("audio"), QStringLiteral("video") }; }
};
''',
    '''class Provider final : public RTP::MediaProvider {
public:
    explicit Provider(PsiMediaJingleCapabilities capabilities) : capabilities_(std::move(capabilities)) { }

    std::unique_ptr<RTP::MediaSession> createSession() override
    {
        const auto types = capabilities_.mediaTypes();
        return types.isEmpty() ? nullptr : std::make_unique<BackendSession>(types);
    }
    QStringList mediaTypes() const override { return capabilities_.mediaTypes(); }

private:
    const PsiMediaJingleCapabilities capabilities_;
};
''',
)
replace(
    "src/avcall/psimediajingle.cpp",
    '''std::shared_ptr<XMPP::Jingle::RTP::MediaProvider> makePsiMediaJingleProvider()
{
    return std::make_shared<Provider>();
}
''',
    '''std::shared_ptr<XMPP::Jingle::RTP::MediaProvider>
makePsiMediaJingleProvider(const PsiMediaJingleCapabilities &capabilities)
{
    return std::make_shared<Provider>(capabilities);
}
''',
)

# Forward asynchronous codec probe completion separately from device selection.
replace(
    "src/avcall/mediadevicewatcher.h",
    '''    inline QList<PsiMedia::VideoParams> supportedVideoModes() { return _features.supportedVideoModes(); }

signals:
    void updated();
    void availibityChanged();

private:
    MediaConfiguration         _configuration;
    PsiMedia::Features         _features;
    static MediaDeviceWatcher *_instance;
''',
    '''    inline QList<PsiMedia::VideoParams> supportedVideoModes() { return _features.supportedVideoModes(); }
    inline bool                         featuresReady() const { return _featuresReady; }

signals:
    void updated();
    void availibityChanged();
    void capabilitiesChanged();

private:
    MediaConfiguration         _configuration;
    PsiMedia::Features         _features;
    bool                       _featuresReady = false;
    static MediaDeviceWatcher *_instance;
''',
)
replace(
    "src/avcall/mediadevicewatcher.cpp",
    '''MediaDeviceWatcher::MediaDeviceWatcher(QObject *parent) : QObject(parent)
{
    connect(&_features, &PsiMedia::Features::availibityChanged, this, &MediaDeviceWatcher::availibityChanged);
}
''',
    '''MediaDeviceWatcher::MediaDeviceWatcher(QObject *parent) : QObject(parent)
{
    connect(&_features, &PsiMedia::Features::availibityChanged, this, [this] {
        _featuresReady = false;
        emit availibityChanged();
        emit capabilitiesChanged();
    });
    connect(&_features, &PsiMedia::Features::updated, this, [this] {
        _featuresReady = true;
        emit capabilitiesChanged();
    });
}
''',
)

# AvCall uses one capability builder for UI, provider/session creation and capture policy.
replace(
    "src/avcall/avcall.h",
    '''    static void config();
    static bool isSupported();
''',
    '''    static void config();
    static bool isSupported();
    static bool isAudioSupported();
    static bool isVideoSupported();
    static QString unsupportedReason();
''',
)
replace(
    "src/avcall/avcall.cpp",
    '''static MediaConfiguration *g_config = new MediaConfiguration;

class AvCallManagerPrivate : public QObject {
''',
    '''static MediaConfiguration *g_config = new MediaConfiguration;

static PsiMediaJingleCapabilities currentNativeCallCapabilities()
{
    PsiMediaJingleCapabilities result;
    auto                       watcher = MediaDeviceWatcher::instance();
    result.backendAvailable            = PsiMedia::isSupported();
    result.probeComplete               = watcher->featuresReady();
#ifdef PSI_ENABLE_AVCALL
    result.secureRtp = !RTP::supportedSecureRtpProfiles().isEmpty();
#else
    result.secureRtp = false;
#endif
    const bool mediaReady = result.backendAvailable && result.probeComplete && result.secureRtp;
    result.audio           = mediaReady && !watcher->supportedAudioModes().isEmpty();
    result.video           = mediaReady && !watcher->supportedVideoModes().isEmpty();
    result.audioInput      = !watcher->audioInputDevices().isEmpty();
    result.audioOutput     = !watcher->audioOutputDevices().isEmpty();
    result.videoInput      = !watcher->videoInputDevices().isEmpty();
    return result;
}

class AvCallManagerPrivate : public QObject {
''',
)
replace(
    "src/avcall/avcall.cpp",
    '''    void unlink(AvCall *call);
    void applyNetworkConfiguration();
''',
    '''    void unlink(AvCall *call);
    void applyNetworkConfiguration();
    void refreshCapabilities();
''',
)
replace(
    "src/avcall/avcall.cpp",
    '''    std::shared_ptr<RTP::MediaProvider>  mediaProvider;
    QList<AvCall *>                      sessions;
''',
    '''    std::shared_ptr<RTP::MediaProvider>  mediaProvider;
    PsiMediaJingleCapabilities           capabilities;
    QList<AvCall *>                      sessions;
''',
)
replace(
    "src/avcall/avcall.cpp",
    '''        const bool needAudio = mode == AvCall::Audio || mode == AvCall::Both;
        const bool needVideo = mode == AvCall::Video || mode == AvCall::Both;
        requestedAudio      = needAudio;
''',
    '''        const bool needAudio = mode == AvCall::Audio || mode == AvCall::Both;
        const bool needVideo = mode == AvCall::Video || mode == AvCall::Both;
        if (!manager->capabilities.available()) {
            fail(manager->capabilities.unavailableReason());
            return;
        }
        if ((needAudio && !manager->capabilities.audio) || (needVideo && !manager->capabilities.video)) {
            fail(tr("The requested media type is not supported by the current media backend."));
            return;
        }
        requestedAudio      = needAudio;
''',
)
replace(
    "src/avcall/avcall.cpp",
    '''        const bool transmitAudio = acceptedAudio && captureAudioConsent && audioMaySend;
        const bool transmitVideo = acceptedVideo && captureVideoConsent && videoMaySend;
''',
    '''        const bool audioCaptureAvailable
            = !g_config->liveInput || (manager && manager->capabilities.audioInput);
        const bool videoCaptureAvailable
            = !g_config->liveInput || (manager && manager->capabilities.videoInput);
        const bool transmitAudio = acceptedAudio && captureAudioConsent && audioMaySend && audioCaptureAvailable;
        const bool transmitVideo = acceptedVideo && captureVideoConsent && videoMaySend && videoCaptureAvailable;
''',
)
replace(
    "src/avcall/avcall.cpp",
    '''    mediaProvider = makePsiMediaJingleProvider();

    rtpManager->setMediaProvider(mediaProvider);
    rtpManager->setTransportNamespaces({ ICE::NS, ICE::NS_ICE_UDP });
    connect(jingleManager, &Jingle::Manager::incomingSession, this, &AvCallManagerPrivate::incomingSession);
''',
    '''    auto watcher = MediaDeviceWatcher::instance();
    connect(watcher, &MediaDeviceWatcher::capabilitiesChanged, this, &AvCallManagerPrivate::refreshCapabilities);
    refreshCapabilities();

    rtpManager->setTransportNamespaces({ ICE::NS, ICE::NS_ICE_UDP });
    connect(jingleManager, &Jingle::Manager::incomingSession, this, &AvCallManagerPrivate::incomingSession);
''',
)
replace(
    "src/avcall/avcall.cpp",
    '''void AvCallManagerPrivate::applyNetworkConfiguration()
{
    iceManager->setBasePort(g_config->basePort);
    iceManager->setExternalAddress(g_config->extHost);
}

void AvCallManagerPrivate::incomingSession(Jingle::Session *incoming)
''',
    '''void AvCallManagerPrivate::applyNetworkConfiguration()
{
    iceManager->setBasePort(g_config->basePort);
    iceManager->setExternalAddress(g_config->extHost);
}

void AvCallManagerPrivate::refreshCapabilities()
{
    const auto next = currentNativeCallCapabilities();
    if (mediaProvider && next == capabilities)
        return;
    capabilities  = next;
    mediaProvider = makePsiMediaJingleProvider(capabilities);
    rtpManager->setMediaProvider(mediaProvider);
}

void AvCallManagerPrivate::incomingSession(Jingle::Session *incoming)
''',
)
replace(
    "src/avcall/avcall.cpp",
    '''    if (!PsiMedia::isSupported()) {
        incoming->terminate(Jingle::Reason::UnsupportedApplications);
        return;
    }
''',
    '''    if (!capabilities.available()) {
        incoming->terminate(Jingle::Reason::UnsupportedApplications);
        return;
    }
''',
)
replace(
    "src/avcall/avcall.cpp",
    '''bool AvCallManager::isSupported()
{
    if (!QCA::isSupported("hmac(sha1)")) {
        qWarning("hmac support missing for calls, install qca-ossl");
        return false;
    }
    return PsiMedia::isSupported();
}
''',
    '''bool AvCallManager::isSupported() { return currentNativeCallCapabilities().available(); }

bool AvCallManager::isAudioSupported() { return currentNativeCallCapabilities().audio; }

bool AvCallManager::isVideoSupported() { return currentNativeCallCapabilities().video; }

QString AvCallManager::unsupportedReason() { return currentNativeCallCapabilities().unavailableReason(); }
''',
)

# Call dialog offers video only when a codec exists; camera presence controls capture later, not receive capability.
replace(
    "src/avcall/calldlg.cpp",
    '''        if (AvCallManager::isSupported()) {
            auto config = MediaDeviceWatcher::instance()->configuration();
''',
    '''        if (AvCallManager::isSupported()) {
            auto config = MediaDeviceWatcher::instance()->configuration();
''',
)
replace(
    "src/avcall/calldlg.cpp",
    '''        ui.lb_bandwidth->setEnabled(false);
        ui.cb_bandwidth->setEnabled(false);
''',
    '''        if (!AvCallManager::isVideoSupported()) {
            ui.ck_useVideo->setChecked(false);
            ui.ck_useVideo->setEnabled(false);
        }

        ui.lb_bandwidth->setEnabled(false);
        ui.cb_bandwidth->setEnabled(false);
''',
)
replace(
    "src/avcall/calldlg.cpp",
    '''        if (sess->mode() == AvCall::Video || sess->mode() == AvCall::Both) {
            ui.ck_useVideo->setChecked(true);
''',
    '''        if (AvCallManager::isVideoSupported()
            && (sess->mode() == AvCall::Video || sess->mode() == AvCall::Both)) {
            ui.ck_useVideo->setChecked(true);
''',
)

# Presence caps follow the same asynchronous capability snapshot.
replace(
    "src/psiaccount.cpp",
    '''connect(MediaDeviceWatcher::instance(), &MediaDeviceWatcher::availibityChanged, this, &PsiAccount::updateFeatures);''',
    '''connect(MediaDeviceWatcher::instance(), &MediaDeviceWatcher::capabilitiesChanged, this,
            &PsiAccount::updateFeatures);''',
)
replace(
    "src/psiaccount.cpp",
    '''    if (AvCallManager::isSupported()) {
        features << QLatin1String("urn:xmpp:jingle:transports:ice-udp:1");
        features << QLatin1String("urn:xmpp:jingle:transports:ice:0");
        features << QLatin1String("urn:xmpp:jingle:apps:rtp:1");
        features << QLatin1String("urn:xmpp:jingle:apps:rtp:audio");
        features << QLatin1String("urn:xmpp:jingle:apps:rtp:video");
    }
''',
    '''    if (AvCallManager::isSupported()) {
        features << QLatin1String("urn:xmpp:jingle:transports:ice-udp:1");
        features << QLatin1String("urn:xmpp:jingle:transports:ice:0");
        features << QLatin1String("urn:xmpp:jingle:apps:rtp:1");
        if (AvCallManager::isAudioSupported())
            features << QLatin1String("urn:xmpp:jingle:apps:rtp:audio");
        if (AvCallManager::isVideoSupported())
            features << QLatin1String("urn:xmpp:jingle:apps:rtp:video");
    }
''',
)

# Provider snapshot regression and existing lifecycle harness use an explicit full capability set.
replace(
    "tests/avcall/backend-lifecycle/tst_backend_lifecycle.cpp",
    '''struct Harness {
    std::shared_ptr<RTP::MediaProvider> provider = makePsiMediaJingleProvider();
''',
    '''PsiMediaJingleCapabilities fullCapabilities()
{
    PsiMediaJingleCapabilities result;
    result.backendAvailable = true;
    result.probeComplete    = true;
    result.secureRtp        = true;
    result.audio            = true;
    result.video            = true;
    result.audioInput       = true;
    result.audioOutput      = true;
    result.videoInput       = true;
    return result;
}

struct Harness {
    std::shared_ptr<RTP::MediaProvider> provider = makePsiMediaJingleProvider(fullCapabilities());
''',
)
replace(
    "tests/avcall/backend-lifecycle/tst_backend_lifecycle.cpp",
    '''    void destroyBeforeStart()
    {
''',
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

    void destroyBeforeStart()
    {
''',
)

# CI/build dependencies for IRIS_ENABLE_SRTP=ON in bundled-call builds.
replace_all(".github/workflows/windows-msvc-qt6.yml", "windows-sdk-msvc-qt6-v1", "windows-sdk-msvc-qt6-v2")
replace_all(".github/workflows/windows-msvc-qt6-profile.yml", "windows-sdk-msvc-qt6-v1", "windows-sdk-msvc-qt6-v2")
replace_all(".github/workflows/windows-sdk-msvc-qt6.yml", "windows-sdk-msvc-qt6-v1", "windows-sdk-msvc-qt6-v2")
replace(
    ".github/workflows/windows-sdk-msvc-qt6.yml",
    '''            minizip:x64-windows
''',
    '''            minizip:x64-windows `
            libsrtp[openssl]:x64-windows
''',
)
replace(
    ".github/workflows/windows-sdk-msvc-qt6.yml",
    '''          foreach ($package in @('openssl', 'zlib', 'hunspell', 'minizip', 'libiconv')) {
''',
    '''          foreach ($package in @('openssl', 'zlib', 'hunspell', 'minizip', 'libiconv', 'libsrtp')) {
''',
)
replace(
    ".github/workflows/windows-sdk-msvc-qt6.yml",
    '''          if (-not (Get-ChildItem sdk\\lib -File | Where-Object Name -Match 'libcrypto.*\\.lib$')) {
            throw 'OpenSSL crypto import library missing from SDK'
          }
''',
    '''          if (-not (Get-ChildItem sdk\\lib -File | Where-Object Name -Match 'libcrypto.*\\.lib$')) {
            throw 'OpenSSL crypto import library missing from SDK'
          }
          if (-not (Test-Path sdk\\include\\srtp2\\srtp.h)) {
            throw 'libSRTP headers missing from SDK'
          }
          if (-not (Get-ChildItem sdk\\lib -File | Where-Object Name -Match 'srtp2.*\\.lib$')) {
            throw 'libSRTP import library missing from SDK'
          }
''',
)
replace(
    ".github/workflows/windows-sdk-msvc-qt6.yml",
    '''          vcpkg packages: openssl, zlib, hunspell, minizip and transitive runtime dependencies
''',
    '''          vcpkg packages: openssl, zlib, hunspell, minizip, libsrtp[openssl] and transitive runtime dependencies
''',
)
replace(
    ".github/workflows/windows-msys2-qt5-profile.yml",
    '''            mingw-w64-x86_64-openssl
            mingw-w64-x86_64-usrsctp
''',
    '''            mingw-w64-x86_64-openssl
            mingw-w64-x86_64-libsrtp
            mingw-w64-x86_64-usrsctp
''',
)
replace(
    ".github/workflows/ubuntu.yml",
    '''            libssl-dev \\
            libtidy-dev \\
''',
    '''            libssl-dev \\
            libsrtp2-dev \\
            libtidy-dev \\
''',
)
replace(
    ".github/workflows/fedora.yml",
    '''            libotr-devel \\
            libtidy-devel \\
''',
    '''            libotr-devel \\
            libsrtp-devel \\
            libtidy-devel \\
''',
)
