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
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QTextFrame>
#include <QUrl>
#include <QVariant>

#ifndef WIDGET_PLUGIN
#include "iconset.h"
#include "textutil.h"
#else
class Iconset;
class PsiIcon;
#endif

static const int   IconFormatType   = QTextFormat::UserObject;
static const int   MarkerFormatType = QTextFormat::UserObject + 1;
static QStringList allowedImageDirs;

struct HtmlSize {
    enum Unit { Em, Pt, Px };

    float size;
    Unit  unit;
};
Q_DECLARE_METATYPE(HtmlSize)

namespace {
std::optional<HtmlSize> parseSize(QStringView view)
{
    HtmlSize hsize { 0.0f, HtmlSize::Px };
    bool     ok = true;
    if (view.endsWith(QLatin1String("em"))) {
        view.chop(2);
        hsize.size = view.toFloat(&ok);
        hsize.unit = HtmlSize::Em;
    } else if (view.endsWith(QLatin1String("pt"))) {
        view.chop(2);
        hsize.size = view.toInt(&ok);
        hsize.unit = HtmlSize::Pt;
    } else {
        if (view.endsWith(QLatin1String("px"))) {
            view.chop(2);
        }
        hsize.size = view.toInt(&ok);
    }
    if (ok) {
        return hsize;
    }
    return {};
}

int htmlSizeToPixels(const HtmlSize &size, const QTextCharFormat &format)
{
    if (size.unit == HtmlSize::Pt) {
        return pointToPixel(size.size);
    }
    if (size.unit == HtmlSize::Em) {
        auto fs = format.fontPointSize() ? format.fontPointSize() : format.font().pointSize();
        return int(size.size * pointToPixel(fs) + 0.5);
    }
    return size.size;
}

}

//----------------------------------------------------------------------------
// TextIconFormat
//----------------------------------------------------------------------------

class TextIconFormat : public QTextCharFormat {
public:
    TextIconFormat(const QString &iconName, const QString &text, std::optional<HtmlSize> width = {},
                   std::optional<HtmlSize> height = {}, std::optional<HtmlSize> minWidth = {},
                   std::optional<HtmlSize> minHeight = {}, std::optional<HtmlSize> maxWidth = {},
                   std::optional<HtmlSize> maxHeight = {}, std::optional<VerticalAlignment> valign = {},
                   std::optional<HtmlSize> fontSize = {});

    enum Property {
        IconName      = QTextFormat::UserProperty + 1,
        IconText      = QTextFormat::UserProperty + 2,
        IconWidth     = QTextFormat::UserProperty + 3,
        IconHeight    = QTextFormat::UserProperty + 4,
        IconMinWidth  = QTextFormat::UserProperty + 5,
        IconMinHeight = QTextFormat::UserProperty + 6,
        IconMaxWidth  = QTextFormat::UserProperty + 7,
        IconMaxHeight = QTextFormat::UserProperty + 8,
        IconFontSize  = QTextFormat::UserProperty + 9
    };
};

TextIconFormat::TextIconFormat(const QString &iconName, const QString &text, std::optional<HtmlSize> width,
                               std::optional<HtmlSize> height, std::optional<HtmlSize> minWidth,
                               std::optional<HtmlSize> minHeight, std::optional<HtmlSize> maxWidth,
                               std::optional<HtmlSize>                           maxHeight,
                               std::optional<QTextCharFormat::VerticalAlignment> valign,
                               std::optional<HtmlSize>                           fontSize) : QTextCharFormat()
{
    Q_UNUSED(text);

    setObjectType(IconFormatType);
    QTextFormat::setProperty(IconName, iconName);
    QTextFormat::setProperty(IconText, text);
    if (width) {
        QTextFormat::setProperty(IconWidth, QVariant::fromValue<HtmlSize>(*width));
    }
    if (height) {
        QTextFormat::setProperty(IconHeight, QVariant::fromValue<HtmlSize>(*height));
    }
    if (minWidth) {
        QTextFormat::setProperty(IconMinWidth, QVariant::fromValue<HtmlSize>(*minWidth));
    }
    if (minHeight) {
        QTextFormat::setProperty(IconMinHeight, QVariant::fromValue<HtmlSize>(*minHeight));
    }
    if (maxWidth) {
        QTextFormat::setProperty(IconMaxWidth, QVariant::fromValue<HtmlSize>(*maxWidth));
    }
    if (maxHeight) {
        QTextFormat::setProperty(IconMaxHeight, QVariant::fromValue<HtmlSize>(*maxHeight));
    }
    if (valign) {
        setVerticalAlignment(*valign);
    }
    if (fontSize) {
        QTextFormat::setProperty(IconFontSize, QVariant::fromValue<HtmlSize>(*fontSize));
    }

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

private:
    QFont adjustFontSize(const QTextCharFormat charFormat)
    {
        auto propHeight    = charFormat.property(TextIconFormat::IconHeight);
        auto propMinHeight = charFormat.property(TextIconFormat::IconMinHeight);
        auto propMaxHeight = charFormat.property(TextIconFormat::IconMaxHeight);
        auto propFontSize  = charFormat.property(TextIconFormat::IconFontSize);

        std::optional<int> height;
        std::optional<int> minHeight;
        std::optional<int> maxHeight;

        if (propMinHeight.isValid()) {
            minHeight = htmlSizeToPixels(propMinHeight.value<HtmlSize>(), charFormat);
        }
        if (propMaxHeight.isValid()) {
            maxHeight = htmlSizeToPixels(propMaxHeight.value<HtmlSize>(), charFormat);
        }
        if (propHeight.isValid()) { // we want to scale ignoring aspect ratio
            int limitMin = minHeight ? *minHeight : 8;
            int limitMax = maxHeight ? *maxHeight : 1080;
            height       = htmlSizeToPixels(propHeight.value<HtmlSize>(), charFormat);
            height       = qMax(qMin(limitMax, *height), limitMin);
        }

        auto font = charFormat.font();
        int  ps   = font.pixelSize();
        if (ps == -1) {
            ps = pointToPixel(font.pointSizeF());
        }
        if (propFontSize.isValid()) {
            auto fontSize = propFontSize.value<HtmlSize>();
            if (fontSize.unit == HtmlSize::Em) {
                ps *= fontSize.size;
            } else if (fontSize.unit == HtmlSize::Px) {
                ps = fontSize.size;
            } else if (fontSize.unit == HtmlSize::Pt) {
                ps = pointToPixel(fontSize.size);
            }
        }
        if (height) {
            ps = *height;
        } else if (minHeight && maxHeight) {
            ps = (*minHeight + *maxHeight) / 2;
        } else if (minHeight && ps < *minHeight) {
            ps = *minHeight;
        } else if (maxHeight && ps > *maxHeight) {
            ps = *maxHeight;
        }
        font.setPixelSize(ps);
        return font;
    }
};

TextIconHandler::TextIconHandler(QObject *parent) : QObject(parent) { }

QSizeF TextIconHandler::intrinsicSize(QTextDocument *doc, int posInDocument, const QTextFormat &format)
{
    Q_UNUSED(doc);
    Q_UNUSED(posInDocument)
    QSizeF                ret;
    const QTextCharFormat charFormat = format.toCharFormat();

    auto           iconName  = charFormat.stringProperty(TextIconFormat::IconName);
    auto           iconText  = charFormat.stringProperty(TextIconFormat::IconText);
    const PsiIcon *icon      = nullptr;
    bool           doScaling = true;
    if (!iconName.isEmpty()) {
        icon = IconsetFactory::iconPtr(iconName);
        if (!icon) {
            // qWarning("invalid icon: %s", qPrintable(iconName));
            return {};
        }
        ret       = icon->size();
        doScaling = icon->isScalable();
    } else if (!iconText.isEmpty()) {
        auto font = adjustFontSize(charFormat);
        return QFontMetricsF(font).tightBoundingRect(iconText).size() * 1.16; // 1.16 - magic for Windows
    }
    if (ret.isEmpty()) {
        // something went wrong with this icon
        return ret;
    }

    auto propWidth     = charFormat.property(TextIconFormat::IconWidth);
    auto propHeight    = charFormat.property(TextIconFormat::IconHeight);
    auto propMinWidth  = charFormat.property(TextIconFormat::IconMinWidth);
    auto propMinHeight = charFormat.property(TextIconFormat::IconMinHeight);
    auto propMaxWidth  = charFormat.property(TextIconFormat::IconMaxWidth);
    auto propMaxHeight = charFormat.property(TextIconFormat::IconMaxHeight);

    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> minWidth;
    std::optional<int> minHeight;
    std::optional<int> maxWidth;
    std::optional<int> maxHeight;
    QSize              maxSize { 20000, 20000 }; // should be enough fow a few decades

    if (propMinWidth.isValid()) {
        minWidth = htmlSizeToPixels(propMinWidth.value<HtmlSize>(), charFormat);
    }
    if (propMinHeight.isValid()) {
        minHeight = htmlSizeToPixels(propMinHeight.value<HtmlSize>(), charFormat);
    }
    if (propMaxWidth.isValid()) {
        maxSize.setWidth(htmlSizeToPixels(propMaxWidth.value<HtmlSize>(), charFormat));
        maxWidth = maxSize.width();
    }
    if (propMaxHeight.isValid()) {
        maxSize.setHeight(htmlSizeToPixels(propMaxHeight.value<HtmlSize>(), charFormat));
        maxHeight = maxSize.height();
    }
    if (propWidth.isValid()) {
        int limitMin = minWidth ? *minWidth : 8;
        width        = qMax(qMin(maxSize.width(), htmlSizeToPixels(propWidth.value<HtmlSize>(), charFormat)), limitMin);
    }
    if (propHeight.isValid()) { // we want to scale ignoring aspect ratio
        int limitMin = minHeight ? *minHeight : 8;
        height = qMax(qMin(maxSize.height(), htmlSizeToPixels(propHeight.value<HtmlSize>(), charFormat)), limitMin);
    }

    if (width || height) {
        QSize scaledTo;
        if (width) {
            if (height) {
                scaledTo = { *width, *height };
            } else {
                scaledTo = { *width, maxSize.height() };
            }
        } else { // scaling by height by not by width
            scaledTo = { maxSize.width(), *height };
        }
        ret.scale(scaledTo, Qt::KeepAspectRatio);
        doScaling = false;
    } else if (!doScaling) {
        // check where scaling is required even for non-scalable
        doScaling = (ret.width() > maxSize.width() || ret.height() > maxSize.height())
            || (minWidth && ret.width() < *minWidth) || (minHeight && ret.height() < *minHeight);
    }

    if (doScaling) {
        QSizeF desiredSize { 0, 0 };
        if (minWidth) {
            if (maxWidth) {
                desiredSize.setWidth((*minWidth + *maxWidth) / 2.0);
            } else {
                desiredSize.setWidth(qMax(qreal(*minWidth), ret.width()));
            }
        } else if (maxWidth) {
            desiredSize.setWidth(qMin(qreal(*maxWidth), ret.width()));
        }
        if (minHeight) {
            if (maxHeight) {
                desiredSize.setHeight((*minHeight + *maxHeight) / 2.0);
            } else {
                desiredSize.setHeight(qMax(qreal(*minHeight), ret.height()));
            }
        } else if (maxHeight) {
            desiredSize.setHeight(qMin(qreal(*maxHeight), ret.height()));
        }
        if (desiredSize.width() && !desiredSize.height()) {
            desiredSize.setHeight(desiredSize.width() / ret.width() * ret.height());
        }
        if (!desiredSize.width() && desiredSize.height()) {
            desiredSize.setWidth(desiredSize.height() / ret.height() * ret.width());
        }
        if (!desiredSize.isEmpty()) {
            ret = desiredSize;
        }
    }
    // qDebug() << ret;
    return ret;
}

void TextIconHandler::drawObject(QPainter *painter, const QRectF &rect, QTextDocument *doc, int posInDocument,
                                 const QTextFormat &format)
{
    Q_UNUSED(doc);
    Q_UNUSED(posInDocument);

    const QTextCharFormat charFormat = format.toCharFormat();
    auto const            iconName   = charFormat.stringProperty(TextIconFormat::IconName);
    auto const            iconText   = charFormat.stringProperty(TextIconFormat::IconText);

    if (rect.isNull()) {
        qWarning("Null rect for drawing icon %s: %s", qPrintable(iconName), qPrintable(iconText));
        return;
    }

    // qDebug() << "render icon " << iconText << iconName << " in " << rect;
    if (iconName.isEmpty()) {
        auto font = adjustFontSize(charFormat);
        font.setPixelSize(font.pixelSize());
        painter->setFont(font);
        painter->drawText(rect, Qt::AlignCenter, iconText);
    } else {
        auto pixmap      = IconsetFactory::iconPixmap(iconName, rect.size().toSize());
        auto alignedSize = rect.size().toSize();
        if (alignedSize != pixmap.size()) {
            pixmap = pixmap.scaled(alignedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        // qDebug() << "render icon " << iconName << " size " << pixmap.size() << " in " << rect;
        painter->drawPixmap(rect, pixmap, pixmap.rect());
    }
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
static QStringView preserveOriginalObjectReplacementCharacters(const QStringView &text, TextCharFormatQueue *queue)
{
    int objReplChars = 0;
    objReplChars += text.count(QChar::ObjectReplacementCharacter);
    // <img> tags are replaced to ObjectReplacementCharacters
    // internally by Qt functions.
    // But we must be careful if some other character instead of
    // 0x20 is used immediately after tag opening, this could
    // create a hole. ejabberd protects us from it though.
    objReplChars += text.count(QStringLiteral(u"<img "));
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
static QString convertIconsToObjectReplacementCharacters(const QStringView &text, TextCharFormatQueue *queue,
                                                         int insertedAfter, const PsiRichText::ParsersMap &parsers)
{
    QString            result;
    QStringView        work(text);
    static QStringList emojiFontFamilies = { "Apple Color Emoji", "Noto Color Emoji", "Segoe UI Emoji" };

    int start = -1;
    forever
    {
        start = work.indexOf(QLatin1Char('<'), start + 1);
        if (start == -1)
            break;
        if (work.mid(start + 1, 5) == QLatin1String { "icon " }) {
            // Format: <icon name="" text="">
            static QRegularExpression rxName("([a-z-]+)=\"([^\"]+)\"");

            result += preserveOriginalObjectReplacementCharacters(work.left(start), queue);

            int end = work.indexOf(QLatin1Char { '>' }, start);
            Q_ASSERT(end != -1);

            QStringView fragment = work.mid(start + 6, end - start);

            std::optional<HtmlSize> width;
            std::optional<HtmlSize> height;
            std::optional<HtmlSize> minWidth;
            std::optional<HtmlSize> minHeight;
            std::optional<HtmlSize> maxWidth;
            std::optional<HtmlSize> maxHeight;
            std::optional<HtmlSize> fontSize;
            QString                 iconName;
            QString                 iconText;
            QString                 iconType;

            std::optional<QTextCharFormat::VerticalAlignment> valign;

            QRegularExpressionMatchIterator i = rxName.globalMatch(fragment);
            while (i.hasNext()) {
                auto match = i.next();
                if (match.capturedView(1) == QLatin1String("name")) {
                    iconName = match.captured(2);
                } else if (match.capturedView(1) == QLatin1String("text")) {
                    iconText = match.capturedView(2).contains(QLatin1Char('%'))
                        ? QUrl::fromPercentEncoding(match.capturedView(2).toUtf8())
                        : match.capturedView(2).toString();
                } else if (match.capturedView(1) == QLatin1String("width")) {
                    width = parseSize(match.capturedView(2));
                } else if (match.capturedView(1) == QLatin1String("height")) {
                    height = parseSize(match.capturedView(2));
                } else if (match.capturedView(1) == QLatin1String("min-width")) {
                    minWidth = parseSize(match.capturedView(2));
                } else if (match.capturedView(1) == QLatin1String("min-height")) {
                    minHeight = parseSize(match.capturedView(2));
                } else if (match.capturedView(1) == QLatin1String("max-width")) {
                    maxWidth = parseSize(match.capturedView(2));
                } else if (match.capturedView(1) == QLatin1String("max-height")) {
                    maxHeight = parseSize(match.capturedView(2));
                } else if (match.capturedView(1) == QLatin1String("valign")) {
                    static QHash<QString, QTextCharFormat::VerticalAlignment> vaMap {
                        { { QLatin1String("normal"), QTextCharFormat::AlignNormal },
                          { QLatin1String("superscript"), QTextCharFormat::AlignSuperScript },
                          { QLatin1String("subscript"), QTextCharFormat::AlignSubScript },
                          { QLatin1String("middle"), QTextCharFormat::AlignMiddle },
                          { QLatin1String("bottom"), QTextCharFormat::AlignBottom },
                          { QLatin1String("top"), QTextCharFormat::AlignTop },
                          { QLatin1String("baseline"), QTextCharFormat::AlignBaseline } }
                    };
                    auto it = vaMap.find(match.capturedView(2).toString());
                    if (it != vaMap.end()) {
                        valign = *it;
                    }
                } else if (match.capturedView(1) == QLatin1String("type")) {
                    iconType = match.captured(2);
                } else if (match.capturedView(1) == QLatin1String("font-size")) {
                    fontSize = parseSize(match.capturedView(2));
                }
            }

            if (!iconName.isEmpty() || !iconText.isEmpty()) {
                auto format = new TextIconFormat(iconName, iconText, std::move(width), std::move(height),
                                                 std::move(minWidth), std::move(minHeight), std::move(maxWidth),
                                                 std::move(maxHeight), std::move(valign), std::move(fontSize));
                if (iconType == QLatin1String("smiley") && iconName.isEmpty()) {
                    format->setFontFamilies(emojiFontFamilies);
                }
                queue->enqueue(format);
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

                    int end = work.indexOf(QLatin1Char { '>' }, start);
                    Q_ASSERT(end != -1);

                    // take attributes part of the tag
                    auto    fragment = work.mid(start + it.key().length() + 1, end - start - it.key().length() - 1);
                    QString replaceHtml;
                    QTextCharFormat charFormat;

                    std::tie(charFormat, replaceHtml) = it.value()(fragment, insertedAfter);
                    if (replaceHtml.size()) {
                        result += convertIconsToObjectReplacementCharacters(replaceHtml, queue, insertedAfter, parsers);
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
    static QRegularExpression imgRe("<img[^>]+src\\s*=\\s*(\"[^\"]*\"|'[^']*')[^>]*>");
    QString                   replace;
    QRegularExpressionMatch   match;
    for (int pos = 0; (match = imgRe.match(text, pos)).hasMatch();) {
        replace.clear();
        QString imgSrc    = match.captured(1).mid(1, match.captured(1).size() - 2);
        QUrl    imgSrcUrl = QUrl::fromEncoded(imgSrc.toLatin1());
        if (imgSrcUrl.isValid()) {
            if (imgSrcUrl.scheme() == "data") {
                static QRegularExpression dataRe("^[a-zA-Z]+/[a-zA-Z]+;base64,([a-zA-Z0-9/=+%]+)$");
                auto                      dataMatch = dataRe.match(imgSrcUrl.path());
                if (dataMatch.hasMatch()) {
                    const QByteArray ba = QByteArray::fromBase64(dataMatch.captured(1).toLatin1());
                    if (!ba.isNull()) {
                        QImage image;
                        if (image.loadFromData(ba)) {
                            replace = "srcdata" + QCryptographicHash::hash(ba, QCryptographicHash::Sha1).toHex();
                            doc->addResource(QTextDocument::ImageResource, QUrl(replace), image);
                        }
                    }
                }
            } else if (imgSrc.startsWith(":/") || (!imgSrcUrl.scheme().isEmpty() && imgSrcUrl.scheme() != "file")) {
                pos = match.capturedEnd();
                continue;
            } else {
                // go here when  scheme in ["", "file"] and its not resource
                QString path
                    = QFileInfo(imgSrcUrl.scheme() == "file" ? imgSrcUrl.toLocalFile() : imgSrc).absoluteFilePath();
                bool baseDirFound = false;
                for (const QString &baseDir : std::as_const(allowedImageDirs)) {
                    if (path.startsWith(baseDir)) {
                        baseDirFound = true;
                        break;
                    }
                }
                if (baseDirFound) {
                    if (imgSrcUrl.scheme() == "file") {
                        replace = path;
                    } else {
                        pos = match.capturedEnd();
                        continue;
                    }
                }
            }
        }
        if (replace.isEmpty()) {
            text.remove(match.capturedStart(), match.capturedLength());
        } else {
            text.replace(match.capturedStart(1) + 1, imgSrc.size(), replace);
            pos += replace.size() + 1;
        }
    }

    cursor.insertFragment(QTextDocumentFragment::fromHtml(
        convertIconsToObjectReplacementCharacters(text, &queue, cursor.position(), parsers)));
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
