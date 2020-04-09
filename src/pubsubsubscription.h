/*
 * pubsubsubscription.h
 * Copyright (C) 2006  Remko Troncon
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

#ifndef PUBSUBSUBSCRIPTION_H
#define PUBSUBSUBSCRIPTION_H

#include <QString>

class QDomDocument;
class QDomElement;

class PubSubSubscription {
public:
    enum State { None, Pending, Unconfigured, Subscribed };

    PubSubSubscription();
    PubSubSubscription(const QDomElement &e);

    const QString &jid() const;
    const QString &node() const;
    State          state() const;

    bool        isNull() const;
    void        fromXml(const QDomElement &e);
    QDomElement toXml(QDomDocument &doc) const;

    bool operator==(const PubSubSubscription &) const;
    bool operator!=(const PubSubSubscription &) const;

private:
    QString jid_, node_;
    State   state_;
};

#endif // PUBSUBSUBSCRIPTION_H
