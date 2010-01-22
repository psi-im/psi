/*
 * main.h - initialization and profile/settings handling
 * Copyright (C) 2001-2003  Justin Karneges
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

#ifndef MAIN_H
#define MAIN_H

#include <QString>
#include <QObject>
#include <QMap>

class PsiCon;

class PsiMain : public QObject
{
	Q_OBJECT
public:
	PsiMain(const QMap<QString, QString>& commandline, QObject *parent=0);
	~PsiMain();

	bool useActiveInstance();
	void useLocalInstance();

signals:
	void quit();

private slots:
	void chooseProfile();
	void sessionStart();
	void sessionQuit(int);
	void bail();

private:
	QString lastProfile, lastLang;
	bool autoOpen;
	QMap<QString, QString> cmdline;

	PsiCon *pcon;
};

#endif
