/*
 * serverlistquerier.cpp
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

#include "serverlistquerier.h"

#ifdef XML_SERVER_LIST
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#else
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#endif
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

// #define XML_SERVER_LIST
#define SERVERLIST_MAX_REDIRECT 5

// legacy format could be found here as well
// https://list.jabber.at/api/?format=services-full.xml
// original http://xmpp.org/services/services.xml does not work anymore (checked on 2016-03-27)

ServerListQuerier::ServerListQuerier(QObject *parent) : QObject(parent), redirectCount_(0)
{
    http_ = new QNetworkAccessManager(this);
    url_  = QUrl("https://data.xmpp.net/providers/v2/providers-A.json");
}

void ServerListQuerier::getList()
{
    redirectCount_       = 0;
    QNetworkReply *reply = http_->get(QNetworkRequest(url_));
    connect(reply, SIGNAL(finished()), SLOT(get_finished()));
}

void ServerListQuerier::get_finished()
{
    QNetworkReply *reply = static_cast<QNetworkReply *>(sender());

    if (reply->error()) {
        emit error(reply->errorString());
    } else {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200) {
#ifdef XML_SERVER_LIST
            // Parse the XML file
            QDomDocument doc;
            if (!doc.setContent(reply->readAll())) {
                emit error(tr("Unable to parse server list"));
                return;
            }

            // Fill the list
            QStringList  servers;
            QDomNodeList items = doc.elementsByTagName("item");
            for (int i = 0; i < items.count(); i++) {
                QString jid = items.item(i).toElement().attribute("jid");
                if (!jid.isEmpty()) {
                    servers.push_back(jid);
                }
            }
            emit listReceived(servers);
#else

            if (auto r = parseJson(reply->readAll()); r) {
                emit listReceived(*r);
            }
#endif
        } else {
            emit error(tr("Unexpected HTTP status code: %1").arg(status));
        }
    }
    reply->deleteLater();
}

std::optional<QStringList> ServerListQuerier::parseJson(const QByteArray &data)
{
    QStringList jidList;
    bool        parsingErrorOccurred = false;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (!jsonDoc.isArray()) {
        emit error(tr("Failed to parse json."));
        return {};
    }

    QJsonArray jsonArray = jsonDoc.array();

    for (const QJsonValue &providerValue : jsonArray) {
        if (!providerValue.isObject()) {
            parsingErrorOccurred = true;
            continue; // Skip if the item is not a valid object
        }

        QJsonObject providerObj = providerValue.toObject();
        auto        jidIt       = providerObj.find("jid");

        QString jid;
        if (jidIt == providerObj.end() || !jidIt->isString() || (jid = jidIt->toString()).isEmpty()) {
            parsingErrorOccurred = true;
            continue; // Skip if "jid" is not found or is not a string
        }
        jidList.append(jid);
    }

    // Emit an error if the list is empty and there was a parsing error
    if (jidList.isEmpty() && parsingErrorOccurred) {
        emit error(tr("Failed to parse any valid server JIDs from %1.").arg(url_.toDisplayString()));
    }

    return jidList;
}
