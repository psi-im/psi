/*
 * msgmle.h - subclass of PsiTextView to handle various hotkeys
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

#ifndef MSGMLE_H
#define MSGMLE_H

#include <QTextEdit>

#include "xmpp_htmlelement.h"

#define MAX_MESSAGE_HISTORY 50

class ChatEdit;
class QEvent;
class QKeyEvent;
class QResizeEvent;
class QTimer;
class SpellHighlighter;
class HTMLTextController;
class CapitalLettersController;


class ChatEdit : public QTextEdit
{
	void updateBackground();

	Q_OBJECT

public:
	ChatEdit(QWidget* parent);
	~ChatEdit();

	void setDialog(QWidget* dialog);

	// reimplemented
	QSize sizeHint() const;
	void setFont(const QFont &);

	static bool checkSpellingGloballyEnabled();
	void setCheckSpelling(bool);
	XMPP::HTMLElement toHTMLElement();
	bool isCorrection() { return correction; }
	void setLastMessageId(const QString& id) { lastId = id; }
	const QString& lastMessageId() { return lastId; }
	void resetCorrection() { correction = false; updateBackground(); };
	CapitalLettersController * capitalizer();

public slots:
	void appendMessageHistory(const QString& text);
	void clearMessageHistory();
	void doHTMLTextMenu();
	void setCssString(const QString& css);

protected slots:
 	void applySuggestion();
 	void addToDictionary();
	void optionsChanged();
	void showHistoryMessageNext();
	void showHistoryMessagePrev();
	void showHistoryMessageFirst();
	void showHistoryMessageLast();

protected:
	// override the tab/esc behavior
	bool focusNextPrevChild(bool next);
	void keyPressEvent(QKeyEvent *);
	bool event(QEvent * event);
	void contextMenuEvent(QContextMenuEvent *e);
	void showMessageHistory();
	void initActions();
	void setShortcuts();
	void setEditText(const QString& text);

private:
	QWidget	*dialog_;
	bool check_spelling_;
	SpellHighlighter* spellhighlighter_;
	QPoint last_click_;
	int previous_position_;
	QStringList typedMsgsHistory;
	long typedMsgsIndex;
	QAction* act_showMessagePrev;
	QAction* act_showMessageNext;
	QAction* act_showMessageFirst;
	QAction* act_showMessageLast;
	QAction *act_changeCase;
	QString currentText;
	HTMLTextController *controller_;
	CapitalLettersController *capitalizer_;
	bool correction;
	QString lastId;
	QPalette palOriginal;
	QPalette palCorrection;
};


class LineEdit : public ChatEdit
{
	Q_OBJECT
public:
	LineEdit(QWidget* parent);
	~LineEdit();

	// reimplemented
	QSize minimumSizeHint() const;
	QSize sizeHint() const;

protected:
	// reimplemented
	void resizeEvent(QResizeEvent*);

private slots:
	void recalculateSize();
	void updateScrollBar();
};

#endif
