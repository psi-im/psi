/*
 * chateditproxy.cpp - abstraction to change ChatEdit type in runtime
 * Copyright (C) 2007  Michail Pishchagin
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

#include "chateditproxy.h"

#include <QVBoxLayout>

#include "msgmle.h"
#include "psioptions.h"

ChatEditProxy::ChatEditProxy(QWidget* parent)
	: QWidget(parent)
	, lineEditEnabled_(false)
	, textEdit_(0)
	, layout_(0)
{
	layout_ = new QVBoxLayout(this);
	layout_->setMargin(0);
	layout_->setSpacing(0);

	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionsChanged()));
	optionsChanged();

	if (!textEdit_)
		updateLayout();
}

/**
 * If \a enable is true, then the LineEdit is used as internal
 * QTextEdit. Updates internal layout if necessary.
 */
void ChatEditProxy::setLineEditEnabled(bool enable)
{
	if (lineEditEnabled_ == enable)
		return;

	lineEditEnabled_ = enable;
	updateLayout();
}

/**
 * Creates new QTextEdit basing on ChatEditProxy's properties.
 */
ChatEdit* ChatEditProxy::createTextEdit()
{
	if (lineEditEnabled())
		return new LineEdit(this);
	return new ChatEdit(this);
}

/**
 * Moves the QTextDocument and QTextCursor data from \a oldTextEdit
 * to \a newTextEdit.
 *
 * NB: Make sure that all QSyntaxHighlighters are detached prior to calling
 * this function.
 */
void ChatEditProxy::moveData(QTextEdit* newTextEdit, QTextEdit* oldTextEdit) const
{
	QTextDocument* doc = oldTextEdit->document();
	QTextCursor cursor = oldTextEdit->textCursor();

	doc->setParent(newTextEdit);
	oldTextEdit->setDocument(0);

	newTextEdit->setDocument(doc);
	newTextEdit->setTextCursor(cursor);
}

/**
 * Creates new QTextEdit and moves data to it from the old one.
 * Text, selection and cursor position are left intact.
 */
void ChatEditProxy::updateLayout()
{
	ChatEdit* newEdit = createTextEdit();

	if (textEdit_) {
		// all syntaxhighlighters should be removed while we move
		// the documents around, and should be reattached afterwards
		textEdit_->setCheckSpelling(false);
		newEdit->setCheckSpelling(false);

		moveData(newEdit, textEdit_);

		newEdit->setCheckSpelling(ChatEdit::checkSpellingGloballyEnabled());
	}

	delete textEdit_;
	textEdit_ = newEdit;
	layout_->addWidget(textEdit_);
	emit textEditCreated(textEdit_);
}

/**
 * Update ChatEdit widget according to current options.
 * FIXME: When PsiOptions::instance()->getOption("options.ui.chat.use-expanding-line-edit").toBool() finally makes it to PsiOptions, make this slot
 *        private.
 */
void ChatEditProxy::optionsChanged()
{
	setLineEditEnabled(PsiOptions::instance()->getOption("options.ui.chat.use-expanding-line-edit").toBool());
}
