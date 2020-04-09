/*
 * conferencebookmark.cpp
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

#include "conferencebookmark.h"

#include "xmpp_xmlcommon.h"

#include <QDomDocument>
#include <QDomElement>
#include <QObject>
#include <QStringList>
#include <QVector>

ConferenceBookmark::ConferenceBookmark(const QString &name, const XMPP::Jid &jid, JoinType auto_join,
                                       const QString &nick, const QString &password) :
    name_(name),
    jid_(jid), auto_join_(auto_join), nick_(nick), password_(password)
{
}

ConferenceBookmark::ConferenceBookmark(const QDomElement &el) { fromXml(el); }

QStringList ConferenceBookmark::joinTypeNames()
{
    static QStringList jtn;
    if (jtn.isEmpty()) {
        QVector<QString> tmp(LastJoinType);
        tmp[Never]              = QObject::tr("Never");
        tmp[Always]             = QObject::tr("Always");
        tmp[OnlyThisComputer]   = QObject::tr("This computer only");
        tmp[ExceptThisComputer] = QObject::tr("Except this computer");
        jtn                     = tmp.toList();
    }
    return jtn;
}

const QString &ConferenceBookmark::name() const { return name_; }

const XMPP::Jid &ConferenceBookmark::jid() const { return jid_; }

void ConferenceBookmark::setAutoJoin(JoinType type) { auto_join_ = type; }

ConferenceBookmark::JoinType ConferenceBookmark::autoJoin() const { return auto_join_; }

bool ConferenceBookmark::needJoin() const
{
    return auto_join_ == ConferenceBookmark::Always || auto_join_ == ConferenceBookmark::OnlyThisComputer;
}

const QString &ConferenceBookmark::nick() const { return nick_; }

const QString &ConferenceBookmark::password() const { return password_; }

bool ConferenceBookmark::isNull() const { return name_.isEmpty() && jid_.isEmpty(); }

void ConferenceBookmark::fromXml(const QDomElement &e)
{
    jid_       = e.attribute("jid");
    name_      = e.attribute("name");
    auto_join_ = Never;
    if (e.attribute("autojoin") == "true" || e.attribute("autojoin") == "1")
        auto_join_ = Always;

    for (QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
        QDomElement i = n.toElement();
        if (i.isNull())
            continue;
        else if (i.tagName() == "nick") {
            nick_ = i.text();
        } else if (i.tagName() == "password") {
            password_ = i.text();
        }
    }
}

QDomElement ConferenceBookmark::toXml(QDomDocument &doc) const
{
    QDomElement e = doc.createElement("conference");
    e.setAttribute("jid", jid_.full());
    e.setAttribute("name", name_);
    if (auto_join_ == Always || auto_join_ == ExceptThisComputer)
        e.setAttribute("autojoin", "true");
    if (!nick_.isEmpty())
        e.appendChild(textTag(&doc, "nick", nick_));
    if (!password_.isEmpty())
        e.appendChild(textTag(&doc, "password", password_));

    return e;
}

bool ConferenceBookmark::operator==(const ConferenceBookmark &other) const
{
    return name_ == other.name_ && jid_.full() == other.jid_.full() && auto_join_ == other.auto_join_
        && nick_ == other.nick_ && password_ == other.password_;
}
