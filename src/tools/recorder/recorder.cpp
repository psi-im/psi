/*
 * recorder.cpp - Sound recorder
 * Copyright (C) 2019 Sergey Ilinykh, Vitaly Tonkacheyev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "recorder.h"
#include "3rdparty/qite/libqite/qiteaudiorecorder.h"
#include "applicationinfo.h"

#include <QAudioRecorder>
#include <QStandardPaths>
#include <QFile>
#include <QDate>

Recorder::Recorder(QObject *parent)
    : QObject(parent)
    , audioRecorder_(nullptr)
    , recFileName_(QString())
{
}

Recorder::~Recorder()
{
    cleanUp();
}

void Recorder::record()
{
    cleanUp();
    const QString tmpPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    recFileName_ = QString("%1/psi_tmp_record_%2.ogg")
                    .arg(ApplicationInfo::documentsDir())
                    .arg(QDateTime::currentDateTimeUtc().toString("dd_MM_yyyy_hh_mm_ss"));
    audioRecorder_ = std::unique_ptr<AudioRecorder>(new AudioRecorder(parent()));
    connect(audioRecorder_.get(), &AudioRecorder::stateChanged, this, [this](){
        if (audioRecorder_->recorder()->state() == QAudioRecorder::StoppedState) {
            emit recordingStopped(data(), recFileName_);
        }
    });

    if (audioRecorder_->recorder()->state() == QAudioRecorder::StoppedState) {
        audioRecorder_->record(recFileName_);
    }
}

void Recorder::stop()
{
    if(!audioRecorder_)
        return;
    if (audioRecorder_->recorder()->state() == QAudioRecorder::RecordingState) {
        audioRecorder_->stop();
    }
}

QByteArray Recorder::data() const
{
    QByteArray result;
    if(!recFileName_.isEmpty()) {
        QFile file(recFileName_);
        file.open(QIODevice::ReadOnly);
        result = file.readAll();
        file.close();
    }
    return result;
}

void Recorder::cleanUp()
{
    if(QFile::exists(recFileName_)) {
        QFile::remove(recFileName_);
    }
    if(audioRecorder_) {
        audioRecorder_->disconnect();
        audioRecorder_.reset();
    }
}
