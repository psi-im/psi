// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef CALLVIEW_H
#define CALLVIEW_H

#include <QList>
#include <QString>
#include <QWidget>
class QLabel;
class QPushButton;
class QToolButton;
class QMenu;
class QActionGroup;
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
    struct InputDevice {
        QString id;
        QString name;
        bool    isDefault = false;
    };
    void                   setInputDevices(const QList<InputDevice> &microphones, const QString &microphoneId,
                                           const QList<InputDevice> &cameras, const QString &cameraId);
    void                   setPeer(const QString &peer);
    QString                peer() const;
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
    void                   setSharePrepared(const QString &source);
    void                   setShareSelecting(bool selecting);
    void                   setDuration(qint64 seconds);
    bool                   cameraEnabled() const;
    bool                   microphoneEnabled() const;
    int                    bitrate() const;
    PsiMedia::VideoWidget *remoteVideo() const;
    PsiMedia::VideoWidget *remotePresentation() const;
    CallStage             *stage() const;
    // A session starting with a whiteboard; capture may be selected before media joins.
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

signals:
    void headingChanged();
    void accepted();
    void ended();
    void microphoneChanged(bool enabled);
    void cameraChanged(bool enabled);
    void microphoneDeviceSelected(const QString &id);
    void cameraDeviceSelected(const QString &id);
    void shareRequested();
    void whiteboardChanged(bool enabled);

private:
    QString                peer_;
    QLabel                *security_;
    QLabel                *sharingHint_;
    QPushButton           *microphone_;
    QPushButton           *camera_;
    QToolButton           *microphoneDevices_;
    QToolButton           *cameraDevices_;
    QMenu                 *microphoneMenu_;
    QMenu                 *cameraMenu_;
    QActionGroup          *microphoneDeviceGroup_ = nullptr;
    QActionGroup          *cameraDeviceGroup_     = nullptr;
    bool                   microphonesAvailable_  = false;
    bool                   camerasAvailable_      = false;
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
    bool                   shareSupported_ = false;
    bool                   videoSupported_ = true;
    bool                   whiteboardOnly_ = false;
    bool                   whiteboardSupported_ = false;
    bool                   finished_       = false;
    bool                   connecting_     = false;
    bool                   mediaAvailable_ = true;
    bool                   updatingAppearance_ = false;
    QString                elapsedText_ = QStringLiteral("00:00:00");
    QString                shareSource_;
    bool                   shareConnecting_ = false;
    bool                   sharePrepared_   = false;
};
#endif
