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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <QtCore>

#include "psiplugin.h"
#include "eventfilter.h"
#include "stanzasender.h"
#include "stanzasendinghost.h"
#include "optionaccessor.h"
#include "optionaccessinghost.h"

class EchoPlugin : public QObject, public PsiPlugin, public EventFilter, public StanzaSender, public OptionAccessor
{
	Q_OBJECT
	Q_INTERFACES(PsiPlugin EventFilter StanzaSender OptionAccessor)

public:
	EchoPlugin();

	// PsiPlugin
	virtual QString name() const;
	virtual QString shortName() const;
	virtual QString version() const;
	virtual QWidget* options() const;
	virtual bool enable();
	virtual bool disable();

	// EventFilter
    virtual bool processEvent(int account, const QDomElement& e);
	virtual bool processMessage(int account, const QString& fromJid, const QString& body, const QString& subject);

	// StanzaSender
	virtual void setStanzaSendingHost(StanzaSendingHost *host);

	// OptionAccessor
	virtual void setOptionAccessingHost(OptionAccessingHost* host);
	virtual void optionChanged(const QString& option);



private:
	bool enabled;
	OptionAccessingHost* psiOptions;
	StanzaSendingHost* sender;
};

Q_EXPORT_PLUGIN(EchoPlugin);

EchoPlugin::EchoPlugin()
	: enabled(false), psiOptions(0), sender(0)
{
}

//-- PsiPlugin ------------------------------------------------------

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
	return "0.1";
}

QWidget* EchoPlugin::options() const
{
	return 0;
}

bool EchoPlugin::enable()
{
	if (psiOptions && sender) {
		enabled = true;
	}

	return enabled;
}

bool EchoPlugin::disable()
{
	enabled = false;
	return true;
}

//-- EventFilter ----------------------------------------------------

bool EchoPlugin::processEvent(int account, const QDomElement& e)
{
	Q_UNUSED(account);
	Q_UNUSED(e);
	return false;	// don't stop processing
}

bool EchoPlugin::processMessage(int account, const QString& fromJid, const QString& body, const QString& subject)
{
	if (enabled) {
		qWarning(qPrintable(QString("Received message '%1', echoing back to %2").arg(body).arg(fromJid)));
		QVariant option = psiOptions->getGlobalOption(body);
		QString reply;
		QString type;
		if (option.isValid()) {
			reply = QString("Option %1 = %2").arg(body).arg(option.toString());
			type = "chat";
		} else {
			reply = QString("What you say? %1 ?").arg(body);
			type = "normal";
		}
		QString replySubject = QString("Re: %1").arg(subject);

		sender->sendMessage(account, fromJid, reply, replySubject, type);
	}

	return false;
}

//-- StanzaSender ---------------------------------------------------

void EchoPlugin::setStanzaSendingHost(StanzaSendingHost *host)
{
	sender = host;
}

//-- OptionAccessor -------------------------------------------------

void EchoPlugin::setOptionAccessingHost(OptionAccessingHost* host)
{
	psiOptions = host;
}

void EchoPlugin::optionChanged(const QString& option)
{
	Q_UNUSED(option);
}


#include "echoplugin.moc"
