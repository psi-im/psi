/*
 * psitextview.cpp - PsiIcon-aware QTextView subclass widget
 * Copyright (C) 2003-2006  Michail Pishchagin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "psitextview.h"

#include <QMenu>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>
#include <QTextDocumentFragment>
#include <QTextFragment>
#include <QMimeData>

#include "urlobject.h"
#include "psirichtext.h"

//----------------------------------------------------------------------------
// PsiTextView::Private
//----------------------------------------------------------------------------

//! \if _hide_doc_
class PsiTextView::Private : public QObject
{
	Q_OBJECT

public:
	Private(QObject *parent)
	: QObject(parent)
	{
		anchorOnMousePress = QString();
		hadSelectionOnMousePress = false;
	}

	QString anchorOnMousePress;
	bool hadSelectionOnMousePress;

	QString fragmentToPlainText(const QTextFragment &fragment);
	QString blockToPlainText(const QTextBlock &block);
	QString documentFragmentToPlainText(const QTextDocument &doc, QTextFrame::Iterator frameIt);
};
//!endif

//----------------------------------------------------------------------------
// PsiTextView
//----------------------------------------------------------------------------

/**
 * \class PsiTextView
 * \brief PsiIcon-aware QTextView-subclass widget
 */

/**
 * Default constructor.
 */
PsiTextView::PsiTextView(QWidget *parent)
: QTextEdit(parent)
{
	d = new Private(this);

	setReadOnly(true);
	PsiRichText::install(document());

	viewport()->setMouseTracking(true); // we want to get all mouseMoveEvents
}

/**
 * This function returns true if vertical scroll bar is
 * at its maximum position.
 */
bool PsiTextView::atBottom()
{
	// '32' is 32 pixels margin, which was used in the old code
	return (verticalScrollBar()->maximum() - verticalScrollBar()->value()) <= 32;
}

/**
 * Scrolls the vertical scroll bar to its maximum position i.e. to the bottom.
 */
void PsiTextView::scrollToBottom()
{
	verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

/**
 * Scrolls the vertical scroll bar to its minimum position i.e. to the top.
 */
void PsiTextView::scrollToTop()
{
	verticalScrollBar()->setValue(verticalScrollBar()->minimum());
}

/**
 * This function is provided for convenience. Please see
 * PsiRichText::appendText() documentation for usage details.
 */
void PsiTextView::appendText(const QString &text)
{
	QTextCursor cursor = textCursor();
	PsiRichText::Selection selection = PsiRichText::saveSelection(this, cursor);

	PsiRichText::appendText(document(), cursor, text);

	PsiRichText::restoreSelection(this, cursor, selection);
	setTextCursor(cursor);
}

QString PsiTextView::getTextHelper(bool html) const
{
	PsiTextView *ptv = (PsiTextView *)this;
	QTextCursor cursor = ptv->textCursor();
	int position = ptv->verticalScrollBar()->value();

	bool unselectAll = false;
	if (!textCursor().hasSelection()) {
#if QT_VERSION == 0x040701
		// workaround for crash when deleting last character with backspace (qt-4.7.1)
		// http://bugreports.qt.nokia.com/browse/QTBUG-15857
		QTextCursor tempCursor = QTextCursor(ptv->document());
		tempCursor.movePosition(QTextCursor::Start);
		ptv->setTextCursor(tempCursor);
#endif
		ptv->selectAll();
		unselectAll = true;
	}

	QMimeData *mime = createMimeDataFromSelection();
	QString result;
	if (html)
		result = mime->html();
	else
		result = mime->text();
	delete mime;

	// we need to restore original position if selectAll()
	// was called, because setTextCursor() (which is necessary
	// to clear selection) will move vertical scroll bar
	if (unselectAll) {
		cursor.clearSelection();
		ptv->setTextCursor(cursor);
		ptv->verticalScrollBar()->setValue(position);
	}

	return result;
}

/**
 * Returns HTML markup for selected text. If no text is selected, returns
 * HTML markup for all text.
 */
QString PsiTextView::getHtml() const
{
	return getTextHelper(true);
}

QString PsiTextView::getPlainText() const
{
	return getTextHelper(false);
}

void PsiTextView::contextMenuEvent(QContextMenuEvent *e)
{
	QMenu *menu;
	if (!anchorAt(e->pos()).isEmpty())
		menu = URLObject::getInstance()->createPopupMenu(anchorAt(e->pos()));
	else
		menu = createStandardContextMenu();
	menu->exec(e->globalPos());
	e->accept();
	delete menu;
}

// Copied (with modifications) from QTextBrowser
void PsiTextView::mouseMoveEvent(QMouseEvent *e)
{
	QTextEdit::mouseMoveEvent(e);

	QString anchor = anchorAt(e->pos());
	viewport()->setCursor(anchor.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
}

// Copied (with modifications) from QTextBrowser
void PsiTextView::mousePressEvent(QMouseEvent *e)
{
	d->anchorOnMousePress = anchorAt(e->pos());
	if (!textCursor().hasSelection() && !d->anchorOnMousePress.isEmpty()) {
		QTextCursor cursor = textCursor();
		QPoint mapped = QPoint(e->pos().x() + horizontalScrollBar()->value(),
							   e->pos().y() + verticalScrollBar()->value()); // from QTextEditPrivate::mapToContents
		const int cursorPos = document()->documentLayout()->hitTest(mapped, Qt::FuzzyHit);
		if (cursorPos != -1)
			cursor.setPosition(cursorPos);
		setTextCursor(cursor);
	}

	QTextEdit::mousePressEvent(e);

	d->hadSelectionOnMousePress = textCursor().hasSelection();
}

// Copied (with modifications) from QTextBrowser
void PsiTextView::mouseReleaseEvent(QMouseEvent *e)
{
	QTextEdit::mouseReleaseEvent(e);

	if (!(e->button() & Qt::LeftButton))
		return;

	const QString anchor = anchorAt(e->pos());

	if (anchor.isEmpty())
		return;

	if (!textCursor().hasSelection()
		|| (anchor == d->anchorOnMousePress && d->hadSelectionOnMousePress))
		URLObject::getInstance()->popupAction(anchor);
}

/**
 * This is overridden in order to properly convert Icons back
 * to their original text.
 */
QMimeData *PsiTextView::createMimeDataFromSelection() const
{
	QTextDocument *doc = new QTextDocument();
	QTextCursor cursor(doc);
	cursor.insertFragment(textCursor().selection());
	QString text = PsiRichText::convertToPlainText(doc);
	delete doc;

	QMimeData *data = new QMimeData;
	data->setText(text);
	data->setHtml(Qt::convertFromPlainText(text));
	return data;
}

/**
 * Ensures that if PsiTextView was scrolled to bottom when resize
 * operation happened, it will still be scrolled to bottom after the fact.
 */
void PsiTextView::resizeEvent(QResizeEvent *e)
{
	bool atEnd = verticalScrollBar()->value() ==
				 verticalScrollBar()->maximum();
	bool atStart = verticalScrollBar()->value() ==
				   verticalScrollBar()->minimum();
	double value = 0;
	if (!atEnd && !atStart)
		value = (double)verticalScrollBar()->maximum() /
				(double)verticalScrollBar()->value();

	QTextEdit::resizeEvent(e);

	if (atEnd)
		verticalScrollBar()->setValue(verticalScrollBar()->maximum());
	else if (value != 0)
		verticalScrollBar()->setValue((int) ((double)verticalScrollBar()->maximum() / value));
}

#include "psitextview.moc"
