/*
 * consoledump.cpp - Psi plugin to dump stream to console
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <QtCore>

#include "psiplugin.h"
#include "eventfilter.h"

class ConsoleDumpPlugin : public QObject, public PsiPlugin, public EventFilter
{
	Q_OBJECT
	Q_INTERFACES(PsiPlugin EventFilter)

public:
	ConsoleDumpPlugin();

	virtual QString name() const;
	virtual QString shortName() const;
	virtual QString version() const;
	virtual QWidget* options() const;
	virtual bool enable();
	virtual bool disable();

    virtual bool processEvent(int account, const QDomElement& e);
	virtual bool processMessage(int account, const QString& fromJid, const QString& body, const QString& subject) ;

private:
	bool enabled;
};

Q_EXPORT_PLUGIN(ConsoleDumpPlugin);

ConsoleDumpPlugin::ConsoleDumpPlugin()
	: enabled(false)
{
}

QString ConsoleDumpPlugin::name() const
{
	return "Console Dump Plugin";
}

QString ConsoleDumpPlugin::shortName() const
{
	return "consdump";
}

QString ConsoleDumpPlugin::version() const
{
	return "0.1";
}

QWidget* ConsoleDumpPlugin::options() const
{
	return 0;
}

bool ConsoleDumpPlugin::enable()
{
	enabled = true;
	return true;
}

bool ConsoleDumpPlugin::disable()
{
	enabled = false;
	return true;
}

bool ConsoleDumpPlugin::processEvent(int account, const QDomElement& e)
{
	Q_UNUSED(account);
	Q_UNUSED(e);
	return false;	// don't stop processing
}

bool ConsoleDumpPlugin::processMessage(int account, const QString& fromJid, const QString& body, const QString& subject)
{
	Q_UNUSED(account);
	Q_UNUSED(subject);
	if (enabled) {
		qWarning(qPrintable(QString("Received message from %1").arg(fromJid)));
		qWarning(qPrintable(body));
	}

	return false;	// don't stop processing
}

#include "consoledumpplugin.moc"
