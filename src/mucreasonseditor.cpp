/*
 * mucreasonseditor.cpp
 * Copyright (C)
 * 2011 Evgeny Khryukin
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

#include "mucreasonseditor.h"
#include "common.h"
#include "psioptions.h"
#include "ui_mucreasonseditor.h"


MUCReasonsEditor::MUCReasonsEditor(QWidget* parent)
	: QDialog(parent)
	, ui_(new Ui::MUCReasonsEditor)
{
	ui_->setupUi(this);
	ui_->lstReasons->addItems(PsiOptions::instance()->getOption("options.muc.reasons").toStringList());

	connect(ui_->btnAdd, SIGNAL(clicked()), SLOT(addButtonClicked()));
	connect(ui_->btnRemove, SIGNAL(clicked()), SLOT(removeButtonClicked()));
	connect(ui_->lstReasons, SIGNAL(currentTextChanged(QString)), SLOT(currentChanged(QString)));
}

MUCReasonsEditor::~MUCReasonsEditor()
{
	delete ui_;
}

void MUCReasonsEditor::accept()
{
	save();
	reason_ = ui_->txtReason->text();
	QDialog::accept();
}

void MUCReasonsEditor::currentChanged(const QString &r)
{
	ui_->txtReason->setText(r);
}

void MUCReasonsEditor::addButtonClicked()
{
	reason_ = ui_->txtReason->text().trimmed();
	if (reason_.isEmpty())
		return;
	ui_->lstReasons->addItem(reason_);
}

void MUCReasonsEditor::removeButtonClicked()
{
	int idx = ui_->lstReasons->currentRow();
	if (idx >= 0) {
		QListWidgetItem *item =ui_->lstReasons->takeItem(idx);
		if (item)
			delete item;
	}
}

void MUCReasonsEditor::save()
{
	QStringList reasons;
	int cnt = ui_->lstReasons->count();
	for (int i = 0; i < cnt; ++i)
		reasons.append(ui_->lstReasons->item(i)->text());
	PsiOptions::instance()->setOption("options.muc.reasons", reasons);
}
