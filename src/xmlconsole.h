/*
 * xmlconsole.h - dialog for interacting manually with XMPP
 * Copyright (C) 2001-2002  Justin Karneges, Remko Troncon
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

#ifndef XMLCONSOLE_H
#define XMLCONSOLE_H

#include "ui_xmlconsole.h"

#include <QDialog>
#include <QPointer>
#include <QWidget>

class PsiAccount;
class QCheckBox;
class QTextEdit;
class XmlPrompt;

class XmlConsole : public QWidget {
    Q_OBJECT
public:
    XmlConsole(PsiAccount *);
    ~XmlConsole();
    void enable();

private slots:
    void clear();
    void updateCaption();
    void insertXml();
    void dumpRingbuf();
    void client_xmlIncoming(const QString &);
    void client_xmlOutgoing(const QString &);
    void xml_textReady(const QString &);

protected:
    bool filtered(const QString &) const;
    void addRecord(bool incoming, const QString &str);

private:
    Ui::XMLConsole      ui_;
    PsiAccount *        pa;
    QPointer<XmlPrompt> prompt;
};

class XmlPrompt : public QDialog {
    Q_OBJECT
public:
    XmlPrompt(QWidget *parent = nullptr);
    ~XmlPrompt();

signals:
    void textReady(const QString &);

private slots:
    void doTransmit();

private:
    QTextEdit *te;
};

#endif // XMLCONSOLE_H
