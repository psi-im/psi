/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ChatMessageBody.h"
#include "ChatMessageText.h"
#include "CodeBlockWidget.h"
#include "psirichtext.h"

#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>

namespace {
// QTextDocument forgets them whenever its content is replaced.
void installImages(ChatMessageText *prose, const QHash<QString, QPixmap> &images)
{
    for (auto it = images.cbegin(); it != images.cend(); ++it)
        prose->document()->addResource(QTextDocument::ImageResource, QUrl(it.key()), it.value());
}
}

ChatMessageBody::ChatMessageBody(QWidget *parent) : QWidget(parent)
{
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(0);
    addProse();
}

ChatMessageText *ChatMessageBody::addProse()
{
    auto *prose = new ChatMessageText(this);
    prose->setForegroundRole(QPalette::Text);
    prose->setQuoteColor(quoteColor_);
    if (!prose_.isEmpty()) {
        // One text to a style sheet, like QTextEdit#mamBody of a theme.
        prose->setObjectName(text()->objectName());
        prose->setPalette(text()->palette());
    }
    connect(prose, &ChatMessageText::plainClicked, this, &ChatMessageBody::plainClicked);
    layout_->addWidget(prose);
    prose_.append(prose);
    filterPart(prose);
    return prose;
}

void ChatMessageBody::clearParts()
{
    while (layout_->count() > 1) {
        auto *item = layout_->takeAt(1);
        delete item->widget();
        delete item;
    }
    prose_ = { text() };
    codeBlocks_.clear();
    text()->show();
}

void ChatMessageBody::setPlainText(const QString &text)
{
    clearParts();
    this->text()->setPlainMessage(text);
}

void ChatMessageBody::setRichText(const QString &html, const RichTextOptions &options)
{
    clearParts();
    auto *first = text();
    // A part's HTML cannot carry the quote level of a line, only its marker.
    // So keep the markers of a document with code for the parts to style.
    const bool mayBeSplit = options.splitCode && html.contains(QString(3, QChar(0x60)));
    first->setQuoteStylingEnabled(options.quoteStyling && !mayBeSplit);
    first->setRichText(html);
    first->setQuoteStylingEnabled(options.quoteStyling);
    if (options.withoutQuotedLines)
        first->removeQuotedLines();
    installImages(first, options.inlineImages);

    const auto parts = mayBeSplit ? PsiRichText::splitCodeBlocks(first->document(), options.inlineImages.keys())
                                  : QList<PsiRichText::ContentPart>();
    if (parts.isEmpty()) {
        first->markQuotedLines();
        return;
    }
    for (int i = 0; i < parts.size(); ++i) {
        const auto &part = parts.at(i);
        if (part.kind == PsiRichText::ContentPart::Code) {
            auto *code = new CodeBlockWidget(this);
            code->setExecutionProvenance(options.messageEncrypted, options.verifiedOmemo);
            code->setCode(part.text, part.language);
            layout_->addWidget(code);
            codeBlocks_.append(code);
            filterPart(code);
            continue;
        }
        auto *prose = i == 0 ? first : addProse();
        prose->setQuoteStylingEnabled(options.quoteStyling);
        prose->setRichText(part.text);
        installImages(prose, options.inlineImages);
    }
    if (parts.first().kind == PsiRichText::ContentPart::Code) {
        first->setPlainMessage({}); // Its parts have the whole document now.
        first->hide();
    }
}

void ChatMessageBody::setQuoteColor(const QColor &color)
{
    quoteColor_ = color;
    for (auto *prose : std::as_const(prose_))
        prose->setQuoteColor(color);
}

void ChatMessageBody::setPartEventFilter(QObject *filter)
{
    partFilter_ = filter;
    for (auto *prose : std::as_const(prose_))
        filterPart(prose);
    for (auto *code : std::as_const(codeBlocks_))
        filterPart(code);
}

void ChatMessageBody::filterPart(QWidget *part)
{
    if (!partFilter_)
        return;
    part->installEventFilter(partFilter_);
    for (auto *child : part->findChildren<QWidget *>())
        child->installEventFilter(partFilter_);
}

int ChatMessageBody::naturalWidth() const
{
    int width = 0;
    for (const auto *prose : prose_)
        width = qMax(width, prose->naturalWidth());
    for (const auto *code : codeBlocks_)
        width = qMax(width, code->naturalWidth());
    return width;
}

int ChatMessageBody::contentHeight(int width) const
{
    // Code alone has a fixed height: the layout has no height for a width then.
    return hasHeightForWidth() ? heightForWidth(width) : sizeHint().height();
}
