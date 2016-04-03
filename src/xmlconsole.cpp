/*
 * xmlconsole.cpp - dialog for interacting manually with XMPP
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <QLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QTextFrame>
#include <QScrollBar>

#include "xmpp_client.h"
#include "xmlconsole.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psicontactlist.h"
#include "textutil.h"

//----------------------------------------------------------------------------
// XmlConsole
//----------------------------------------------------------------------------
XmlConsole::XmlConsole(PsiAccount *_pa)
:QWidget()
{
	ui_.setupUi(this);

	pa = _pa;
	pa->dialogRegister(this);
	connect(pa, SIGNAL(updatedAccount()), SLOT(updateCaption()));
	connect(pa->client(), SIGNAL(xmlIncoming(const QString &)), SLOT(client_xmlIncoming(const QString &)));
	connect(pa->client(), SIGNAL(xmlOutgoing(const QString &)), SLOT(client_xmlOutgoing(const QString &)));
	connect(pa->psi(), SIGNAL(accountCountChanged()), this, SLOT(updateCaption()));
	updateCaption();

	prompt = 0;

	ui_.te->setUndoRedoEnabled(false);
	ui_.te->setReadOnly(true);
	ui_.te->setAcceptRichText(false);

	QTextFrameFormat f = ui_.te->document()->rootFrame()->frameFormat();
	f.setBackground(QBrush(Qt::black));
	ui_.te->document()->rootFrame()->setFrameFormat(f);

	connect(ui_.pb_clear, SIGNAL(clicked()), SLOT(clear()));
	connect(ui_.pb_input, SIGNAL(clicked()), SLOT(insertXml()));
	connect(ui_.pb_close, SIGNAL(clicked()), SLOT(close()));
	connect(ui_.pb_dumpRingbuf, SIGNAL(clicked()), SLOT(dumpRingbuf()));

	resize(560,400);
}

XmlConsole::~XmlConsole()
{
	pa->dialogUnregister(this);
}

void XmlConsole::clear()
{
	ui_.te->clear();
	QTextFrameFormat f = ui_.te->document()->rootFrame()->frameFormat();
	f.setBackground(QBrush(Qt::black));
	ui_.te->document()->rootFrame()->setFrameFormat(f);
}

void XmlConsole::updateCaption()
{
	if (pa->psi()->contactList()->enabledAccounts().count() > 1)
		setWindowTitle(pa->name() + ": " + tr("XML Console"));
	else
		setWindowTitle(tr("XML Console"));
}

void XmlConsole::enable()
{
	ui_.ck_enable->setChecked(true);
}

bool XmlConsole::filtered(const QString& str) const
{
	if(ui_.ck_enable->isChecked()) {
		// Only do parsing if needed
		if (!ui_.le_jid->text().isEmpty() || !ui_.ck_iq->isChecked() || !ui_.ck_message->isChecked() || !ui_.ck_presence->isChecked() || !ui_.ck_sm->isChecked()) {
			QDomDocument doc;
			if (!doc.setContent(str))
				return true;

			QDomElement e = doc.documentElement();
			QString tn = e.tagName();
			if ((tn == "iq" && !ui_.ck_iq->isChecked()) || (tn == "message" && !ui_.ck_message->isChecked()) || (tn == "presence" && !ui_.ck_presence->isChecked()) || ((tn == "a" || tn == "r") && !ui_.ck_sm->isChecked()))
				return true;

			if (!ui_.le_jid->text().isEmpty()) {
				Jid jid(ui_.le_jid->text());
				bool hasResource = !jid.resource().isEmpty();
				if (!jid.compare(e.attribute("to"),hasResource) && !jid.compare(e.attribute("from"),hasResource))
					return true;
			}
		}
		return false;
	}
	return true;
}

void XmlConsole::dumpRingbuf()
{
	QList<PsiAccount::xmlRingElem> buf = pa->dumpRingbuf();
	bool enablesave = ui_.ck_enable->isChecked();
	ui_.ck_enable->setChecked(true);
	QString stamp;
	foreach (PsiAccount::xmlRingElem el, buf) {
		stamp = "<!-- TS:" + el.time.toString(Qt::ISODate) + "-->";
		if (el.type == PsiAccount::RingXmlOut) {
			client_xmlOutgoing(stamp + el.xml);
		} else {
			client_xmlIncoming(stamp + el.xml);
		}
	}
	ui_.ck_enable->setChecked(enablesave);
}

void XmlConsole::addRecord(bool incoming, const QString &str) {
	if (!filtered(str)) {
		int prevSPos = ui_.te->verticalScrollBar()->value();
		bool atBottom = (prevSPos == ui_.te->verticalScrollBar()->maximum());
		QTextCursor prevCur = ui_.te->textCursor();

		ui_.te->moveCursor(QTextCursor::End);
		ui_.te->setTextColor(incoming? Qt::yellow : Qt::red);
		ui_.te->insertPlainText(str + '\n');

		if (!atBottom) {
			ui_.te->setTextCursor(prevCur);
			ui_.te->verticalScrollBar()->setValue(prevSPos);
		} else {
			ui_.te->verticalScrollBar()->setValue(ui_.te->verticalScrollBar()->maximum());
		}
	}
}

void XmlConsole::client_xmlIncoming(const QString &str)
{
	addRecord(true, str);
}

void XmlConsole::client_xmlOutgoing(const QString &str)
{
	addRecord(false, str);
}

void XmlConsole::insertXml()
{
	if(prompt)
		bringToFront(prompt);
	else {
		prompt = new XmlPrompt(this);
		connect(prompt, SIGNAL(textReady(const QString &)), SLOT(xml_textReady(const QString &)));
		prompt->show();
	}
}

void XmlConsole::xml_textReady(const QString &str)
{
	pa->client()->send(str);
}


//----------------------------------------------------------------------------
// XmlPrompt
//----------------------------------------------------------------------------
XmlPrompt::XmlPrompt(QWidget *parent)
	: QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle(tr("XML Input"));

	QVBoxLayout *vb1 = new QVBoxLayout(this);
	vb1->setMargin(8);

	te = new QTextEdit(this);
	te->setAcceptRichText(false);
	vb1->addWidget(te);

	QHBoxLayout *hb1 = new QHBoxLayout;
	vb1->addLayout(hb1);
	QPushButton *pb;

	pb = new QPushButton(tr("&Transmit"), this);
	pb->setDefault(true);
	connect(pb, SIGNAL(clicked()), SLOT(doTransmit()));
	hb1->addWidget(pb);
	hb1->addStretch(1);

	pb = new QPushButton(tr("&Close"), this);
	connect(pb, SIGNAL(clicked()), SLOT(close()));
	hb1->addWidget(pb);

	resize(320,240);
}

XmlPrompt::~XmlPrompt()
{
}

void XmlPrompt::doTransmit()
{
	QString str = te->toPlainText();

	// Validate input
	QDomDocument doc;
	if (!doc.setContent(str)) {
		int i = QMessageBox::warning(this, tr("Malformed XML"), tr("You have entered malformed XML input. Are you sure you want to send this ?"), tr("Yes"), tr("No"));
		if (i != 0)
			return;
	}

	textReady(str);
	close();
}

