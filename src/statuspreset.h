/*
 * statuspreset.h
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

#ifndef STATUSPRESET_H
#define STATUSPRESET_H

#include "maybe.h"
#include "optionstree.h"
#include "xmpp_status.h"

#include <QString>

class QDomDocument;
class QDomElement;

class StatusPreset {
public:
    StatusPreset();
    StatusPreset(QString name, QString message = QString(), XMPP::Status::Type status = XMPP::Status::Away);
    StatusPreset(QString name, int priority, QString message = QString(),
                 XMPP::Status::Type status = XMPP::Status::Away);
    StatusPreset(const QDomElement &);

    QString            name() const;
    void               setName(const QString &);
    QString            message() const;
    void               setMessage(const QString &);
    XMPP::Status::Type status() const;
    void               setStatus(XMPP::Status::Type);
    Maybe<int>         priority() const;
    void               setPriority(int priority);
    void               setPriority(const QString &priority);
    void               clearPriority();
    void               filterStatus();

    void        toOptions(OptionsTree *o);
    void        fromOptions(OptionsTree *o, QString name);
    QDomElement toXml(QDomDocument &) const;
    void        fromXml(const QDomElement &);

private:
    QString            name_, message_;
    XMPP::Status::Type status_;
    Maybe<int>         priority_;
};

#endif // STATUSPRESET_H
