/*
 * recorder.h - Sound recorder
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

#ifndef RECORDER_H
#define RECORDER_H

#include <QObject>
#include <memory>

class AudioRecorder;

class VoiceRecorder: public QObject
{
    Q_OBJECT
public:
    VoiceRecorder(QObject *parent = nullptr);
    ~VoiceRecorder();
    void record();
    void stop();

signals:
    void recordingStopped(const QByteArray &data, const QString &file);

private:
    void cleanUp();
    QByteArray data() const;

private:
    std::unique_ptr<AudioRecorder> audioRecorder_;
    QString recFileName_;
};

#endif //RECORDER_H
