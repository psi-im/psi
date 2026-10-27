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
#include <QUrlQuery>

#include <QAbstractTextDocumentLayout>
#include <QMenu>
#include <QMimeData>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextDocumentFragment>
#include <QTextFragment>
#include <QTextBlock>
#include <QPainter>
#include <QVector>
#include <QTimer>

#include "filesharingdownloader.h"
#include "filesharingmanager.h"
#include "psirichtext.h"
#include "qiteaudio.h"
#include "qitevideo.h"
#include "textutil.h"
#include "urlobject.h"

// Quoted lines ("> text", XEP-0393 block quotes) become blocks with this
// property, their nesting level, indented and drawn with bars as in a quote.
constexpr int QuoteLevelProperty = QTextFormat::UserProperty + 0x51;
constexpr int QuoteBarWidth      = 3;
constexpr int QuoteIndent        = 10; // A bar and the gap after it, as in MessagePreview.

// The nesting level of a line quoted with '>' (also "> >" and ">>"), and the
// length of its markers including one space after them.
int quoteMarkers(const QString &line, int *length = nullptr)
{
    int at = 0;
    while (at < line.size() && line.at(at) == QLatin1Char(' '))
        ++at;
    int level = 0;
    while (at < line.size() && line.at(at) == QLatin1Char('>')) {
        ++level;
        ++at;
        if (at + 1 < line.size() && line.at(at) == QLatin1Char(' ') && line.at(at + 1) == QLatin1Char('>'))
            ++at;
    }
    if (level && at < line.size() && line.at(at) == QLatin1Char(' '))
        ++at;
    if (length)
        *length = at;
    return level;
}

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
    bool                     keepAtBottom             = true;
    FileSharingDeviceOpener *mediaOpener              = nullptr;
    ITEAudioController      *voiceMsgCtrl             = nullptr;
    ITEVideoController      *videoCtrl                = nullptr;

    // handler function accepts everything after tag name upto but exluding final ">"
    PsiRichText::ParsersMap objectParsers;

    QMap<QString, QString> parseHtmlAttrs(const QStringView &html)
    {
        static QRegularExpression attrStart("([a-zA-Z0-9]+)=([\"'])((.(?!\\2))*.)\\2");
        // static QRegularExpression attrStart("([a-zA-Z0-9]+)=([\"'])([^\"']*)\\2");
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
        auto it = attrStart.globalMatch(html);
#else
        auto it = attrStart.globalMatchView(html);
#endif
        QMap<QString, QString> attrs;
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
    /* TODO For some reason this doesn't work. please reveiw and fix

    document()->addResource(QTextDocument::StyleSheetResource, QUrl(QLatin1String("psi.css")), R"style(
span.emojis {
    font-family: 'Apple Color Emoji', 'Noto Color Emoji', 'Segoe UI Emoji';
    font-size: 20px;
}
)style");
    */

    viewport()->setMouseTracking(true); // we want to get all mouseMoveEvents

    auto itc        = new InteractiveText(this, QTextFormat::UserObject + 2); // +1 was allocated for MarkerFormatType
    d->voiceMsgCtrl = new ITEAudioController(itc, viewport());
    d->voiceMsgCtrl->setAutoFetchMetadata(true);
    d->videoCtrl   = new ITEVideoController(itc, viewport());
    auto saveMedia = [this](const QUrl &source) {
        auto opener = d->mediaOpener
            ? d->mediaOpener->forAccount(QUrlQuery(source).queryItemValue(QStringLiteral("account")))
            : nullptr;
        if (opener)
            opener->saveAs(source, this);
    };
    connect(d->voiceMsgCtrl, &ITEAudioController::downloadRequested, this, saveMedia);
    connect(d->videoCtrl, &ITEVideoController::downloadRequested, this, saveMedia);

    d->objectParsers = PsiRichText::ParsersMap {
        { "share",
          [this](const QStringView &html, int insertAfter) -> PsiRichText::ParserRet {
              if (!d->mediaOpener)
                  return { QTextCharFormat(), "" };
              auto    attrs = d->parseHtmlAttrs(html);
              QString id    = attrs.value(QLatin1String("id"));
              if (id.isEmpty())
                  return { QTextCharFormat(), "" };

              const auto account = attrs.value(QLatin1String("account"));
              auto       opener  = d->mediaOpener->forAccount(account);
              if (!opener)
                  return { QTextCharFormat(), "" };
              QUrl source(QLatin1String("share:") + id);
              if (!account.isEmpty()) {
                  QUrlQuery query;
                  query.addQueryItem(QStringLiteral("account"), account);
                  source.setQuery(query);
              }
              auto item = opener->sharedItem(id);
              if (!item)
                  return { QTextCharFormat(), "" };
              // Downloaded by itself only as far as the message allows,
              // see FileSharingManager::autoDownloadLimit().
              const quint64 autoLimit = attrs.value(QLatin1String("auto")).toULongLong();

              if (item->mimeType().startsWith(QLatin1String("audio/"))) {
                  item->autoDownload(autoLimit);
                  return { d->voiceMsgCtrl->makeFormat(source, opener, true), "" };
              }

              if (item->mimeType().startsWith(QLatin1String("video/"))) {
                  item->autoDownload(autoLimit);
                  return { d->videoCtrl->makeFormat(source, opener, QSize(320, 220), true), "" };
              }

              if (item->mimeType().startsWith("image/")) {
                  QString anchorName = QLatin1String("share:") + id;
                  QUrl    url(anchorName);

                  if (item->isCached()) {
                      QVariant         vimg = document()->resource(QTextDocument::ImageResource, url);
                      QTextImageFormat fmt;
                      fmt.setName(url.toString());
                      QImage img;
                      if (vimg.isValid()) {
                          img = vimg.value<QImage>();
                      } else {
                          img = item->preview(QSize(640, 480));
                          document()->addResource(QTextDocument::ImageResource, url, img);
                      }

                      fmt.setWidth(img.width());
                      fmt.setHeight(img.height());

                      return { fmt, "" };
                  }

                  // otherwise we have to download it
                  connect(item, &FileSharingItem::downloadFinished, this, [this, anchorName, url, item, insertAfter]() {
                      if (!item->isCached())
                          return; // failed; the link stays until a download succeeds
                      item->disconnect(this);
                      document()->addResource(QTextDocument::ImageResource, url, item->preview(QSize(640, 480)));
                      auto        prevCur = textCursor();
                      QTextCursor cur(document());
                      cur.setPosition(insertAfter);

                      cur = PsiRichText::findMarker(cur, anchorName + "/start");
                      if (cur.isNull()) {
                          cur = PsiRichText::findMarker(QTextCursor(document()), anchorName + "/start");
                          if (cur.isNull())
                              return;
                      }
                      auto curEnd = PsiRichText::findMarker(cur, anchorName + "/end");
                      if (curEnd.isNull())
                          return;

                      bool doScroll = atBottom();
                      cur.setPosition(curEnd.position() + 1, QTextCursor::KeepAnchor);
                      // qDebug() << "DEBUG SELECTED:" << cur.selectedText();
                      cur.removeSelectedText();
                      cur.insertHtml(QString("<img src=\"%1\"/>").arg(anchorName));
                      if (doScroll)
                          QTimer::singleShot(0, this, &PsiTextView::scrollToBottom);
                      setTextCursor(prevCur);
                  });
                  item->autoDownload(autoLimit);

                  QString sizeStr;
                  if (item->fileSize()) {
                      qlonglong div;
                      QString   unit = TextUtil::sizeUnit(qlonglong(*item->fileSize()), &div);
                      sizeStr        = TextUtil::roundedNumber(qint64(*item->fileSize()), div) + unit;
                  } else {
                      sizeStr = item->isUrlOnly() ? tr("unknown size") : QStringLiteral("stream");
                  }

                  const QString label
                      = QString("%1 (%2, %3)")
                            .arg(TextUtil::escape(item->displayName()), TextUtil::escape(item->mimeType()), sizeStr);
                  QUrl simpleUrl = item->simpleSource();
                  if (simpleUrl.isValid()) {
                      return { QTextCharFormat(),
                               QString("<marker id=\"%1/start\"><a href=\"%2\">%3</a><marker id=\"%4/end\">")
                                   .arg(anchorName, TextUtil::escape(simpleUrl.toString(QUrl::FullyEncoded)), label,
                                        anchorName) };
                  }

                  return { QTextCharFormat(),
                           QString("<marker id=\"%1/start\">%2<marker id=\"%3/end\">")
                               .arg(anchorName,
                                    item->downloadsItself(autoLimit)
                                        ? tr("Shared media is downloading (%1, %2)")
                                              .arg(TextUtil::escape(item->mimeType()), sizeStr)
                                        : label,
                                    anchorName) };
              }

              // Other files: a link to the source, or at least the name.
              const QUrl    simpleUrl = item->simpleSource();
              const QString name      = TextUtil::escape(item->displayName());
              if (simpleUrl.isValid())
                  return { QTextCharFormat(),
                           QString("<a href=\"%1\">%2</a>")
                               .arg(TextUtil::escape(simpleUrl.toString(QUrl::FullyEncoded)), name) };
              return { QTextCharFormat(), name };
          } },
        { "marker",
          [this](const QStringView &html, int) -> PsiRichText::ParserRet {
              auto attrs = d->parseHtmlAttrs(html);
              auto id    = attrs.value("id");
              return { PsiRichText::markerFormat(id), QString() };
          } }
    };
}

PsiTextView::~PsiTextView()
{
    delete d->videoCtrl;
    delete d->voiceMsgCtrl; // remove before QTextDocument is destroyed
}

/**
 * Reimplemented createStandardContextMenu(const QPoint &position)
 * for creating of custom context menu
 */

QMenu *PsiTextView::createStandardContextMenu(const QPoint &position)
{
    QTextCursor textcursor = cursorForPosition(position);
    QMenu      *menu;
    QString     anc = anchorAt(position);
    if (!anc.isEmpty()) {
        // menu = URLObject::getInstance()->createPopupMenu(anc);
        const QString href      = textcursor.charFormat().anchorHref();
        auto          clickPos  = textcursor.position();
        const auto    inAnchor  = [&href](const QTextCursor &c) {
            const auto format = c.charFormat();
            return format.isAnchor() && format.anchorHref() == href;
        };
        // we rely on cow to quickly find boundaries. A link may start or end
        // the whole document (one message per document in the native chat),
        // where movePosition() fails; stop there instead of looping forever.
        while (inAnchor(textcursor) && !textcursor.atBlockStart()
               && textcursor.movePosition(QTextCursor::PreviousCharacter)) { }
        textcursor.setPosition(qMin(clickPos + 1, document()->characterCount() - 1), QTextCursor::KeepAnchor);
        while (inAnchor(textcursor) && !textcursor.atBlockEnd()
               && textcursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor)) { }
        if (!inAnchor(textcursor))
            textcursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
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

void PsiTextView::setKeepAtBottom(bool v) { d->keepAtBottom = v; }

bool PsiTextView::isSelectedBlock()
{
    if (textCursor().hasSelection()) {
        const QTextCursor &cursor = textCursor();
        const QTextBlock  &block  = cursor.block();
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
void PsiTextView::clearQuoteFormatting()
{
    hasQuotes_ = false;
}

bool PsiTextView::isQuotedBlock(const QTextBlock &block) const
{
    return block.isValid() && block.blockFormat().property(QuoteLevelProperty).toInt() > 0;
}

void PsiTextView::markQuotedLines()
{
    if (!quoteStylingEnabled_)
        return;
    QTextDocument *doc = document();
    // Give every quoted line a block of its own: turn the line breaks around
    // it into paragraph breaks. Each replacement keeps the positions.
    QList<int> breaks;
    const int  end   = doc->characterCount() - 1;
    int        start = 0;
    while (start <= end) {
        int stop = start;
        QString line;
        while (stop < end && doc->characterAt(stop) != QChar::ParagraphSeparator
               && doc->characterAt(stop) != QChar::LineSeparator)
            line += doc->characterAt(stop++);
        if (quoteMarkers(line)) {
            if (start > 0 && doc->characterAt(start - 1) == QChar::LineSeparator)
                breaks.append(start - 1);
            if (stop < end && doc->characterAt(stop) == QChar::LineSeparator)
                breaks.append(stop);
        }
        start = stop + 1;
    }
    if (breaks.isEmpty() && !quoteMarkers(doc->begin().text()))
        return;
    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    for (int at : std::as_const(breaks)) {
        cursor.setPosition(at);
        cursor.setPosition(at + 1, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertBlock();
    }
    // Then drop the markers and indent each quoted block by its level.
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        int       length = 0;
        const int level  = quoteMarkers(block.text(), &length);
        if (!level)
            continue;
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + length, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        QTextBlockFormat format;
        format.setLeftMargin(level * QuoteIndent);
        format.setProperty(QuoteLevelProperty, level);
        cursor.mergeBlockFormat(format);
        hasQuotes_ = true;
    }
    cursor.endEditBlock();
    applyQuoteColor();
}

void PsiTextView::setQuoteColor(const QColor &color)
{
    if (quoteColor_ == color)
        return;
    quoteColor_ = color;
    applyQuoteColor();
}

void PsiTextView::applyQuoteColor()
{
    if (!hasQuotes_ || !quoteColor_.isValid())
        return;
    // The text of quoted blocks in the quote colour; links keep theirs.
    QTextCursor cursor(document());
    cursor.beginEditBlock();
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        if (!block.blockFormat().hasProperty(QuoteLevelProperty))
            continue;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid() || fragment.charFormat().isAnchor() || fragment.charFormat().isImageFormat())
                continue;
            cursor.setPosition(fragment.position());
            cursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
            QTextCharFormat format;
            format.setForeground(quoteColor_);
            cursor.mergeCharFormat(format);
        }
    }
    cursor.endEditBlock();
    viewport()->update();
}

void PsiTextView::paintEvent(QPaintEvent *event)
{
    QTextEdit::paintEvent(event);
    if (!hasQuotes_)
        return;
    QPainter     painter(viewport());
    const QColor color  = quoteColor_.isValid() ? quoteColor_ : palette().color(QPalette::Mid);
    const qreal  left   = document()->documentMargin() - horizontalScrollBar()->value();
    const qreal  scroll = verticalScrollBar()->value();
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().property(QuoteLevelProperty).toInt();
        if (!level)
            continue;
        const QRectF rect = document()->documentLayout()->blockBoundingRect(block).translated(0, -scroll);
        if (rect.bottom() < event->rect().top() || rect.top() > event->rect().bottom())
            continue;
        for (int i = 0; i < level; ++i)
            painter.fillRect(QRectF(left + i * QuoteIndent, rect.top(), QuoteBarWidth, rect.height()), color);
    }
}


void PsiTextView::scrollToBottom() { verticalScrollBar()->setValue(verticalScrollBar()->maximum()); }

/**
 * Scrolls the vertical scroll bar to its minimum position i.e. to the top.
 */
void PsiTextView::scrollToTop() { verticalScrollBar()->setValue(verticalScrollBar()->minimum()); }

void PsiTextView::setRichText(const QString &text)
{
    clearQuoteFormatting();
    PsiRichText::setText(document(), text, d->objectParsers);
    markQuotedLines();
}

// Legacy text logs append fragments rather than replacing the whole document.
// Only revisit existing blocks when the new fragment contains a possible
// beginning-of-line quote marker. This avoids walking a long chat log for
// virtually every ordinary incoming message.
static bool containsQuotedLine(const QString &text)
{
    return text.startsWith(QLatin1Char('>'))
        || text.contains(QString(QChar::LineSeparator) + QLatin1Char('>'))
        || text.contains(QString(QChar::ParagraphSeparator) + QLatin1Char('>'))
        || text.contains(QStringLiteral("\n>"));
}



/**
 * This function is provided for convenience. Please see
 * PsiRichText::appendText() documentation for usage details.
 */
void PsiTextView::appendText(const QString &text) { appendTextWithRange(text); }

QTextCursor PsiTextView::appendTextWithRange(const QString &text)
{
    QTextCursor            cursor    = textCursor();
    PsiRichText::Selection selection = PsiRichText::saveSelection(this, cursor);
    QTextCursor            inserted;

    PsiRichText::appendText(document(), cursor, text, true, d->objectParsers, &inserted);

    PsiRichText::restoreSelection(this, cursor, selection);
    setTextCursor(cursor);
    if (quoteStylingEnabled_ && containsQuotedLine(inserted.selectedText()))
        markQuotedLines();
    return inserted;
}

/**
 * This function is provided for convenience. Please see
 * PsiRichText::appendText() documentation for usage details.
 */
void PsiTextView::insertText(const QString &text, QTextCursor &cursor) { insertTextWithRange(text, cursor); }

QTextCursor PsiTextView::insertTextWithRange(const QString &text, QTextCursor &cursor)
{
    Q_ASSERT(!cursor.isNull());
    QTextCursor            selCursor = textCursor();
    PsiRichText::Selection selection = PsiRichText::saveSelection(this, selCursor);
    QTextCursor            inserted;

    // qDebug() << "DEBUG TO INSERT:" << text;
    PsiRichText::appendText(document(), cursor, text, false, d->objectParsers, &inserted);

    PsiRichText::restoreSelection(this, selCursor, selection);
    setTextCursor(selCursor);
    if (quoteStylingEnabled_ && containsQuotedLine(inserted.selectedText()))
        markQuotedLines();
    return inserted;
}

QTextCursor PsiTextView::insertTextFragmentWithRange(const QString &text, QTextCursor &cursor)
{
    Q_ASSERT(!cursor.isNull());
    QTextCursor            selCursor = textCursor();
    PsiRichText::Selection selection = PsiRichText::saveSelection(this, selCursor);
    QTextCursor            inserted;

    PsiRichText::insertTextFragment(document(), cursor, text, d->objectParsers, &inserted);

    PsiRichText::restoreSelection(this, selCursor, selection);
    setTextCursor(selCursor);
    if (quoteStylingEnabled_ && containsQuotedLine(inserted.selectedText()))
        markQuotedLines();
    return inserted;
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
    QTextDocument selected;
    QTextCursor(&selected).insertFragment(textCursor().selection());
    // Quoted blocks are copied with their markers again.
    for (QTextBlock block = selected.begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().property(QuoteLevelProperty).toInt();
        if (level)
            QTextCursor(block).insertText(QString(level, QLatin1Char('>')) + QLatin1Char(' '));
    }
    const QString plain = PsiRichText::convertToPlainText(&selected);
    auto         *data  = new QMimeData;
    data->setText(plain);
    data->setHtml(Qt::convertFromPlainText(plain));
    return data;
}


/**
 * Ensures that if PsiTextView was scrolled to bottom when resize
 * operation happened, it will still be scrolled to bottom after the fact.
 */
void PsiTextView::resizeEvent(QResizeEvent *e)
{
    if (!d->keepAtBottom) {
        QTextEdit::resizeEvent(e);
        return;
    }
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
