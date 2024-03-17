/*
 * simplecli.cpp - Simple CommandLine Interface parser / manager
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

#include "simplecli.h"

#include <QDebug>

/**
 * \class SimpleCli
 * \brief Simple Commandline Interface parser.
 *
 * This class allows you to define Commandline Interface for your application
 * and then use this information to convert argv to a map of options and their values.
 *
 * Please note that support for short options (-x) is very limited
 * and provided only to support options like -h and -v.
 */

/**
 * \brief Define a switch (option that does not have a value)
 */
void SimpleCli::defineSwitch(const QByteArray &name, const QString &help)
{
    argdefs[name] = Arg(name, "", help, false);
    aliases[name] = name;
}

/**
 * \brief Define a parameter (option that requires a value)
 */
void SimpleCli::defineParam(const QByteArray &name, const QString &valueHelp, const QString &help)
{
    argdefs[name] = Arg(name, valueHelp, help, true);
    aliases[name] = name;
}

/**
 * \brief Add alias for already existing option.
 * \a alias will be mapped to \a originalName in parse() result.
 */
void SimpleCli::defineAlias(const QByteArray &alias, const QByteArray &originalName)
{
    if (!argdefs.contains(originalName)) {
        qDebug("CLI: cannot add alias '%s' because name '%s' does not exist", alias.constData(),
               originalName.constData());
        return;
    }
    argdefs[originalName].aliases.append(alias);
    aliases[alias] = originalName;
    if (alias.length() == 1 && argdefs[originalName].shortName.isNull()) {
        argdefs[originalName].shortName = alias.at(0);
    }
}

/**
 * \brief Parse \a argv into a name,value map.
 * \param terminalArgs stop parsing when one of these options is found (it will be included in result)
 * \param safeArgs if not NULL, will be used to pass number of arguments before terminal argument (or argc if there was
 * no terminal argument)
 *
 * Supported options syntax: --switch; --param=value; --param value; -switch; -param=value; -param value.
 * Additionally on Windows: /switch; /param:value; /param value.
 *
 * When creating the map, alias names are converted to original option names.
 *
 * Use \a terminalArgs if you want need to stop parsing after certain options for security reasons, etc.
 */
QHash<QByteArray, QByteArray> SimpleCli::parse(int argc, char *argv[], const QList<QByteArray> &terminalArgs,
                                               int *safeArgc)
{
#ifdef Q_OS_WIN
    const bool winmode = true;
#else
    const bool winmode = false;
#endif

    QHash<QByteArray, QByteArray> map;
    int                           safe = 1;
    int                           n    = 1;
    for (; n < argc; ++n) {
        QByteArray str = QByteArray(argv[n]);
        QByteArray left, right;
        int        sep           = str.indexOf('=');
        bool       explicitValue = false;
        if (sep == -1) {
            left = str;
        } else {
            left          = str.mid(0, sep);
            right         = str.mid(sep + 1);
            explicitValue = true;
        }

        bool unnamedArgument = true;
        if (left.startsWith("--")) {
            left            = left.mid(2);
            unnamedArgument = false;
        } else if (left.startsWith('-') || (left.startsWith('/') && winmode)) {
            left            = left.mid(1);
            unnamedArgument = false;
        } else if (n == 1 && left.startsWith("xmpp:")) {
            unnamedArgument = false;
            left            = "uri";
            right           = str;
        }

        QByteArray name, value;
        if (unnamedArgument) {
            value = left;
        } else {
            name  = left;
            value = right;
            if (aliases.contains(name)) {
                name            = argdefs[aliases[name]].name;
                bool needsValue = argdefs[name].needsValue;
                if (!needsValue && explicitValue) {
                    continue; // ignore strange switch with value
                }
                if (needsValue && value.isNull() && n + 1 < argc) {
                    value = QByteArray(argv[++n]);
                }
            }
        }

        if (map.contains(name)) {
            qDebug("CLI: Ignoring next value ('%s') for '%s' arg.", value.constData(), name.constData());
        } else {
            map[name] = value;
        }

        if (terminalArgs.contains(name)) {
            break;
        } else {
            safe = n + 1;
        }
    }

    if (safeArgc) {
        *safeArgc = safe;
    }
    return map;
}

/**
 * \brief Produce options description, for use in --help.
 * \param textWidth wrap text when wider than \a textWidth
 */
QString SimpleCli::optionsHelp(int textWidth)
{
    QString ret;

    int margin = 2;

    int  longest    = -1;
    bool foundShort = false;

    for (const Arg &arg : std::as_const(argdefs)) {
        if (arg.needsValue) {
            longest = qMax(arg.name.length() + arg.valueHelp.length() + 1, longest);
        } else {
            longest = qMax(arg.name.length(), longest);
        }

        foundShort = foundShort || !arg.shortName.isNull();
    }
    longest += 2;                  // 2 = length("--")
    int helpPadding = longest + 6; // 6 = 2 (left margin) + 2 (space before help) + 2 (next line indent)
    if (foundShort) {
        helpPadding += 4; // 4 = length("-x, ")
    }

    for (const Arg &arg : std::as_const(argdefs)) {
        QString line;
        line.fill(' ', margin);
        if (foundShort) {
            if (arg.shortName.isNull()) {
                line += "    ";
            } else {
                line += '-' + arg.shortName + ", ";
            }
        }
        QString longarg = "--" + arg.name;
        if (arg.needsValue) {
            longarg += '=' + arg.valueHelp;
        }
        line += longarg;
        line += QString().fill(' ', longest - longarg.length() + 2); // 2 (space before help)
        line += arg.help;

        ret += wrap(line, textWidth, helpPadding, 0);
    }

    return ret;
}

/**
 * \brief Wrap text for printing on console.
 * \param text text to be wrapped
 * \param width width of the text
 * \param margin left margin (filled with spaces)
 * \param firstMargin margin in first line
 * Note: This function is designed for text that do not contain tabs or line breaks.
 * Results may be not pretty if such \a text is passed.
 */
QString SimpleCli::wrap(QString text, int width, int margin, int firstMargin)
{
    if (firstMargin < 0) {
        firstMargin = margin;
    }

    QString output;

    int prevBreak     = -1;
    int currentMargin = firstMargin;
    int nextBreak;

    do {
        nextBreak = prevBreak + width + 1 - currentMargin;
        if (nextBreak < text.length()) {
            int lastSpace = text.lastIndexOf(' ', nextBreak);
            if (lastSpace > prevBreak) {
                nextBreak = lastSpace;
            } else {
                // will be a bit longer...
                nextBreak = text.indexOf(' ', nextBreak);
                if (nextBreak == -1) {
                    nextBreak = text.length();
                }
            }
        } else {
            nextBreak = text.length();
        }

        output += QString().fill(' ', currentMargin);
        output += QStringView { text }.mid(prevBreak + 1, nextBreak - prevBreak - 1);
        output += '\n';

        prevBreak     = nextBreak;
        currentMargin = margin;
    } while (nextBreak < text.length());

    return output;
}
