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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "showtextdlg.h"
#include "fileutil.h"

#include <QTextStream>

// FIXME: combine to common init function
ShowTextDlg::ShowTextDlg(const QString &fname, bool rich, QWidget *parent) : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    ui_.setupUi(this);

    QString text = FileUtil::readFileText(fname);
    ui_.textEdit->setAcceptRichText(rich);
    ui_.textEdit->setText(text);
    if (rich) {
        ui_.textEdit->setTextInteractionFlags(Qt::TextBrowserInteraction);
    }

    connect(ui_.okButton, SIGNAL(clicked()), SLOT(accept()));
    resize(560, 384);
}

ShowTextDlg::ShowTextDlg(const QString &text, bool nonfile, bool rich, QWidget *parent) : QDialog(parent)
{
    Q_UNUSED(nonfile);

    setAttribute(Qt::WA_DeleteOnClose);
    ui_.setupUi(this);

    ui_.textEdit->setAcceptRichText(rich);
    ui_.textEdit->setText(text);
    if (rich) {
        ui_.textEdit->setTextInteractionFlags(Qt::TextBrowserInteraction);
    }

    connect(ui_.okButton, SIGNAL(clicked()), SLOT(accept()));
    resize(560, 384);
}
