/*
 * xmlconsole.h - dialog for interacting manually with Jabber
 * Copyright (C) 2001, 2002  Justin Karneges, Remko Troncon
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

#ifndef XMLCONSOLE_H
#define XMLCONSOLE_H

#include <QWidget>
#include <QDialog>
#include <QPointer>

#include "ui_xmlconsole.h"

class QTextEdit;
class QCheckBox;
class PsiAccount;
class XmlPrompt;

class XmlConsole : public QWidget
{
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
	bool filtered(const QString&) const;

private:
	Ui::XMLConsole ui_;
	PsiAccount *pa;
	QPointer<XmlPrompt> prompt;
};

class XmlPrompt : public QDialog
{
	Q_OBJECT
public:
	XmlPrompt(QWidget *parent=0);
	~XmlPrompt();

signals:
	void textReady(const QString &);

private slots:
	void doTransmit();

private:
	QTextEdit *te;
};

#endif
