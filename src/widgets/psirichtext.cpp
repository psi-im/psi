/*
 * psirichtext.h - helper functions to handle Icons in QTextDocuments
 * Copyright (C) 2006  Michail Pishchagin
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

#include "psirichtext.h"
#include "common.h"

#include <QAbstractTextDocumentLayout> // for QTextObjectInterface
#include <QApplication>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QFont>
#include <QList>
#include <QPainter>
#include <QQueue>
#include <QRegExp>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QTextFrame>
#include <QUrl>
#include <QVariant>

#include <cmath>

#ifndef WIDGET_PLUGIN
#include "iconset.h"
#include "qite.h"
#include "textutil.h"
#else
class Iconset;
class PsiIcon;
#endif

static const int   IconFormatType   = QTextFormat::UserObject;
static const int   MarkerFormatType = QTextFormat::UserObject + 1;
static QStringList allowedImageDirs;

//----------------------------------------------------------------------------
// TextIconFormat
//----------------------------------------------------------------------------

class TextIconFormat : public QTextCharFormat {
public:
    TextIconFormat(const QString &iconName, const QString &text, qreal size = -1);

    enum Property {
        IconName = QTextFormat::UserProperty + 1,
        IconText = QTextFormat::UserProperty + 2,
        IconSize = QTextFormat::UserProperty + 3
    };
};

TextIconFormat::TextIconFormat(const QString &iconName, const QString &text, qreal size) : QTextCharFormat()
{
    Q_UNUSED(text);

    setObjectType(IconFormatType);
    QTextFormat::setProperty(IconName, iconName);
    QTextFormat::setProperty(IconText, text);
    QTextFormat::setProperty(IconSize, size);

    setVerticalAlignment(size < -1 ? QTextCharFormat::AlignBottom : QTextCharFormat::AlignNormal);

    // TODO: handle animations
}

//----------------------------------------------------------------------------
// IconTextObjectInterface
//----------------------------------------------------------------------------

#ifndef WIDGET_PLUGIN

class TextIconHandler : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)
public:
    TextIconHandler(QObject *parent = nullptr);

    virtual QSizeF intrinsicSize(QTextDocument *doc, int posInDocument, const QTextFormat &format);
    virtual void   drawObject(QPainter *painter, const QRectF &rect, QTextDocument *doc, int posInDocument,
                              const QTextFormat &format);
};

TextIconHandler::TextIconHandler(QObject *parent) : QObject(parent) { }

QSizeF TextIconHandler::intrinsicSize(QTextDocument *doc, int posInDocument, const QTextFormat &format)
{
    Q_UNUSED(doc);
    Q_UNUSED(posInDocument)
    const QTextCharFormat charFormat = format.toCharFormat();

    /*
     * >0 - size in points
     *  0 - original size
     * <0 - scale factor relative to font size after converting to absolute(positive) value
     */
    auto htmlSize = charFormat.doubleProperty(TextIconFormat::IconSize);
    auto iconName = charFormat.stringProperty(TextIconFormat::IconName);

    QSizeF ret;
    auto   icon = IconsetFactory::iconPtr(iconName);
    if (!icon) {
        qWarning("invalid icon: %s", qPrintable(iconName));
        ret = QSizeF();
    } else if (htmlSize > 0) {
        auto pxSize = pointToPixel(htmlSize);
        ret         = icon->size(QSize(pxSize, pxSize));
    } else if (htmlSize == 0) {
        ret = icon->size();
    } else {
        auto relSize = QFontInfo(charFormat.font()).pixelSize() * std::fabs(double(htmlSize));
        if (icon->isScalable()) {
            ret = icon->size(QSize(0, relSize));
        } else if (icon->size().height() > relSize * HugeIconTextViewK) { // still too huge
            ret = icon->size().scaled(QSize(icon->size().width(), relSize), Qt::KeepAspectRatio);
        } else {
            ret = icon->size();
        }
    }

    return ret;
}

void TextIconHandler::drawObject(QPainter *painter, const QRectF &rect, QTextDocument *doc, int posInDocument,
                                 const QTextFormat &format)
{
    Q_UNUSED(doc);
    Q_UNUSED(posInDocument);

    const QTextCharFormat charFormat = format.toCharFormat();
    auto const            iconName   = charFormat.stringProperty(TextIconFormat::IconName);

    if (rect.isNull()) {
        qWarning("Null rect for drawing icon %s", qPrintable(iconName));
        return;
    }

    auto pixmap      = IconsetFactory::iconPixmap(iconName, rect.size().toSize());
    auto alignedSize = rect.size().toSize();
    if (alignedSize != pixmap.size()) {
        pixmap = pixmap.scaled(alignedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    painter->drawPixmap(rect, pixmap, pixmap.rect());
}
#endif // WIDGET_PLUGIN

//----------------------------------------------------------------------------
// TextMarkerFormat
//----------------------------------------------------------------------------

class TextMarkerFormat : public QTextCharFormat {
public:
    TextMarkerFormat(const QString &id);

    enum Property { MarkerId = QTextFormat::UserProperty + 3 };
};

TextMarkerFormat::TextMarkerFormat(const QString &id) : QTextCharFormat()
{
    setObjectType(MarkerFormatType);
    QTextFormat::setProperty(MarkerId, id);
}

//----------------------------------------------------------------------------
// PsiRichText
//----------------------------------------------------------------------------

/**
 * You need to call this function on your QTextDocument to make it
 * capable of displaying inline Icons. Uninstaller ships separately.
 */
void PsiRichText::install(QTextDocument *doc)
{
    Q_ASSERT(doc);
#ifndef WIDGET_PLUGIN
    static TextIconHandler *handler = nullptr;
    if (!handler) {
        handler = new TextIconHandler(qApp);
    }

    doc->documentLayout()->registerHandler(IconFormatType, handler);
#endif
}

/**
 * Make sure that QTextDocument has correctly layouted its text.
 */
void PsiRichText::ensureTextLayouted(QTextDocument *doc, int documentWidth, Qt::Alignment align,
                                     Qt::LayoutDirection layoutDirection, bool textWordWrap)
{
    // from QLabelPrivate::ensureTextLayouted

    Q_UNUSED(textWordWrap);
    Q_UNUSED(layoutDirection);
    Q_UNUSED(align);
    // bah, QTextDocumentLayout is private :-/
    // QTextDocumentLayout *lout = qobject_cast<QTextDocumentLayout *>(doc->documentLayout());
    // Q_ASSERT(lout);
    //
    // int flags = (textWordWrap ? 0 : Qt::TextSingleLine) | align;
    // flags |= (layoutDirection == Qt::RightToLeft) ? QTextDocumentLayout::RTL : QTextDocumentLayout::LTR;
    // lout->setBlockTextFlags(flags);
    //
    // if (textWordWrap) {
    //     // ensure that we break at words and not just about anywhere
    //     lout->setWordWrapMode(QTextOption::WordWrap);
    // }

    QTextFrameFormat fmt = doc->rootFrame()->frameFormat();
    fmt.setMargin(0);
    doc->rootFrame()->setFrameFormat(fmt);
    doc->setTextWidth(documentWidth);
}

/**
 * Inserts an PsiIcon into document.
 * \param cursor this cursor is used to insert icon
 * \param iconName icon's name, by which it could be found in IconsetFactory
 * \param iconText icon's text, used when copy operation is performed
 */
void PsiRichText::insertIcon(QTextCursor &cursor, const QString &iconName, const QString &iconText)
{
#ifdef WIDGET_PLUGIN
    Q_UNUSED(cursor);
    Q_UNUSED(iconName);
    Q_UNUSED(iconText);
#else
    QTextCharFormat format = cursor.charFormat();

    TextIconFormat icon(iconName, iconText);
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), icon);

    cursor.setCharFormat(format);
#endif
}

typedef QQueue<QTextCharFormat *> TextCharFormatQueue;

/**
 * Adds null format to queue for all ObjectReplacementCharacters that were
 * already in the text. Returns passed \param text to save some code.
 */
static QStringRef preserveOriginalObjectReplacementCharacters(const QStringRef &text, TextCharFormatQueue *queue)
{
    int objReplChars = 0;
    objReplChars += text.count(QChar::ObjectReplacementCharacter);
    // <img> tags are replaced to ObjectReplacementCharacters
    // internally by Qt functions.
    // But we must be careful if some other character instead of
    // 0x20 is used immediately after tag opening, this could
    // create a hole. ejabberd protects us from it though.
    objReplChars += text.count("<img ");
    for (int i = objReplChars; i; i--) {
        queue->enqueue(nullptr);
    }

    return text;
}

/**
 * Replaces all <icon> tags with handy ObjectReplacementCharacters, and
 * adds appropriate format to the \param queue. Returns processed
 * \param text.
 */
static QString convertIconsToObjectReplacementCharacters(const QStringRef &text, TextCharFormatQueue *queue,
                                                         int insertedAfter, const PsiRichText::ParsersMap &parsers)
{
    QString    result;
    QStringRef work(text);

    int start = -1;
    forever
    {
        start = work.indexOf("<", start + 1);
        if (start == -1)
            break;
        if (work.mid(start + 1, 4) == "icon") {
            // Format: <icon name="" text="">
            static QRegularExpression rxName("name=\"([^\"]+)\"");
            static QRegularExpression rxText("text=\"([^\"]+)\"");
            static QRegularExpression rxSize("size=\"([^\"]+)\"");

            result += preserveOriginalObjectReplacementCharacters(work.left(start), queue);

            int end = work.indexOf(">", start);
            Q_ASSERT(end != -1);

            QStringRef fragment  = work.mid(start, end - start);
            auto       matchName = rxName.match(fragment);
            if (matchName.hasMatch()) {
#ifndef WIDGET_PLUGIN
                QString iconName = TextUtil::unescape(matchName.capturedTexts().at(1));
                QString iconText;
                auto    matchText = rxText.match(fragment);
                if (matchText.hasMatch()) {
                    iconText = TextUtil::unescape(matchText.capturedTexts().at(1));
                }

                double iconSize  = -1; // not defined. will be resized to be aligned with text if necessary
                auto   matchSize = rxSize.match(fragment);
                if (matchSize.hasMatch()) {
                    auto szText = matchSize.capturedTexts().at(1);
                    if (szText == QLatin1String("original"))
                        iconSize = 0; // use original size
                    else
                        iconSize = szText.toDouble(); // size in points
                }
#else
                QString iconName = matchName.capturedTexts()[1];
                QString iconText = matchText.capturedTexts()[1];
                QString iconSize = matchSize.capturedTexts()[1];
#endif
                queue->enqueue(new TextIconFormat(iconName, iconText, iconSize));
                result += QChar::ObjectReplacementCharacter;
            }

            work  = work.mid(end + 1);
            start = -1;
        } else {
            auto it = parsers.constBegin();
            for (; it != parsers.constEnd(); ++it) {
                if (work.mid(start + 1, it.key().length()) == it.key()) {
                    // if parsers key matches with html element name
                    result += preserveOriginalObjectReplacementCharacters(work.left(start), queue);

                    int end = work.indexOf(">", start);
                    Q_ASSERT(end != -1);

                    // take attributes part of the tag
                    QStringRef fragment = work.mid(start + it.key().length() + 1, end - start - it.key().length() - 1);
                    QString    replaceHtml;
                    QTextCharFormat charFormat;

                    std::tie(charFormat, replaceHtml) = it.value()(fragment, insertedAfter);
                    if (replaceHtml.size()) {
                        result += convertIconsToObjectReplacementCharacters(QStringRef(&replaceHtml), queue,
                                                                            insertedAfter, parsers);
                    }
                    if (charFormat.isValid()) {
                        queue->enqueue(new QTextCharFormat(charFormat));
                        result += QChar::ObjectReplacementCharacter;
                    }

                    work  = work.mid(end + 1);
                    start = -1;

                    break;
                }
            }
        }
    }

    result += preserveOriginalObjectReplacementCharacters(work, queue);
    return result;
}

/**
 * Applies text formats from \param queue to all ObjectReplacementCharacters
 * in \param doc, starting from \param cursor's position.
 */
static void applyFormatToIcons(QTextDocument *doc, TextCharFormatQueue *queue, QTextCursor &cursor)
{
    QTextCursor searchCursor = cursor;
    forever
    {
        searchCursor = doc->find(QString(QChar::ObjectReplacementCharacter), searchCursor);
        if (searchCursor.isNull() || queue->isEmpty()) {
            break;
        }
        QTextCharFormat *format = queue->dequeue();
        if (format) {
            searchCursor.setCharFormat(*format);
            delete format;
        }
    }

    // if it's not true, there's a memleak
    Q_ASSERT(queue->isEmpty());

    // clear the selection that's left after successful QTextDocument::find()
    cursor.clearSelection();
}

/**
 * Groups some related function calls together.
 */
static void appendTextHelper(QTextDocument *doc, QString text, QTextCursor &cursor,
                             const PsiRichText::ParsersMap &parsers)
{
    TextCharFormatQueue queue;

    // we need to save this to start searching from
    // here when applying format to icons
    int initialpos = cursor.position();

    // prepare images and remove insecure images
    static QRegExp imgRe("<img[^>]+src\\s*=\\s*(\"[^\"]*\"|'[^']*')[^>]*>");
    QString        replace;
    for (int pos = 0; (pos = imgRe.indexIn(text, pos)) != -1;) {
        replace.clear();
        QString imgSrc    = imgRe.cap(1).mid(1, imgRe.cap(1).size() - 2);
        QUrl    imgSrcUrl = QUrl::fromEncoded(imgSrc.toLatin1());
        if (imgSrcUrl.isValid()) {
            if (imgSrcUrl.scheme() == "data") {
                static QRegExp dataRe("^[a-zA-Z]+/[a-zA-Z]+;base64,([a-zA-Z0-9/=+%]+)$");
                if (dataRe.indexIn(imgSrcUrl.path()) != -1) {
                    const QByteArray ba = QByteArray::fromBase64(dataRe.cap(1).toLatin1());
                    if (!ba.isNull()) {
                        QImage image;
                        if (image.loadFromData(ba)) {
                            replace = "srcdata" + QCryptographicHash::hash(ba, QCryptographicHash::Sha1).toHex();
                            doc->addResource(QTextDocument::ImageResource, QUrl(replace), image);
                        }
                    }
                }
            } else if (imgSrc.startsWith(":/") || (!imgSrcUrl.scheme().isEmpty() && imgSrcUrl.scheme() != "file")) {
                pos += imgRe.matchedLength();
                continue;
            } else {
                // go here when  scheme in ["", "file"] and its not resource
                QString path
                    = QFileInfo(imgSrcUrl.scheme() == "file" ? imgSrcUrl.toLocalFile() : imgSrc).absoluteFilePath();
                bool baseDirFound = false;
                for (const QString &baseDir : qAsConst(allowedImageDirs)) {
                    if (path.startsWith(baseDir)) {
                        baseDirFound = true;
                        break;
                    }
                }
                if (baseDirFound) {
                    if (imgSrcUrl.scheme() == "file") {
                        replace = path;
                    } else {
                        pos += imgRe.matchedLength();
                        continue;
                    }
                }
            }
        }
        if (replace.isEmpty()) {
            text.remove(pos, imgRe.matchedLength());
        } else {
            text.replace(imgRe.pos(1) + 1, imgSrc.size(), replace);
            pos += replace.size() + 1;
        }
    }

    cursor.insertFragment(QTextDocumentFragment::fromHtml(
        convertIconsToObjectReplacementCharacters(QStringRef(&text), &queue, cursor.position(), parsers)));
    cursor.setPosition(initialpos);

    applyFormatToIcons(doc, &queue, cursor);
}

/**
 * Sets entire contents of specified QTextDocument to text.
 * \param text text to append to the QTextDocument. Please note that if you
 *             insert any <icon>s, attributes' values MUST be Qt::escaped.
 */
void PsiRichText::setText(QTextDocument *doc, const QString &text, const ParsersMap &parsers)
{
    QFont font = doc->defaultFont();
    doc->clear();
    QTextCursor     cursor(doc);
    QTextCharFormat charFormat = cursor.charFormat();
    charFormat.setFont(font);
    cursor.setCharFormat(charFormat);
    appendText(doc, cursor, text, true, parsers);
}

/**
 * Appends a new paragraph with text to the end of the document.
 * \param text text to append to the QTextDocument. Please note that if you
 *             insert any <icon>s, attributes' values MUST be Qt::escaped.
 */
void PsiRichText::appendText(QTextDocument *doc, QTextCursor &cursor, const QString &text, bool append,
                             const ParsersMap &parsers)
{
    cursor.beginEditBlock();
    if (append) {
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        cursor.clearSelection();
    }
    if (!cursor.atBlockStart()) {
        cursor.insertBlock();

        // clear trackbar for new blocks
        QTextBlockFormat blockFormat = cursor.blockFormat();
        blockFormat.setTopMargin(0);
        blockFormat.setBottomMargin(0);
        blockFormat.clearProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);
        cursor.setBlockFormat(blockFormat);
    }

    appendTextHelper(doc, text, cursor, parsers);

    cursor.endEditBlock();
}
#if 0
void PsiRichText::insertText(QTextDocument *doc, QTextCursor &cursor, const QString &text)
{
    cursor.beginEditBlock();
    appendTextHelper(doc, text, cursor);
    cursor.endEditBlock();
}
#endif
/**
 * Call this function on your QTextDocument to get plain text
 * representation, and all Icons will be replaced by their
 * initial text.
 */
QString PsiRichText::convertToPlainText(const QTextDocument *doc)
{
    QString                 obrepl = QString(QChar::ObjectReplacementCharacter);
    QQueue<QTextCharFormat> queue;
    QTextCursor             nc = doc->find(obrepl, 0);
    QTextCursor             cursor;

    while (!nc.isNull()) {
        queue.enqueue(nc.charFormat());

        cursor = nc;
        nc     = doc->find(obrepl, cursor);
    }

    QString raw = doc->toPlainText();

    QStringList parts = raw.split(obrepl);

    QString result = parts.at(0);

    for (int i = 1; i < parts.size(); ++i) {
        if (!queue.isEmpty()) {
            QTextCharFormat format = queue.dequeue();
            if ((format).objectType() == IconFormatType) {
                result += format.stringProperty(TextIconFormat::IconText);
            }
        }
        result += parts.at(i);
    }
    return result;
}

/**
 * Adds \a emoticon to \a textEdit.
 */
void PsiRichText::addEmoticon(QTextEdit *textEdit, const QString &emoticon)
{
    Q_ASSERT(textEdit);
    if (!textEdit || emoticon.isEmpty())
        return;

    QString                text      = emoticon + ' ';
    QTextCursor            cursor    = textEdit->textCursor();
    PsiRichText::Selection selection = PsiRichText::saveSelection(textEdit, cursor);

    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 1);
    if (!cursor.selectedText().isEmpty() && !cursor.selectedText().at(0).isSpace()) {
        text = " " + text;
    }

    textEdit->insertPlainText(text);

    PsiRichText::restoreSelection(textEdit, cursor, selection);
}

void PsiRichText::setAllowedImageDirs(const QStringList &dirs) { allowedImageDirs = dirs; }

QTextCharFormat PsiRichText::markerFormat(const QString &uniqueId) { return TextMarkerFormat(uniqueId); }

void PsiRichText::insertMarker(QTextCursor &cursor, const QString &uniqueId)
{
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), TextMarkerFormat(uniqueId));
}

// returns cursor with selection on marker
QTextCursor PsiRichText::findMarker(const QTextCursor &cursor, const QString &uniqueId)
{
    QString     obrepl = QString(QChar::ObjectReplacementCharacter);
    auto        doc    = cursor.document();
    QTextCursor nc     = doc->find(obrepl, cursor);

    while (!nc.isNull()) {
        if (nc.charFormat().objectType() == MarkerFormatType
            && nc.charFormat().stringProperty(TextMarkerFormat::MarkerId) == uniqueId) {
            break;
        }
        nc = doc->find(obrepl, nc);
    }
    return nc;
}

/**
 * Saves current Selection in a structure, so it could be restored at later time.
 */
PsiRichText::Selection PsiRichText::saveSelection(QTextEdit *textEdit, QTextCursor &cursor)
{
    Q_UNUSED(textEdit)

    Selection selection;
    selection.start = selection.end = -1;

    if (cursor.hasSelection()) {
        selection.start = cursor.selectionStart();
        selection.end   = cursor.selectionEnd();
    }

    return selection;
}

/**
 * Restores a Selection that was previously saved by call to saveSelection().
 */
void PsiRichText::restoreSelection(QTextEdit *textEdit, QTextCursor &cursor, PsiRichText::Selection selection)
{
    Q_UNUSED(textEdit)

    if (selection.start != -1 && selection.end != -1) {
        cursor.setPosition(selection.start, QTextCursor::MoveAnchor);
        cursor.setPosition(selection.end, QTextCursor::KeepAnchor);
    }
}

#ifndef WIDGET_PLUGIN
#include "psirichtext.moc"
#endif
