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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef SIMPLECLI_H
#define SIMPLECLI_H

#include <QMap>
#include <QObject>
#include <QStringList>

class SimpleCli : public QObject {
public:
    void defineSwitch(const QByteArray &name, const QString &help = QString());
    void defineParam(const QByteArray &name, const QString &valueHelp = QString("ARG"),
                     const QString &help = QString());

    void defineAlias(const QByteArray &alias, const QByteArray &originalName);

    QHash<QByteArray, QByteArray>
    parse(int argc, char *argv[], const QList<QByteArray> &terminalArgs = QList<QByteArray>(), int *safeArgc = nullptr);

    QString        optionsHelp(int textWidth);
    static QString wrap(QString text, int width, int margin = 0, int firstMargin = -1);

private:
    struct Arg {
        QByteArray        name;
        QList<QByteArray> aliases;
        QChar             shortName;

        bool needsValue;

        QString help;
        QString valueHelp;

        Arg(const QByteArray &argName, const QString &argValueHelp, const QString &argHelp, bool argNeedsValue) :
            name(argName), needsValue(argNeedsValue), help(argHelp), valueHelp(argValueHelp)
        {
        }

        Arg() : needsValue(false) { } // needed only by QMap
    };

    QMap<QByteArray, Arg>        argdefs;
    QMap<QByteArray, QByteArray> aliases;
};

#endif // SIMPLECLI_H
