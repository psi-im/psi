// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef AVCALLAUDIODIRECTION_H
#define AVCALLAUDIODIRECTION_H

#include <iris/jingle-rtp.h>

// Call-owned adapter of explicit user consent/device facts to Iris policy.
// No signaling, retry or negotiated-direction state is duplicated here.
class AvCallAudioDirection : public QObject {
    Q_OBJECT
public:
    explicit AvCallAudioDirection(QObject *parent = nullptr, int deadlineMs = 15000);
    ~AvCallAudioDirection() override;
    void bind(XMPP::Jingle::RTP::Application *, bool localSendingWanted, bool captureAvailable);
    void setLocalSending(bool); // explicit user action, never a peer notification
    void setCaptureAvailable(bool);
    bool allowsCapture(const XMPP::Jingle::RTP::Application *) const;
    const XMPP::Jingle::RTP::DirectionOperation *operation() const { return operation_.get(); }

signals:
    // Reapply capture controls immediately for local changes and on queued policy outcomes.
    void changed();

private:
    void                                                                clear();
    void                                                                observe(bool desired);
    QPointer<XMPP::Jingle::RTP::Application>                            content_;
    QPointer<XMPP::Jingle::RTP::DirectionController>                    controller_;
    std::unique_ptr<XMPP::Jingle::RTP::DirectionController::Constraint> unavailable_;
    std::unique_ptr<XMPP::Jingle::RTP::DirectionOperation>              operation_;
    bool                                                                captureAvailable_ = false;
    int                                                                 deadlineMs_;
};
#endif
