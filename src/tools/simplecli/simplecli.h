/*
 * simplecli.h - Simple CommandLine Interface parser / manager
 * Copyright (C) 2009  Maciej Niedzielski
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

#ifndef SIMPLECLI_H
#define SIMPLECLI_H

#include <QMap>
#include <QStringList>

class SimpleCli : public QObject
{
public:
	void defineSwitch(const QString& name, const QString& help = QString());
	void defineParam(const QString& name, const QString& valueHelp = QString("ARG"), const QString& help = QString());

	void defineAlias(const QString& alias, const QString& originalName);

	QMap<QString, QString> parse(int argc, char* argv[], const QStringList& terminalArgs = QStringList(), int* safeArgc = 0);

	QString optionsHelp(int textWidth);
	static QString wrap(QString text, int width, int margin = 0, int firstMargin = -1);

private:
	struct Arg {
		QString name;
		QStringList aliases;
		QChar shortName;

		bool needsValue;

		QString help;
		QString valueHelp;


		Arg(const QString& argName, const QString& argValueHelp, const QString& argHelp, bool argNeedsValue)
				: name(argName), needsValue(argNeedsValue), help(argHelp), valueHelp(argValueHelp) {}

		Arg() : needsValue(false) {}	// needed only by QMap
	};

	QMap<QString, Arg> argdefs;
	QMap<QString, QString> aliases;
};

#endif
