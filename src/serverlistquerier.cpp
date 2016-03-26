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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QStringList>

#include "serverlistquerier.h"

// #define XML_SERVER_LIST
#define SERVERLIST_MAX_REDIRECT  5

// legacy format could be found here as well
// https://list.jabber.at/api/?format=services-full.xml
// original http://xmpp.org/services/services.xml does not work anymore (checked on 2016-03-27)

ServerListQuerier::ServerListQuerier(QObject* parent) : QObject(parent)
{
	http_ = new QNetworkAccessManager(this);
	url_ = QUrl("https://xmpp.net/directory.php");
}

void ServerListQuerier::getList()
{
	redirectCount_ = 0;
	QNetworkReply *reply = http_->get(QNetworkRequest(url_));
	connect(reply, SIGNAL(finished()), SLOT(get_finished()));
}

void ServerListQuerier::get_finished()
{
	QNetworkReply *reply = static_cast<QNetworkReply *>(sender());

	if (reply->error()) {
		emit error(reply->errorString());
	}
	else {
		int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if(status == 200) {
#ifdef XML_SERVER_LIST
			// Parse the XML file
			QDomDocument doc;
			if (!doc.setContent(reply->readAll())) {
				emit error(tr("Unable to parse server list"));
				return;
			}

			// Fill the list
			QStringList servers;
			QDomNodeList items = doc.elementsByTagName("item");
			for (int i = 0; i < items.count(); i++) {
				QString jid = items.item(i).toElement().attribute("jid");
				if (!jid.isEmpty()) {
					servers.push_back(jid);
				}
			}
#else
			QStringList servers;
			QString contents = QString::fromUtf8(reply->readAll());
			int index = 0;
			QRegExp re("data-original-title=\"([^\"]+)\"");
			while ((index = contents.indexOf(re, index + 1)) != -1) {
				servers.append(re.cap(1));
			}
#endif
			emit listReceived(servers);
		}
		else if(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
			if (redirectCount_ >= SERVERLIST_MAX_REDIRECT) {
				emit error(tr("Maximum redirect count reached"));
				return;
			}

			url_ = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).value<QUrl>().resolved(url_);
			if (url_.isValid()) {
				QNetworkReply *newReply = http_->get(QNetworkRequest(url_));
				connect(newReply, SIGNAL(finished()),SLOT(get_finished()));
				++redirectCount_;
			} else {
				emit error(tr("Invalid redirect URL %1").arg(url_.toString()));
				return;
			}
		}
		else {
			emit error(tr("Unexpected HTTP status code: %1").arg(status));
		}
	}
	reply->deleteLater();
}
