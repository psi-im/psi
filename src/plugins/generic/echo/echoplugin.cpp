/*
 * echoplugin.cpp - Psi plugin to echo messages
 * Copyright (C) 2006  Kevin Smith
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
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

#include <QtCore>

#include "psiplugin.h"

class EchoPlugin : public PsiPlugin
{
	Q_OBJECT
	Q_INTERFACES(PsiPlugin)

public:
	virtual QString name() const; 
	virtual void message( const PsiAccount* account, const QString& message, const QString& fromJid, const QString& fromDisplay); 
	virtual QString shortName() const;
	virtual QString version() const;
};

Q_EXPORT_PLUGIN(EchoPlugin);

QString EchoPlugin::name() const
{
	return "Echo Plugin";
}

QString EchoPlugin::shortName() const
{
	return "echo";
}

QString EchoPlugin::version() const
{
	return "0.1.0";
}

void EchoPlugin::message( const PsiAccount* account, const QString& message, const QString& fromJid, const QString& fromDisplay)
{
	qWarning(qPrintable(QString("Received message '%1', echoing back to %2").arg(message).arg(fromDisplay)));
	QVariant option;
	emit getGlobalOption(message, &option);
	QString reply;
	if (option.isValid())
		reply=QString("<message to=\"%1\" type=\"chat\"><body>Option %2 = %3 </body></message>").arg(fromJid).arg(message).arg(option.toString());
	else
		reply=QString("<message to=\"%1\"><body>What you say? %2 ?</body></message>").arg(fromJid).arg(message);
	
	emit sendStanza(account, reply);
}	

#include "echoplugin.moc"
