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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QApplication>
#include <QLayout>
#include <QTimer>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QDesktopWidget>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QScrollBar>
#include <QTextCharFormat>

#include "common.h"
#include "msgmle.h"
#include "shortcutmanager.h"
#include "spellchecker.h"
#include "psioptions.h"

//----------------------------------------------------------------------------
// ChatView
//----------------------------------------------------------------------------
ChatView::ChatView(QWidget *parent, QWidget* dialog) : PsiTextView(parent)
{
	dialog_ = dialog;
	setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

	setReadOnly(true);
	setUndoRedoEnabled(false);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

#ifndef Q_WS_X11	// linux has this feature built-in
	connect(this, SIGNAL(copyAvailable(bool)), SLOT(autoCopy(bool)));
#endif
}

ChatView::~ChatView()
{
}

bool ChatView::focusNextPrevChild(bool next)
{
	return QWidget::focusNextPrevChild(next);
}

void ChatView::keyPressEvent(QKeyEvent *e)
{
	if(dialog_) {
		// Ignore registered key sequences (and pass them up)
		QKeySequence k(e->key() + (e->modifiers() & ~Qt::KeypadModifier));
		foreach(QAction* act, dialog_->actions()) {
			foreach(QKeySequence keyseq, act->shortcuts()) {
				if(!keyseq.isEmpty() && keyseq.matches(k) == QKeySequence::ExactMatch) {
					e->ignore();
					//act->trigger();
					return;
				}
			}
		}
	}

/*	if(e->key() == Qt::Key_Escape)
		e->ignore(); 
#ifdef Q_WS_MAC
	else if(e->key() == Qt::Key_W && e->modifiers() & Qt::ControlModifier)
		e->ignore();
	else
#endif
	else if(e->key() == Qt::Key_Return && ((e->modifiers() & Qt::ControlModifier) || (e->modifiers() & Qt::AltModifier)) )
		e->ignore();
	else if(e->key() == Qt::Key_H && (e->modifiers() & Qt::ControlModifier))
		e->ignore();
	else if(e->key() == Qt::Key_I && (e->modifiers() & Qt::ControlModifier))
		e->ignore(); */
	/*else*/ if(e->key() == Qt::Key_M && (e->modifiers() & Qt::ControlModifier) && !isReadOnly()) // newline 
		insert("\n");
/*	else if(e->key() == Qt::Key_U && (e->modifiers() & Qt::ControlModifier) && !isReadOnly())
		setText(""); */
	else
		PsiTextView::keyPressEvent(e);
}

void ChatView::resizeEvent(QResizeEvent *e)
{
	// This fixes flyspray #45
	QScrollBar *vsb = verticalScrollBar();
	if ((vsb->maximum() - vsb->value()) <= vsb->pageStep())
		scrollToBottom();

	PsiTextView::resizeEvent(e);
}

/**
	Copies any selected text (from selection 0) to the clipboard
	if option.autoCopy is TRUE, \a copyAvailable is TRUE
	and ChatView is in read-only mode.
	In any other case it does nothing.

	This slot is connected with copyAvailable(bool) signal
	in ChatView's constructor.

	\sa copyAvailable()
*/

void ChatView::autoCopy(bool copyAvailable)
{
	if ( isReadOnly() && copyAvailable && option.autoCopy ) {
		copy();
	}
}

/**
 * Handle KeyPress events that happen in ChatEdit widget. This is used
 * to 'fix' the copy shortcut.
 * \param object object that should receive the event
 * \param event received event
 * \param chatEdit pointer to the dialog's ChatEdit widget that receives user input
 */
bool ChatView::handleCopyEvent(QObject *object, QEvent *event, ChatEdit *chatEdit)
{
	if (object == chatEdit && event->type() == QEvent::KeyPress) {
		QKeyEvent *e = (QKeyEvent *)event;
		if ((e->key() == Qt::Key_C && (e->modifiers() & Qt::ControlModifier)) ||
		    (e->key() == Qt::Key_Insert && (e->modifiers() & Qt::ControlModifier)))
		{
			if (!chatEdit->textCursor().hasSelection() &&
			     this->textCursor().hasSelection()) 
			{
				this->copy();
				return true;
			}
		}
	}
	
	return false;
}

void ChatView::appendText(const QString &text)
{
	bool doScrollToBottom = atBottom();
	
	// prevent scrolling back to selected text when 
	// restoring selection
	int scrollbarValue = verticalScrollBar()->value();
	
	PsiTextView::appendText(text);
	
	if (doScrollToBottom)
		scrollToBottom();
	else
		verticalScrollBar()->setValue(scrollbarValue);
}

/**
 * \brief Common function for ChatDlg and GCMainDlg. FIXME: Extract common
 * chat window from both dialogs and move this function to that class.
 */
QString ChatView::formatTimeStamp(const QDateTime &time)
{
	// TODO: provide an option for user to customize
	// time stamp format
	return QString().sprintf("%02d:%02d:%02d", time.time().hour(), time.time().minute(), time.time().second());;
}

//----------------------------------------------------------------------------
// ChatEdit
//----------------------------------------------------------------------------
ChatEdit::ChatEdit(QWidget *parent, QWidget *dlg /* = NULL*/) 
: QTextEdit(parent)
{
	dialog_ = dlg;
	setWordWrapMode(QTextOption::WordWrap);
	setAcceptRichText(false);

	setReadOnly(false);
	setUndoRedoEnabled(true);

	setMinimumHeight(48);

	previous_position_ = 0;
	check_spelling_ = PsiOptions::instance()->getOption("options.ui.spell-check.enabled").toBool();
	connect(PsiOptions::instance(),SIGNAL(optionChanged(const QString&)),SLOT(optionsChanged()));
	connect(this,SIGNAL(cursorPositionChanged()),SLOT(cursorChanged()));
	connect(document(),SIGNAL(contentsChange(int,int,int)), SLOT(contentsChanged(int,int,int)));
}

ChatEdit::~ChatEdit()
{
}

bool ChatEdit::focusNextPrevChild(bool next)
{
	return QWidget::focusNextPrevChild(next);
}

void ChatEdit::keyPressEvent(QKeyEvent *e)
{
	if(dialog_) {
		// Ignore registered key sequences (and pass them up)
		QKeySequence k(e->key() + (e->modifiers() & ~Qt::KeypadModifier));
		foreach(QAction* act, dialog_->actions()) {
			foreach(QKeySequence keyseq, act->shortcuts()) {
				if(!keyseq.isEmpty() && keyseq.matches(k) == QKeySequence::ExactMatch) {
					e->ignore();
					//act->trigger();
					return;
				}
			}
		}
	}
/*	if(e->key() == Qt::Key_Escape || (e->key() == Qt::Key_W && e->modifiers() & Qt::ControlModifier))
		e->ignore();
	else if(e->key() == Qt::Key_Return && 
	       ((e->modifiers() & Qt::ControlModifier) 
#ifndef Q_WS_MAC
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
/*	else if((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !((e->modifiers() & Qt::ShiftModifier) || (e->modifiers() & Qt::AltModifier)) && option.chatSoftReturn)
		e->ignore();
	else if((e->key() == Qt::Key_PageUp || e->key() == Qt::Key_PageDown) && (e->modifiers() & Qt::ShiftModifier))
		e->ignore();
	else if((e->key() == Qt::Key_PageUp || e->key() == Qt::Key_PageDown) && (e->modifiers() & Qt::ControlModifier))
		e->ignore(); */
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
			QMenu spell_menu;
			QList<QString> suggestions = SpellChecker::instance()->suggestions(selected_word);
			if (!suggestions.isEmpty()) {
				foreach(QString suggestion, suggestions) {
					QAction* act_suggestion = spell_menu.addAction(suggestion);
					connect(act_suggestion,SIGNAL(triggered()),SLOT(applySuggestion()));
				}
				spell_menu.addSeparator();
			}
			QAction* act_add = spell_menu.addAction(tr("Add to dictionary"));
			connect(act_add,SIGNAL(triggered()),SLOT(addToDictionary()));
			spell_menu.exec(QCursor::pos());
			e->accept();
			return;
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
	markMisspelled(tc, false);
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
	if (SpellChecker::instance()->add(tc.selectedText())) {
		markMisspelled(tc, false);
	}
	
	// Put the cursor where it belongs
	tc.clearSelection();
	tc.setPosition(current_position);
	setTextCursor(tc);
}

void ChatEdit::cursorChanged()
{
	if (!check_spelling_ || text().isEmpty()) 
		return;

	QTextCursor	tc = textCursor();
	tc.movePosition(QTextCursor::StartOfWord, QTextCursor::MoveAnchor);
	int current_position = tc.position();
	if (current_position != previous_position_) {
		// Unmark the current word
		tc.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
		markMisspelled(tc, false);
		
		// Spell check the previous word
		tc.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
		if (tc.position() > previous_position_) {
			tc.setPosition(previous_position_);
			tc.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
			QString word = tc.selectedText();
			if (!word.isEmpty()) {
				markMisspelled(tc, !SpellChecker::instance()->isCorrect(word));
			}
		}

		previous_position_ = current_position;
	}
}

void ChatEdit::contentsChanged(int position, int, int)
{
	if (!check_spelling_ || text().isEmpty()) 
		return;
	
	QTextCursor tc = textCursor();
	tc.movePosition(QTextCursor::StartOfWord, QTextCursor::MoveAnchor);
	int end_position = tc.position();
	tc.movePosition(QTextCursor::EndOfWord, QTextCursor::MoveAnchor);
	if (tc.position() <= position) {
		end_position = position;
	}

	tc.setPosition(position);
	tc.movePosition(QTextCursor::StartOfWord, QTextCursor::MoveAnchor);
	while (tc.position() < end_position) {
		tc.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
		QString word = tc.selectedText();
		if (!word.isEmpty()) {
			markMisspelled(tc, !SpellChecker::instance()->isCorrect(word));
		}
		tc.movePosition(QTextCursor::NextWord, QTextCursor::MoveAnchor);
	}
}

void ChatEdit::optionsChanged()
{
	bool check_spelling = PsiOptions::instance()->getOption("options.ui.spell-check.enabled").toBool();
	if (check_spelling_ != check_spelling) {
		check_spelling_ = check_spelling;
		QTextCursor tc = textCursor();
		tc.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
		if (!check_spelling_) {
			// Clear the whole document
			tc.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
			markMisspelled(tc, false);
		}
		else {
			// Spell check the whole document
			while(!tc.atEnd()) {
				tc.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
				QString word = tc.selectedText();
				if (!word.isEmpty()) {
					markMisspelled(tc, !SpellChecker::instance()->isCorrect(word));
				}
				tc.movePosition(QTextCursor::NextWord, QTextCursor::MoveAnchor);
			}
		}
	}
}

/*!
 * \brief Marks the selected word as missspelled or unmarks it.
 * \param tc is the TextCursor which holds the selection of the to be marked word.
 * \param bMissSpelled if true the selected word is marked as missspelled (default), if false marks it standard black with no underline
 * 
 * This method is used for marking or unmarking a missspelled or corrected word.
 * Depending on the second Parameter (bMissSpelled) the word is marked as missspelled or not.
 * The first parameter holds the TextCursor which itself holds the selection of the to be marked word.
 */
void ChatEdit::markMisspelled(QTextCursor& tc, bool misspelled /* = true */)
{
	QTextCharFormat	tcf;
	if(misspelled) {
		tcf.setUnderlineColor(QBrush(QColor(255,0,0)));
		tcf.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
	}
	else {
		tcf.setUnderlineColor(QBrush(QColor(0,0,0)));
		tcf.setUnderlineStyle(QTextCharFormat::NoUnderline);
	}
	tc.mergeCharFormat(tcf);
}


//----------------------------------------------------------------------------
// LineEdit
//----------------------------------------------------------------------------
LineEdit::LineEdit( QWidget *parent, QWidget *dlg /*= NULL*/) 
: ChatEdit( parent, dlg)
{
	lastSize = QSize( 0, 0 );
	initialWindowGeometry = QRect( 0, 0, 0, 0 );

	QWidget *topParent = topLevelWidget();
	topParent->installEventFilter( this );
	moveTo = QPoint(topParent->x(), topParent->y());

	moveTimer = new QTimer( this );
	connect( moveTimer, SIGNAL(timeout()), SLOT(checkMoved()) );

	// LineEdit's size hint is to be vertically as small as possible
	setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Maximum );

	setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere); // no need for horizontal scrollbar with this
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	setMinimumHeight(-1);

	connect( this, SIGNAL( textChanged() ), SLOT( recalculateSize() ) );

	// ensure that initial document size calculations are correct
	document()->setPageSize(QSize(100, INT_MAX));
}

LineEdit::~LineEdit()
{
}

/**
 * Returns true if the dialog could be automatically resized by LineEdit.
 */
bool LineEdit::allowResize() const
{
	QWidget *topParent = topLevelWidget();

	QRect desktop = qApp->desktop()->availableGeometry( (QWidget *)topParent );
	float desktopArea = desktop.width() * desktop.height();
	float dialogArea  = topParent->frameGeometry().width() * topParent->frameGeometry().height();

	// maximized and large chat windows shoulnd't resize the dialog
	if ( (dialogArea / desktopArea) > 0.9 )
		return false;

	return true;
}

/**
 * In this implementation, it is quivalent to sizeHint().
 */
QSize LineEdit::minimumSizeHint() const
{
	return sizeHint();
}

/**
 * All magic is contained within this function. It determines the possible maximum
 * height, and controls the appearance of vertical scrollbar.
 */
QSize LineEdit::sizeHint() const
{
	if ( lastSize.width() != 0 && lastSize.height() != 0 )
		return lastSize;

	lastSize.setWidth( QTextEdit::sizeHint().width() );
	// TODO: figure out what to do, if layouting is incomplete.
	qreal h = 4.0 + document()->documentLayout()->documentSize().height();
	QWidget *topParent = topLevelWidget();
	QRect desktop = qApp->desktop()->availableGeometry( (QWidget *)topParent );
	qreal dh = h - height();

	Qt::ScrollBarPolicy showScrollBar = Qt::ScrollBarAlwaysOff;

	// check that our dialog's height doesn't exceed the desktop's
	if ( allowResize() && (topParent->frameGeometry().height() + dh) >= desktop.height() ) {
		// handles the case when the dialog could be resized,
		// but lineedit wants to occupy too much space, so we should limit it
		h = desktop.height() - ( topParent->frameGeometry().height() - height() );
		showScrollBar = Qt::ScrollBarAlwaysOn;
	}
	else if ( !allowResize() && (h > topParent->geometry().height()/2) ) {
		// handles the case when the dialog could not be resized(i.e. it's maximized).
		// in this case we limit maximum height of lineedit to the half of dialog's
		// full height
		h = topParent->geometry().height() / 2;
		showScrollBar = Qt::ScrollBarAlwaysOn;
	}

	// enable vertical scrollbar only when we're surely in need for it
	((QTextEdit *)this)->setVerticalScrollBarPolicy(showScrollBar);

	lastSize.setHeight( h );
	return lastSize;
}

/**
 * Handles automatic dialog resize.
 */
void LineEdit::recalculateSize()
{
	if ( !isUpdatesEnabled() )
		return;

	QSize oldSize = lastSize;
	lastSize = QSize( 0, 0 ); // force sizeHint() to update
	QSize newSize = sizeHint();

	if ( QABS(newSize.height() - oldSize.height()) > 1 ) {
		QWidget *topParent = topLevelWidget();
		
		if ( allowResize() ) {
			topParent->layout()->setEnabled( false ); // try to reduce some flicker

			int dh = newSize.height() - oldSize.height();

			// if we're going to shrink dialog considerably, minimum
			// size will prevent us from doing it. Activating main
			// layout after resize will reset minimum sizes to sensible values
			topParent->setMinimumSize( 10, 10 );

			topParent->resize( topParent->width(),
					   topParent->height() + dh );

			bool canMove = dh > 0;
			int  newy    = topParent->y();

			// try to move window to its old position
			if ( movedWindow() && dh < 0 ) {
				newy = initialWindowGeometry.y();
				canMove = true;
			}

			// check, if we need to move dialog upper
			QRect desktop = qApp->desktop()->availableGeometry( (QWidget *)topParent );
			if ( canMove && ( newy + topParent->frameGeometry().height() >= desktop.bottom() ) ) {
				// initialize default window position
				if ( !movedWindow() ) {
					initialWindowGeometry = topParent->frameGeometry();
				}

				newy = QMAX(0, desktop.bottom() - topParent->frameGeometry().height());
			}

			if ( canMove && newy != topParent->y() ) {
				topParent->move( topParent->x(), newy );
				moveTo = topParent->pos();
			}

			topParent->layout()->setEnabled( true );
		}

		// issue a layout update
		parentWidget()->layout()->update();
	}
}

void LineEdit::resizeEvent( QResizeEvent *e )
{
	// issue a re-layout, just in case
	lastSize = QSize( 0, 0 ); // force sizeHint() to update
	sizeHint(); // update the size hint, and cache the value
	topLevelWidget()->layout()->activate();

	QTextEdit::resizeEvent( e );
}

void LineEdit::setUpdatesEnabled( bool enable )
{
	bool ue = isUpdatesEnabled();
	ChatEdit::setUpdatesEnabled( enable );

	if ( !ue && enable )
		recalculateSize();
}

bool LineEdit::eventFilter(QObject *watched, QEvent *e)
{
	if ( e->type() == QEvent::Reparent ) {
		// In case of tabbed chats, dialog could be reparented to a higher-level dialog
		// we need to get move events from it too. And unnecessary event filters
		// are automatically cleaned up by Qt.
		topLevelWidget()->installEventFilter( this );
	}
	else if ( e->type() == QEvent::Move ) {
		QWidget *topParent = topLevelWidget();
		if ( watched == topParent ) {
			moveTimer->start( 100, true );
		}
	}

	return ChatEdit::eventFilter( watched, e );
}

/**
 * This function serves as a workaround for multiple move events, some of which
 * have incorrect coordinates (at least on KDE)
 */
void LineEdit::checkMoved()
{
	QWidget *topParent = topLevelWidget();
	if ( QABS(moveTo.x() - topParent->x()) > 1 ||
	     QABS(moveTo.y() - topParent->y()) > 1 ) {
		moveTo = topParent->pos();
		initialWindowGeometry = QRect( 0, 0, 0, 0 );
	}
}

bool LineEdit::movedWindow() const
{
	return initialWindowGeometry.left()  ||
	       initialWindowGeometry.top()   ||
	       initialWindowGeometry.width() ||
	       initialWindowGeometry.height();
}

