/*
 * Copyright (C) 2008 Martin Hostettler
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// manager mini command system

#ifndef MCMDMANAGER_H
#define MCMDMANAGER_H

#include <QList>
#include <QStringList>
#include <QVariant>
#include <QMap>
#include <QHash>
#include <minicmd.h>


// implementation in groupchatdlg.cpp
void MiniCommand_Depreciation_Message(const QString &old,const QString &newCmd, QString &line1, QString &line2);

class MCmdSimpleState : public QObject, public MCmdStateIface
{
	Q_OBJECT
public:
	MCmdSimpleState(QString name, QString prompt);
	MCmdSimpleState(QString name, QString prompt, int flags);

	virtual QString getName() { return name_;};

	virtual QString getPrompt() { return prompt_;};

	virtual int getFlags() { return flags_;};

	virtual const QHash<QString, QVariant> &getInfo() { return info_; };

	virtual void dispose() { delete(this); };

	virtual ~MCmdSimpleState();


	QHash<QString, QVariant> info_;

signals:
	bool unhandled(QStringList command);

protected:
	QString name_, prompt_;
	int flags_;
};


class MCmdManager : public MCmdManagerIface
{
public:
	// UiSite -> Manager
	MCmdManager(MCmdUiSiteIface* site_);
	~MCmdManager();

	virtual bool processCommand(QString command);
	virtual QStringList completeCommand(QString &command, int pos, int &start, int &end);
	virtual bool open(MCmdStateIface *state, QStringList preset);

	virtual bool isActive();


	// Provider registratation
	virtual void registerProvider(MCmdProviderIface *prov);

protected:
	QStringList parseCommand(const QString command, int pos, int &part, QString &partial, int &start, int &end, char &quotedAtPos);
	QString serializeCommand(const QStringList &list);

	QList<MCmdProviderIface*> providers_;
	MCmdStateIface *state_;

	MCmdUiSiteIface *uiSite_;
};

#endif
