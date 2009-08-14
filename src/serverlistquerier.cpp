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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QHttp>
#include <QUrl>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>

#include "serverlistquerier.h"

#define SERVERLIST_SERVER        "xmpp.org"
#define SERVERLIST_PORT          80
#define SERVERLIST_PATH          "/services/services.xml"
#define SERVERLIST_MAX_REDIRECT  5

ServerListQuerier::ServerListQuerier(QObject* parent) : QObject(parent)
{
	http_ = new QHttp(SERVERLIST_SERVER, SERVERLIST_PORT, this);
	connect(http_,SIGNAL(requestFinished(int,bool)),SLOT(get_finished(int,bool)));
}

void ServerListQuerier::getList()
{
	redirectCount_ = 0;
	http_->get(SERVERLIST_PATH);
}

void ServerListQuerier::get_finished(int, bool err)
{
	const QHttpResponseHeader response = http_->lastResponse();

	if (err) {
		emit error(http_->errorString());
	}
	else {
		if(response.statusCode() == 200) {
			// Parse the XML file
			QDomDocument doc;
			if (!doc.setContent(http_->readAll())) {
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
			emit listReceived(servers);
		}
		else if(response.statusCode() == 301 || response.statusCode() == 302) {
			// handle redirections:
			// 301 = moved permanently
			// 302 = found
			if (redirectCount_ >= SERVERLIST_MAX_REDIRECT) {
				emit error(tr("Maximum redirect count reached"));
				return;
			}

			QString newUrl = response.value("Location");

			QUrl url = newUrl;
			if (!url.scheme().isEmpty()) {
				if (url.scheme() != "http") {
					emit error(tr("Redirect to protocol '%1' not supported").arg(url.scheme()));
					return;
				}

				if (url.host().isEmpty()) {
					emit error(tr("Cannot redirect to empty host"));
					return;
				}

				http_->disconnect(this);
				http_->deleteLater();
				http_ = new QHttp(url.host(), url.port(80), this);
				connect(http_,SIGNAL(requestFinished(int,bool)),SLOT(get_finished(int,bool)));
			}

			++redirectCount_;
			http_->get(newUrl);
		}
		else {
			emit error(tr("Unexpected HTTP status code: %1").arg(response.statusCode()));
		}
	}
}
