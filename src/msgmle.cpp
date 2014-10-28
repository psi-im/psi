/*
 * msgmle.cpp - subclass of PsiTextView to handle various hotkeys
 * Copyright (C) 2001-2003  Justin Karneges, Michail Pishchagin
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

#include "msgmle.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QDesktopWidget>
#include <QEvent>
#include <QKeyEvent>
#include <QLayout>
#include <QMenu>
#include <QResizeEvent>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTimer>
#include <QMimeData>

#include "shortcutmanager.h"
#include "spellchecker/spellhighlighter.h"
#include "spellchecker/spellchecker.h"
#include "psioptions.h"

//----------------------------------------------------------------------------
// ChatEdit
//----------------------------------------------------------------------------
ChatEdit::ChatEdit(QWidget *parent)
	: QTextEdit(parent)
	, dialog_(0)
	, check_spelling_(false)
	, spellhighlighter_(0)
{
	setWordWrapMode(QTextOption::WordWrap);
	setAcceptRichText(false);

	setReadOnly(false);
	setUndoRedoEnabled(true);

	setMinimumHeight(48);

	previous_position_ = 0;
	setCheckSpelling(checkSpellingGloballyEnabled());
	connect(PsiOptions::instance(),SIGNAL(optionChanged(const QString&)),SLOT(optionsChanged()));
}

ChatEdit::~ChatEdit()
{
	delete spellhighlighter_;
}

void ChatEdit::setDialog(QWidget* dialog)
{
	dialog_ = dialog;
}

QSize ChatEdit::sizeHint() const
{
	return minimumSizeHint();
}

bool ChatEdit::checkSpellingGloballyEnabled()
{
	return (SpellChecker::instance()->available() && PsiOptions::instance()->getOption("options.ui.spell-check.enabled").toBool());
}

void ChatEdit::setCheckSpelling(bool b)
{
	check_spelling_ = b;
	if (check_spelling_) {
		if (!spellhighlighter_)
			spellhighlighter_ = new SpellHighlighter(document());
	}
	else {
		delete spellhighlighter_;
		spellhighlighter_ = 0;
	}
}

bool ChatEdit::focusNextPrevChild(bool next)
{
	return QWidget::focusNextPrevChild(next);
}

// Qt text controls are quite greedy to grab key events.
// disable that.
bool ChatEdit::event(QEvent * event) {
	if (event->type() == QEvent::ShortcutOverride) {
		return false;
	}
	return QTextEdit::event(event);
}

void ChatEdit::keyPressEvent(QKeyEvent *e)
{
/*	if(e->key() == Qt::Key_Escape || (e->key() == Qt::Key_W && e->modifiers() & Qt::ControlModifier))
		e->ignore();
	else if(e->key() == Qt::Key_Return &&
	       ((e->modifiers() & Qt::ControlModifier)
#ifndef Q_OS_MAC
	       || (e->modifiers() & Qt::AltModifier)
#endif
	       ))
		e->ignore();
	else if(e->key() == Qt::Key_M && (e->modifiers() & Qt::ControlModifier)) // newline
		insert("\n");
	else if(e->key() == Qt::Key_H && (e->modifiers() & Qt::ControlModifier)) // history
		e->ignore();
	else  if(e->key() == Qt::Key_S && (e->modifiers() & Qt::AltModifier))
		e->ignore();
	else*/ if(e->key() == Qt::Key_U && (e->modifiers() & Qt::ControlModifier))
		setText("");
/*	else if((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !((e->modifiers() & Qt::ShiftModifier) || (e->modifiers() & Qt::AltModifier)) && LEGOPTS.chatSoftReturn)
		e->ignore();
	else if((e->key() == Qt::Key_PageUp || e->key() == Qt::Key_PageDown) && (e->modifiers() & Qt::ShiftModifier))
		e->ignore();
	else if((e->key() == Qt::Key_PageUp || e->key() == Qt::Key_PageDown) && (e->modifiers() & Qt::ControlModifier))
		e->ignore(); */
#ifdef Q_OS_MAC
	else if (e->key() == Qt::Key_QuoteLeft && e->modifiers() == Qt::ControlModifier) {
		e->ignore();
	}
#endif
	else
	{
		QTextEdit::keyPressEvent(e);
	}
}

/**
 * Work around Qt bug, that QTextEdit doesn't accept() the
 * event, so it could result in another context menu popping
 * out after the first one.
 */
void ChatEdit::contextMenuEvent(QContextMenuEvent *e)
{
	last_click_ = e->pos();
	if (check_spelling_ && textCursor().selectedText().isEmpty() && SpellChecker::instance()->available()) {
		// Check if the word under the cursor is misspelled
		QTextCursor tc = cursorForPosition(last_click_);
		tc.movePosition(QTextCursor::StartOfWord, QTextCursor::MoveAnchor);
		tc.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
		QString selected_word = tc.selectedText();
		if (!selected_word.isEmpty() && !SpellChecker::instance()->isCorrect(selected_word)) {
			QList<QString> suggestions = SpellChecker::instance()->suggestions(selected_word);
			if (!suggestions.isEmpty() || SpellChecker::instance()->writable()) {
				QMenu spell_menu;
				if (!suggestions.isEmpty()) {
					foreach(QString suggestion, suggestions) {
						QAction* act_suggestion = spell_menu.addAction(suggestion);
						connect(act_suggestion,SIGNAL(triggered()),SLOT(applySuggestion()));
					}
					spell_menu.addSeparator();
				}
				if (SpellChecker::instance()->writable()) {
					QAction* act_add = spell_menu.addAction(tr("Add to dictionary"));
					connect(act_add,SIGNAL(triggered()),SLOT(addToDictionary()));
				}
				spell_menu.exec(QCursor::pos());
				e->accept();
				return;
			}
		}
	}

	// Do normal menu
	QTextEdit::contextMenuEvent(e);
	e->accept();
}

/*!
 * \brief handles a click on a suggestion
 * \param the action is just the container which holds the suggestion.
 *
 * This method is called by the framework whenever a user clicked on the child popupmenu
 * to select a suggestion for a missspelled word. It exchanges the missspelled word with the
 * suggestion which is the text of the QAction parameter.
 */
void ChatEdit::applySuggestion()
{
	QAction* act_suggestion = (QAction*) sender();
	int current_position = textCursor().position();

	// Replace the word
	QTextCursor	tc = cursorForPosition(last_click_);
	tc.movePosition(QTextCursor::StartOfWord, QTextCursor::MoveAnchor);
	tc.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
	int old_length = tc.position() - tc.anchor();
	tc.insertText(act_suggestion->text());
	tc.clearSelection();

	// Put the cursor where it belongs
	int new_length = act_suggestion->text().length();
	tc.setPosition(current_position - old_length + new_length);
	setTextCursor(tc);
}

/*!
 * \brief handles a click on the add2dict action of the parent popupmenu
 * \param Never used bool parameter
 *
 * The method sets the cursor to the last mouseclick position and looks for the word which is placed there.
 * This word is than added to the dictionary of aspell.
 */
void ChatEdit::addToDictionary()
{
	QTextCursor	tc = cursorForPosition(last_click_);
	int current_position = textCursor().position();

	// Get the selected word
	tc.movePosition(QTextCursor::StartOfWord, QTextCursor::MoveAnchor);
	tc.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
	SpellChecker::instance()->add(tc.selectedText());

	// Put the cursor where it belongs
	tc.clearSelection();
	tc.setPosition(current_position);
	setTextCursor(tc);
}

void ChatEdit::optionsChanged()
{
	setCheckSpelling(checkSpellingGloballyEnabled());
}

//----------------------------------------------------------------------------
// LineEdit
//----------------------------------------------------------------------------
LineEdit::LineEdit( QWidget *parent)
	: ChatEdit(parent)
{
	setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere); // no need for horizontal scrollbar with this
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	setMinimumHeight(0);

	connect(this, SIGNAL(textChanged()), SLOT(recalculateSize()));
}

LineEdit::~LineEdit()
{
}

QSize LineEdit::minimumSizeHint() const
{
	QSize sh = QTextEdit::minimumSizeHint();
	sh.setHeight(fontMetrics().height() + 1);
	sh += QSize(0, QFrame::lineWidth() * 2);
	return sh;
}

QSize LineEdit::sizeHint() const
{
	QSize sh = QTextEdit::sizeHint();
	sh.setHeight(int(document()->documentLayout()->documentSize().height()));
	sh += QSize(0, QFrame::lineWidth() * 2);
	((QTextEdit*)this)->setMaximumHeight(sh.height());
	return sh;
}

void LineEdit::resizeEvent(QResizeEvent* e)
{
	ChatEdit::resizeEvent(e);
	QTimer::singleShot(0, this, SLOT(updateScrollBar()));
}

void LineEdit::recalculateSize()
{
	updateGeometry();
	QTimer::singleShot(0, this, SLOT(updateScrollBar()));
}

void LineEdit::updateScrollBar()
{
	setVerticalScrollBarPolicy(sizeHint().height() > height() ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
	ensureCursorVisible();
}
