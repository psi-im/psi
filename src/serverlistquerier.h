/*
 * serverlistquerier.h
 * Copyright (C) 2007  Remko Troncon
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

#ifndef SERVERLISTQUERIER_H
#define SERVERLISTQUERIER_H

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;

class ServerListQuerier : public QObject {
    Q_OBJECT

public:
    ServerListQuerier(QObject *parent = nullptr);
    void getList();

signals:
    void listReceived(const QStringList &);
    void error(const QString &);

protected slots:
    void get_finished();

private:
    QNetworkAccessManager *http_;
    QUrl                   url_;
    int                    redirectCount_;
};

#endif // SERVERLISTQUERIER_H
