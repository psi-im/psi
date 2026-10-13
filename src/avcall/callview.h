// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef CALLVIEW_H
#define CALLVIEW_H

#include <QWidget>
class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class CallStage;
class QEvent;
namespace PsiMedia {
class VideoWidget;
}

class CallView : public QWidget {
    Q_OBJECT
public:
    explicit CallView(QWidget *parent = nullptr);
    void                   setPeer(const QString &peer, bool editable);
    QString                peer() const;
    void                   setStatus(const QString &text);
    // How the session is protected; an empty text hides it.
    void                   setSecurity(const QString &text, bool endToEnd, bool partiallyEncrypted = false);
    void                   setActive(bool active);
    void                   setConnecting(bool connecting);
    void                   setIncoming(bool incoming);
    void                   setVideoSupported(bool supported);
    void                   setSharingSupported(bool supported);
    void                   setCameraEnabled(bool enabled);
    void                   setFinished();
    void                   hideRemoteVideo(bool presentation);
    void                   setSharing(bool sharing, const QString &source = {}, bool connecting = false);
    void                   setShareSelecting(bool selecting);
    void                   setDuration(qint64 seconds);
    bool                   cameraEnabled() const;
    bool                   microphoneEnabled() const;
    int                    bitrate() const;
    PsiMedia::VideoWidget *remoteVideo() const;
    PsiMedia::VideoWidget *remotePresentation() const;
    CallStage             *stage() const;
    // A session without media: the whiteboard is the content, media controls are disabled.
    void setWhiteboardOnly(bool whiteboardOnly);
    bool whiteboardOnly() const;
    // Whether this side and the peer can use audio and video; without it the media buttons are disabled.
    void setMediaAvailable(bool available);
    void setMicrophoneChecked(bool enabled);
    void setWhiteboardSupported(bool supported);
    void setWhiteboardEnabled(bool enabled);
    bool whiteboardEnabled() const;

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateMediaButtons();
    void updateAppearance();
    void updateControlPresentation();
    void updatePeerVisibility();

signals:
    void headingChanged();
    void accepted();
    void ended();
    void microphoneChanged(bool enabled);
    void cameraChanged(bool enabled);
    void shareRequested();
    void whiteboardChanged(bool enabled);

private:
    QLineEdit             *peer_;
    QLabel                *status_;
    QLabel                *security_;
    QLabel                *duration_;
    QLabel                *sharingHint_;
    QPushButton           *microphone_;
    QPushButton           *camera_;
    QPushButton           *share_;
    QPushButton           *whiteboard_;
    QPushButton           *accept_;
    QPushButton           *end_;
    QComboBox             *quality_;
    CallStage             *stage_;
    PsiMedia::VideoWidget *video_;
    PsiMedia::VideoWidget *presentation_;
    bool                   active_         = false;
    bool                   sharing_        = false;
    bool                   selecting_      = false;
    bool                   shareSupported_ = true;
    bool                   videoSupported_ = true;
    bool                   whiteboardOnly_ = false;
    bool                   whiteboardSupported_ = false;
    bool                   finished_       = false;
    bool                   connecting_     = false;
    bool                   mediaAvailable_ = true;
    bool                   peerEditable_   = false;
    bool                   updatingAppearance_ = false;
    QString                shareSource_;
    bool                   shareConnecting_ = false;
};
#endif
