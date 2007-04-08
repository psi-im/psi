/*
 * snarlplugin.cpp - Psi plugin to display notifs via snarl
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
#include <QMessageBox>

#include "psiplugin.h"
#include "SnarlInterface.h"

class SnarlPlugin : public QObject, public PsiPlugin
{
	Q_OBJECT
	Q_INTERFACES(PsiPlugin)

public:
	SnarlPlugin();
	~SnarlPlugin();
	virtual QString name() const;
	virtual QString shortName() const;
	virtual void message( const QString& message, const QString& fromJid, const QString& fromDisplay); 
private:
	SnarlInterface* snarl_;

};

Q_EXPORT_PLUGIN(SnarlPlugin);

SnarlPlugin::SnarlPlugin() : PsiPlugin()
{
	snarl_ = new SnarlInterface();
	int major=0;
	int minor=0;
	snarl_->snGetVersion(&major, &minor);
	if (major==0 && minor==0)
		QMessageBox::information(0,"Snarl",QString("Snarl is not running, so notifications are disabled until it is started"));
}

SnarlPlugin::~SnarlPlugin()
{
	delete snarl_;
}

QString SnarlPlugin::name() const
{
	return "Snarl Plugin";
}

QString SnarlPlugin::shortName() const
{
	return "snarl";
}

void SnarlPlugin::message( const QString& message, const QString& fromJid, const QString& fromDisplay)
{
	QString text=QString("%1 says\n %2").arg(fromDisplay).arg(message);
	QString caption=QString("Received message");
	QString icon= "D:\\devel\\psi-plugins\\win32\\psi\\iconsets\\system\\default\\icon_48.png";
	snarl_->snShowMessage(caption.toStdString(), text.toStdString(), 10, icon.toStdString(), 0, 0);
}	

#include "snarlplugin.moc"
