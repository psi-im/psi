/*
 * psiwkavatarhandler.h - "avatar" schema handler for for network manager
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

#ifndef PSIWKAVATARHANDLER_H
#define PSIWKAVATARHANDLER_H

#include "networkaccessmanager.h"
#include "psicon.h"

class PsiWKAvatarHandler : public NAMSchemeHandler
{
public:
	PsiWKAvatarHandler(PsiCon *pc);
	QByteArray data(const QUrl &url) const;
	void setDefaultAvatar(const QString &filename, const QString &host = "");
	void setDefaultAvatar(const QByteArray &ba, const QString &host = "");
	void setAvatarSize(const QSize &);

private:
	QMap<QString,QPixmap> defaultAvatar_;
	PsiCon *psi_;
	QSize size_;
};

#endif
