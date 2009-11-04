/*
 * showtextdlg.cpp - dialog for displaying a text file
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QLayout>
#include <QPushButton>
#include <QFile>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "showtextdlg.h"

// FIXME: combine to common init function
ShowTextDlg::ShowTextDlg(const QString &fname, bool rich, QWidget *parent)
	: QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);
	QString text;

	QFile f(fname);
	if(f.open(QIODevice::ReadOnly)) {
		QTextStream t(&f);
		while(!t.atEnd())
			text += t.readLine() + '\n';
		f.close();
	}

	QVBoxLayout *vb1 = new QVBoxLayout(this);
	vb1->setMargin(8);
	QTextEdit *te = new QTextEdit(this);
	te->setReadOnly(true);
	te->setAcceptRichText(rich);
	te->setText(text);

	vb1->addWidget(te);

	QHBoxLayout *hb1 = new QHBoxLayout;
	vb1->addLayout(hb1);
	hb1->addStretch(1);
	QPushButton *pb = new QPushButton(tr("&OK"), this);
	connect(pb, SIGNAL(clicked()), SLOT(accept()));
	hb1->addWidget(pb);
	hb1->addStretch(1);

	resize(560, 384);
}

ShowTextDlg::ShowTextDlg(const QString &text, bool nonfile, bool rich, QWidget *parent)
	: QDialog(parent)
{
	Q_UNUSED(nonfile);

	setAttribute(Qt::WA_DeleteOnClose);

	QVBoxLayout *vb1 = new QVBoxLayout(this);
	vb1->setMargin(8);
	QTextEdit *te = new QTextEdit(this);
	te->setReadOnly(true);
	te->setAcceptRichText(rich);
	te->setText(text);

	vb1->addWidget(te);

	QHBoxLayout *hb1 = new QHBoxLayout;
	vb1->addLayout(hb1);
	hb1->addStretch(1);
	QPushButton *pb = new QPushButton(tr("&OK"), this);
	connect(pb, SIGNAL(clicked()), SLOT(accept()));
	hb1->addWidget(pb);
	hb1->addStretch(1);

	resize(560, 384);
}
