/*
 * psiwkavatarhandler.cpp - "avatar" schema handler for for network manager
 * Copyright (C) 2010 Rion
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

#include "psiwkavatarhandler.h"

#include <QBuffer>

#include "iconset.h"
#include "avatars.h"
#include "psicontactlist.h"
#include "psiaccount.h"

PsiWKAvatarHandler::PsiWKAvatarHandler(PsiCon *pc)
	: psi_(pc)
{
	defaultAvatar_[""] = IconsetFactory::icon("psi/default_avatar").pixmap();
	size_ = defaultAvatar_[""].size();
}

QByteArray PsiWKAvatarHandler::data(const QUrl &url) const {
	QStringList parts = url.path().split("/");
	qDebug("loading avatar");
	PsiAccount *ac;
	if (parts.size() > 0 && parts[0].isEmpty()) { // first / makes empty string
		parts.removeFirst();
	}
	if (parts.count() == 2 && (ac = psi_->contactList()->getAccount(parts[0]))) {
		QPixmap p = ac->avatarFactory()->getAvatar(parts[1]);
		if (p.isNull()) {
			if (!url.host().isEmpty() && defaultAvatar_.value(url.host()).isNull()) {
				p = defaultAvatar_.value("");
			} else {
				p = defaultAvatar_.value(url.host());
			}
			if (p.isNull()) {
				p = IconsetFactory::icon("psi/default_avatar").pixmap();
			}
		}
		QByteArray ba;
		QBuffer buffer(&ba);
		buffer.open(QIODevice::WriteOnly);
		p.scaled(size_, Qt::KeepAspectRatio, Qt::SmoothTransformation).toImage().save(&buffer, "PNG");
		return ba;
	}
	return QByteArray();
}

void PsiWKAvatarHandler::setDefaultAvatar(const QString &filename, const QString &host)
{
	defaultAvatar_[host] = QPixmap(filename);
	size_ = defaultAvatar_[host].size();
}

void PsiWKAvatarHandler::setDefaultAvatar(const QByteArray &ba, const QString &host)
{
	defaultAvatar_[host] = QPixmap();
	defaultAvatar_[host].loadFromData(ba);
	size_ = defaultAvatar_[host].size();
}

void PsiWKAvatarHandler::setAvatarSize(const QSize &size)
{
	size_ = size;
}
