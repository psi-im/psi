/*
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
    bool available() const { return backendAvailable && probeComplete && secureRtp && (audio || video); }
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
