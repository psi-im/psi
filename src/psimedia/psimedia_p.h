/*
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2008-2009  Barracuda Networks, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <QCoreApplication>
#ifdef QT_GUI_LIB
#    include <QPainter>
#endif
#include <QPluginLoader>

#include "psimedia.h"
#include "psimediaprovider.h"

namespace PsiMedia {
#ifdef QT_GUI_LIB
//----------------------------------------------------------------------------
// VideoWidget
//----------------------------------------------------------------------------
class VideoWidgetPrivate : public QObject, public VideoWidgetContext
{
    Q_OBJECT

public:
    friend class VideoWidget;

    VideoWidget *q;
    QSize videoSize;

    VideoWidgetPrivate(VideoWidget *_q) :
        QObject(_q),
        q(_q)
    {
    }

    virtual QObject *qobject()
    {
        return this;
    }

    virtual QWidget *qwidget()
    {
        return q;
    }

    virtual void setVideoSize(const QSize &size)
    {
        videoSize = size;
        emit q->videoSizeChanged();
    }

signals:
    void resized(const QSize &newSize);
    void paintEvent(QPainter *p);
};

#endif

Provider *provider();
QList<Device> importDevices(const QList<PDevice> &in);
QList<AudioParams> importAudioModes(const QList<PAudioParams> &in);
QList<VideoParams> importVideoModes(const QList<PVideoParams> &in);

class Features::Private : public QObject
{
    Q_OBJECT

public:
    Features *q;
    FeaturesContext *c = nullptr;

    QList<Device> audioOutputDevices;
    QList<Device> audioInputDevices;
    QList<Device> videoInputDevices;
    QList<AudioParams> supportedAudioModes;
    QList<VideoParams> supportedVideoModes;

    Private(Features *_q) :
        QObject(_q),
        q(_q)
    {
        if (provider()->isInitialized()) {
            providerInitialized();
        } else {
            connect(provider()->qobject(), SIGNAL(initialized()), this, SLOT(providerInitialized()));
        }
    }

    ~Private()
    {
        delete c;
    }

    void clearResults()
    {
        audioOutputDevices.clear();
        audioInputDevices.clear();
        videoInputDevices.clear();
        supportedAudioModes.clear();
        supportedVideoModes.clear();
    }

    void importResults(const PFeatures &in)
    {
        audioOutputDevices = importDevices(in.audioOutputDevices);
        audioInputDevices = importDevices(in.audioInputDevices);
        videoInputDevices = importDevices(in.videoInputDevices);
        supportedAudioModes = importAudioModes(in.supportedAudioModes);
        supportedVideoModes = importVideoModes(in.supportedVideoModes);
    }

private slots:
    void providerInitialized()
    {
        c = provider()->createFeatures();
        c->qobject()->setParent(this);
        connect(c->qobject(), SIGNAL(updated()), SLOT(c_updated()));
        importResults(c->results());
    }

    void c_updated()
    {
        importResults(c->results());
        emit q->updated();
    }
};

//----------------------------------------------------------------------------
// RtpChannel
//----------------------------------------------------------------------------
class RtpChannelPrivate : public QObject
{
    Q_OBJECT

public:
    RtpChannel *q;
    RtpChannelContext *c;
    bool enabled;
    int readyReadListeners;

    RtpChannelPrivate(RtpChannel *_q) :
        QObject(_q),
        q(_q),
        c(nullptr),
        enabled(false),
        readyReadListeners(0)
    {
    }

    void setContext(RtpChannelContext *_c)
    {
        if(c)
        {
            c->qobject()->disconnect(this);
            c->qobject()->setParent(nullptr);
            enabled = false;
            c = nullptr;
        }

        if(!_c)
            return;

        c = _c;
        c->qobject()->setParent(this);
        connect(c->qobject(), SIGNAL(readyRead()), SLOT(c_readyRead()));
        connect(c->qobject(), SIGNAL(packetsWritten(int)), SLOT(c_packetsWritten(int)));
        connect(c->qobject(), SIGNAL(destroyed()), SLOT(c_destroyed()));

        if(readyReadListeners > 0)
        {
            enabled = true;
            c->setEnabled(true);
        }
    }

private slots:
    void c_readyRead()
    {
        emit q->readyRead();
    }

    void c_packetsWritten(int count)
    {
        emit q->packetsWritten(count);
    }

    void c_destroyed()
    {
        enabled = false;
        c = nullptr;
    }
};

//----------------------------------------------------------------------------
// RtpSession
//----------------------------------------------------------------------------
class RtpSessionPrivate : public QObject
{
    Q_OBJECT

public:
    RtpSession *q;
    RtpSessionContext *c;
    RtpChannel audioRtpChannel;
    RtpChannel videoRtpChannel;

    RtpSessionPrivate(RtpSession *_q) :
        QObject(_q),
        q(_q)
    {
        c = provider()->createRtpSession();
        c->qobject()->setParent(this);
        connect(c->qobject(), SIGNAL(started()), SLOT(c_started()));
        connect(c->qobject(), SIGNAL(preferencesUpdated()), SLOT(c_preferencesUpdated()));
        connect(c->qobject(), SIGNAL(audioOutputIntensityChanged(int)), SLOT(c_audioOutputIntensityChanged(int)));
        connect(c->qobject(), SIGNAL(audioInputIntensityChanged(int)), SLOT(c_audioInputIntensityChanged(int)));
        connect(c->qobject(), SIGNAL(stoppedRecording()), SLOT(c_stoppedRecording()));
        connect(c->qobject(), SIGNAL(stopped()), SLOT(c_stopped()));
        connect(c->qobject(), SIGNAL(finished()), SLOT(c_finished()));
        connect(c->qobject(), SIGNAL(error()), SLOT(c_error()));
    }

    ~RtpSessionPrivate()
    {
        delete c;
    }

private slots:
    void c_started()
    {
        audioRtpChannel.d->setContext(c->audioRtpChannel());
        videoRtpChannel.d->setContext(c->videoRtpChannel());
        emit q->started();
    }

    void c_preferencesUpdated()
    {
        emit q->preferencesUpdated();
    }

    void c_audioOutputIntensityChanged(int intensity)
    {
        emit q->audioOutputIntensityChanged(intensity);
    }

    void c_audioInputIntensityChanged(int intensity)
    {
        emit q->audioInputIntensityChanged(intensity);
    }

    void c_stoppedRecording()
    {
        emit q->stoppedRecording();
    }

    void c_stopped()
    {
        audioRtpChannel.d->setContext(nullptr);
        videoRtpChannel.d->setContext(nullptr);
        emit q->stopped();
    }

    void c_finished()
    {
        audioRtpChannel.d->setContext(nullptr);
        videoRtpChannel.d->setContext(nullptr);
        emit q->finished();
    }

    void c_error()
    {
        audioRtpChannel.d->setContext(nullptr);
        videoRtpChannel.d->setContext(nullptr);
        emit q->error();
    }
};

} // namespace
