/*
 * tabbable.h
 * Copyright (C) 2007 Kevin Smith
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

#ifndef TABBABLE_H
#define TABBABLE_H

#include "advwidget.h"
#include "im.h" // ChatState

namespace XMPP {
	class Jid;
	class Message;
}
using namespace XMPP;

class PsiAccount;

class Tabbable : public AdvancedWidget<QWidget>
{
	Q_OBJECT
public:
	Tabbable(const Jid &, PsiAccount *);
	~Tabbable();

	virtual Jid jid() const; 
	virtual const QString & getDisplayName();

	virtual bool readyToHide();

signals:
	void eventsRead(const Jid &);
	void captionChanged(QString);
	void contactStateChanged( XMPP::ChatState );
	void unreadEventUpdate(int);

public slots:
	virtual void activated();

private:
	Jid jid_;
	PsiAccount *pa_;
};

#endif
