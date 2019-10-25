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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "psitextview.h"

#include <QAbstractTextDocumentLayout>
#include <QMenu>
#include <QMimeData>
#include <QRegExp>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextDocumentFragment>
#include <QTextFragment>

#include "filesharingdownloader.h"
#include "filesharingmanager.h"
#include "psirichtext.h"
#include "qiteaudio.h"
#include "urlobject.h"

//----------------------------------------------------------------------------
// PsiTextView::Private
//----------------------------------------------------------------------------

//! \if _hide_doc_
class PsiTextView::Private : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    QString                  anchorOnMousePress;
    bool                     hadSelectionOnMousePress = false;
    FileSharingDeviceOpener *mediaOpener              = nullptr;
    ITEAudioController *     voiceMsgCtrl             = nullptr;

    // handler function accepts everything after tag name upto but exluding final ">"
    PsiRichText::ParsersMap objectParsers;

    QMap<QString, QString> parseHtmlAttrs(const QStringRef &html)
    {
        static QRegularExpression attrStart("([a-zA-Z0-9]+)=([\"'])(.*)\\2");
        auto                      it = attrStart.globalMatch(html);
        QMap<QString, QString>    attrs;
        while (it.hasNext()) {
            auto    match = it.next();
            QString name  = match.captured(1);
            QString value = match.captured(3);
            attrs.insert(name, value);
        }
        return attrs;
    }
};
//! endif

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
PsiTextView::PsiTextView(QWidget *parent) : QTextEdit(parent)
{
    d = new Private(this);

    setReadOnly(true);
    PsiRichText::install(document());

    viewport()->setMouseTracking(true); // we want to get all mouseMoveEvents

    auto itc        = new InteractiveText(this, QTextFormat::UserObject + 2); // +1 was allocated for MarkerFormatType
    d->voiceMsgCtrl = new ITEAudioController(itc);
    d->voiceMsgCtrl->setAutoFetchMetadata(true);

    d->objectParsers = PsiRichText::ParsersMap{
        { "share",
          [this](const QStringRef &html) -> QTextCharFormat {
              if (!d->mediaOpener)
                  return QTextCharFormat();
              auto    attrs = d->parseHtmlAttrs(html);
              QString id    = attrs.value(QLatin1String("id"));
              if (id.isEmpty())
                  return QTextCharFormat();

              auto item = d->mediaOpener->sharedItem(id);
              if (!item)
                  return QTextCharFormat();

              if (item->mimeType().startsWith(QLatin1String("audio/"))) {
                  return d->voiceMsgCtrl->makeFormat(QUrl(QLatin1String("share:") + id), d->mediaOpener);
              }

              if (item->mimeType().startsWith("image/")) {
                  QUrl url(QLatin1String("share:") + id);
                  if (item->isCached()) {
                      QImage img = item->preview(QSize(640, 480));
                      document()->addResource(QTextDocument::ImageResource, url, img);
                  } else {
                      connect(item, &FileSharingItem::downloadFinished, this, [this, url, item]() {
                          item->disconnect(this);
                          document()->addResource(QTextDocument::ImageResource, url, item->preview(QSize(640, 480)));
                          // document()->adjustSize();
                      });
                      auto downloader = item->download(false, 0, 0);
                      downloader->setSelfDelete(true);
                      // read just for cache
                      connect(downloader, &FileShareDownloader::readyRead, this,
                              [downloader]() { downloader->read(downloader->bytesAvailable()); });
                      downloader->open();
                  }
                  QTextImageFormat fmt;
                  fmt.setName(url.toString());
                  return fmt;
              }
              return QTextCharFormat();
          } }
    };
}

/**
 * Reimplemented createStandardContextMenu(const QPoint &position)
 * for creating of custom context menu
 */

QMenu *PsiTextView::createStandardContextMenu(const QPoint &position)
{
    QTextCursor textcursor = cursorForPosition(position);
    QMenu *     menu;
    QString     anc = anchorAt(position);
    if (!anc.isEmpty()) {
        menu = URLObject::getInstance()->createPopupMenu(anc);

        int     posInBlock = textcursor.position() - textcursor.block().position();
        QString textblock  = textcursor.block().text();
        int     begin      = textcursor.block().position() + textblock.lastIndexOf(QRegExp("\\s|^"), posInBlock) + 1;
        int     end        = textcursor.block().position() + textblock.indexOf(QRegExp("\\s|$"), posInBlock);
        textcursor.setPosition(begin);
        textcursor.setPosition(end, QTextCursor::KeepAnchor);
        setTextCursor(textcursor);

        menu = URLObject::getInstance()->createPopupMenu(anc);
    } else {
        if (isSelectedBlock() || !textCursor().hasSelection()) { // only if no selection we select text block
            int begin = textcursor.block().position();
            int end   = begin + textcursor.block().length() - 1;
            textcursor.setPosition(begin);
            textcursor.setPosition(end, QTextCursor::KeepAnchor);
            setTextCursor(textcursor);
        }
        menu = QTextEdit::createStandardContextMenu();
    }
    return menu;
}

bool PsiTextView::isSelectedBlock()
{
    if (textCursor().hasSelection()) {
        const QTextCursor &cursor = textCursor();
        const QTextBlock & block  = cursor.block();
        int                start  = cursor.selectionStart();
        if (block.position() == start && block.length() == cursor.selectionEnd() - start + 1)
            return true;
    }
    return false;
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
void PsiTextView::scrollToBottom() { verticalScrollBar()->setValue(verticalScrollBar()->maximum()); }

/**
 * Scrolls the vertical scroll bar to its minimum position i.e. to the top.
 */
void PsiTextView::scrollToTop() { verticalScrollBar()->setValue(verticalScrollBar()->minimum()); }

/**
 * This function is provided for convenience. Please see
 * PsiRichText::appendText() documentation for usage details.
 */
void PsiTextView::appendText(const QString &text)
{
    QTextCursor            cursor    = textCursor();
    PsiRichText::Selection selection = PsiRichText::saveSelection(this, cursor);

    PsiRichText::appendText(document(), cursor, text, true, d->objectParsers);

    PsiRichText::restoreSelection(this, cursor, selection);
    setTextCursor(cursor);
}

/**
 * This function is provided for convenience. Please see
 * PsiRichText::appendText() documentation for usage details.
 */
void PsiTextView::insertText(const QString &text, QTextCursor &cursor)
{
    QTextCursor            selCursor = textCursor();
    PsiRichText::Selection selection = PsiRichText::saveSelection(this, selCursor);

    PsiRichText::appendText(document(), cursor, text, false, d->objectParsers);

    PsiRichText::restoreSelection(this, selCursor, selection);
    setTextCursor(selCursor);
}

QString PsiTextView::getTextHelper(bool html) const
{
    PsiTextView *ptv      = const_cast<PsiTextView *>(this);
    QTextCursor  cursor   = ptv->textCursor();
    int          position = ptv->verticalScrollBar()->value();

    bool unselectAll = false;
    if (!textCursor().hasSelection()) {
        ptv->selectAll();
        unselectAll = true;
    }

    QMimeData *mime = createMimeDataFromSelection();
    QString    result;
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
QString PsiTextView::getHtml() const { return getTextHelper(true); }

QString PsiTextView::getPlainText() const { return getTextHelper(false); }

void PsiTextView::setMediaOpener(FileSharingDeviceOpener *opener) { d->mediaOpener = opener; }

void PsiTextView::contextMenuEvent(QContextMenuEvent *e)
{
    QMenu *menu = createStandardContextMenu(e->pos());
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
        QTextCursor cursor    = textCursor();
        QPoint      mapped    = QPoint(e->pos().x() + horizontalScrollBar()->value(),
                               e->pos().y() + verticalScrollBar()->value()); // from QTextEditPrivate::mapToContents
        const int   cursorPos = document()->documentLayout()->hitTest(mapped, Qt::FuzzyHit);
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

    if (!textCursor().hasSelection() || (anchor == d->anchorOnMousePress && d->hadSelectionOnMousePress))
        URLObject::getInstance()->popupAction(anchor);
}

/**
 * This is overridden in order to properly convert Icons back
 * to their original text.
 */
QMimeData *PsiTextView::createMimeDataFromSelection() const
{
    QTextDocument *doc = new QTextDocument();
    QTextCursor    cursor(doc);
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
    bool   atEnd   = verticalScrollBar()->value() == verticalScrollBar()->maximum();
    bool   atStart = verticalScrollBar()->value() == verticalScrollBar()->minimum();
    double value   = 0;
    if (!atEnd && !atStart)
        value = double(verticalScrollBar()->maximum()) / double(verticalScrollBar()->value());

    QTextEdit::resizeEvent(e);

    if (atEnd)
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    else if (value != 0)
        verticalScrollBar()->setValue(int(double(verticalScrollBar()->maximum()) / value));
}

#include "psitextview.moc"
