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

#include <QAudioRecorder>
#include <QStandardPaths>
#include <QFile>

Recorder::Recorder(QObject *parent)
    : QObject(parent)
    , audioRecorder_(nullptr)
    , recFile_(QString())
    , dataObtained_(false)
{

}

Recorder::~Recorder()
{
}

void Recorder::record()
{
    const QString tmpPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    recFile_ = tmpPath + "/psi_tmp_record.ogg";
    audioRecorder_ = std::unique_ptr<AudioRecorder>(new AudioRecorder(parent()));
    connect(audioRecorder_.get(), &AudioRecorder::stateChanged, this, [this](){
        if (audioRecorder_->recorder()->state() == QAudioRecorder::StoppedState) {
            emit recordingStopped(recFile_);
        }
    });

    if (audioRecorder_->recorder()->state() == QAudioRecorder::StoppedState) {
        audioRecorder_->record(recFile_);
    }
    dataObtained_ = false;
}

void Recorder::stop()
{
    if(!audioRecorder_)
        return;
    if (audioRecorder_->recorder()->state() == QAudioRecorder::RecordingState) {
        audioRecorder_->stop();
        audioRecorder_->disconnect();
        audioRecorder_.reset();
    }
}

QByteArray Recorder::data()
{
    QByteArray result;
    if(!recFile_.isEmpty()) {
        QFile file(recFile_);
        file.open(QIODevice::ReadOnly);
        result = file.readAll();
        file.close();
        dataObtained_ = true;
    }
    return result;
}

void Recorder::cleanUp()
{
    if(QFile::exists(recFile_)) {
        QFile::remove(recFile_);
    }
}

bool Recorder::dataObtained()
{
    return dataObtained_;
}
