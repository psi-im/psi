/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QColor>
#include <QHash>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QWidget>

class ChatMessageText;
class CodeBlockWidget;
class QVBoxLayout;

// What a message says, in a timeline row or a reply preview: its rich text, with
// every completed XEP-0393 fenced block as a CodeBlockWidget between prose views.
// Whatever is around it (author, frame, clipping) belongs to the owner.
class ChatMessageBody final : public QWidget {
    Q_OBJECT
public:
    struct RichTextOptions {
        bool quoteStyling       = true;  // Show lines starting with '>' as quotes.
        bool withoutQuotedLines = false; // Leave them out instead.
        bool splitCode          = true;  // Use code widgets where the document allows it.
        // By resource name. Any other image keeps the document from being split.
        QHash<QString, QPixmap> inlineImages;
        // Do not infer OMEMO verification from "encrypted" alone.
        bool messageEncrypted = false;
        bool verifiedOmemo    = false;
    };

    explicit ChatMessageBody(QWidget *parent = nullptr);

    // The first prose part, which always exists. A document that cannot be split
    // stays here as a whole; it is empty and hidden while the content starts with code.
    ChatMessageText *text() const { return prose_.first(); }
    // All prose parts in document order, text() first.
    const QList<ChatMessageText *> &prose() const { return prose_; }
    const QList<CodeBlockWidget *> &codeBlocks() const { return codeBlocks_; }

    void setPlainText(const QString &text);
    void setRichText(const QString &html, const RichTextOptions &options);

    void setQuoteColor(const QColor &color);
    // Installed on the widgets of every part, including those of later content.
    void setPartEventFilter(QObject *filter);

    // Width of the content laid out without wrapping, e.g. to fit a bubble.
    int naturalWidth() const;
    int contentHeight(int width) const;

signals:
    // A plain left click on prose that no link, selection or inline object claimed.
    void plainClicked();

private:
    ChatMessageText *addProse();
    void             clearParts();
    void             filterPart(QWidget *part);

    QVBoxLayout             *layout_ = nullptr;
    QList<ChatMessageText *> prose_;
    QList<CodeBlockWidget *> codeBlocks_;
    QColor                   quoteColor_;
    QObject                 *partFilter_ = nullptr;
};
