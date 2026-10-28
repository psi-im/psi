/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "CodeBlockWidget.h"

#include "OverlayScrollBarManager.h"
#include "psicodelanguage.h"

#include "qsourcehighliter.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QEvent>
#include <QFrame>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QStyle>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCursor>
#include <QStringList>
#include <QTextDocument>
#include <QTextOption>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>

namespace {

constexpr int HorizontalInset = 10;
constexpr int VerticalInset = 7;
constexpr int MaximumVisibleLines = 25;
// Large enough to let BubbleFrame's theme-defined maximum decide the width.
constexpr int MaximumPreferredWidth = 16384;

// A nested code editor must let the enclosing chat log consume wheel events
// when it cannot scroll further in the requested direction.
class CodeEditor final : public QPlainTextEdit {
public:
    explicit CodeEditor(QWidget *parent) : QPlainTextEdit(parent) { }

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        const int delta = event->pixelDelta().y() != 0 ? event->pixelDelta().y() : event->angleDelta().y();
        const QScrollBar *bar = verticalScrollBar();
        if (delta != 0 && (bar->maximum() == bar->minimum()
                           || (delta > 0 && bar->value() == bar->minimum())
                           || (delta < 0 && bar->value() == bar->maximum()))) {
            event->ignore();
            return;
        }
        QPlainTextEdit::wheelEvent(event);
    }
};

}

CodeBlockWidget::CodeBlockWidget(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("mamCodeBlock"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setAutoFillBackground(false);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(HorizontalInset, VerticalInset, HorizontalInset, VerticalInset);
    column->setSpacing(1);

    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("mamCodeBlockHeader"));
    actionsLayout_ = new QHBoxLayout(header);
    actionsLayout_->setContentsMargins(0, 0, 0, 0);
    actionsLayout_->setSpacing(4);

    languageLabel_ = new QLabel(header);
    languageLabel_->setObjectName(QStringLiteral("mamCodeBlockLanguage"));
    languageLabel_->setStyleSheet(QStringLiteral("color: #aaa9a3; background: transparent;"));
    actionsLayout_->addWidget(languageLabel_);
    actionsLayout_->addStretch(1);
    column->addWidget(header);

    editor_ = new CodeEditor(this);
    editor_->setObjectName(QStringLiteral("mamCodeBlockEditor"));
    editor_->setFrameShape(QFrame::NoFrame);
    editor_->setReadOnly(true);
    editor_->setUndoRedoEnabled(false);
    editor_->setWordWrapMode(QTextOption::NoWrap);
    editor_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    editor_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    editor_->document()->setDocumentMargin(0);
    editor_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    editor_->setMinimumWidth(0);
    editor_->setStyleSheet(QStringLiteral(
        "QPlainTextEdit#mamCodeBlockEditor { border: 0; padding: 0; "
        "background: transparent; color: #e3e2d6; "
        "selection-background-color: #535e71; selection-color: #ffffff; }"));
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setStyleHint(QFont::Monospace);
    editor_->setFont(mono);
    editor_->setTabStopDistance(QFontMetricsF(mono).horizontalAdvance(QLatin1Char(' ')) * 4.0);
    column->addWidget(editor_);

    closingFence_ = new QLabel(QString(3, QChar(0x60)), this);
    closingFence_->setObjectName(QStringLiteral("mamCodeBlockClosingFence"));
    closingFence_->setStyleSheet(QStringLiteral("color: #aaa9a3; background: transparent;"));
    closingFence_->setFont(mono);
    column->addWidget(closingFence_);

    auto *copy = new QAction(tr("Copy code"), this);
    copy->setObjectName(QStringLiteral("mamCodeBlockCopy"));
    copy->setToolTip(tr("Copy code"));
    copy->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    connect(copy, &QAction::triggered, this, [this] {
        QApplication::clipboard()->setText(code_);
    });
    copyAction_ = copy;
    addCodeAction(copyAction_);

    // Hide the standard bars without disabling their scroll model. Overlay
    // thumbs do not reserve a gutter or alter the content's size hint.
    Mattermost::OverlayScrollBarManager::setEdgeNavigationButtonsEnabled(*editor_, false);
    Mattermost::OverlayScrollBarManager::attach(*editor_);
    setCode(QString(), QString());
}

CodeBlockWidget::~CodeBlockWidget()
{
    // CodeBlockWidget's members (including actionButtons_) are destroyed
    // before QWidget::~QWidget deletes its QObject children. In particular,
    // the Copy QAction emits destroyed() *after* actionButtons_ is gone.
    // Disconnect our cleanup callbacks while the map and actions are alive.
    // Plugin-owned actions may outlive this widget, so do not delete them.
    for (QAction *action : actionButtons_.keys())
        disconnect(action, &QObject::destroyed, this, nullptr);
}

void CodeBlockWidget::addCodeAction(QAction *action)
{
    if (!action || actionButtons_.contains(action))
        return;
    auto *button = new QToolButton(this);
    button->setObjectName(QStringLiteral("mamCodeBlockAction"));
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    button->setDefaultAction(action);
    button->setToolButtonStyle(action->icon().isNull() ? Qt::ToolButtonTextOnly : Qt::ToolButtonIconOnly);
    button->setStyleSheet(QStringLiteral(
        "QToolButton { color: #d6d6d1; border: 0; padding: 2px 5px; background: transparent; }"
        "QToolButton:hover { background: #484a42; border-radius: 4px; }"));
    actionsLayout_->addWidget(button);
    actionButtons_.insert(action, button);
    connect(action, &QObject::destroyed, this, [this, action] { removeCodeAction(action); });
    updateGeometry();
}

void CodeBlockWidget::removeCodeAction(QAction *action)
{
    auto *button = actionButtons_.take(action);
    if (!button)
        return;
    actionsLayout_->removeWidget(button);
    button->deleteLater();
    updateGeometry();
}

void CodeBlockWidget::updateLanguageLabel()
{
    // XEP-0393 keeps the literal opening and closing styling directives
    // visible. The source itself remains selectable independently.
    const QString displayed = language_.size() > 48 ? language_.left(47) + QChar(0x2026) : language_;
    languageLabel_->setText(QString(3, QChar(0x60)) + displayed);
    languageLabel_->setFont(editor_->font());
}

void CodeBlockWidget::setCode(const QString &code, const QString &language)
{
    code_ = code;
    language_ = language.trimmed();
    delete highlighter_;
    highlighter_ = nullptr;
    editor_->setPlainText(code_);
    if (const auto lang = PsiCodeLanguage::fromName(language_)) {
        auto *highlighter = new QSourceHighlite::QSourceHighliter(
            editor_->document(), QSourceHighlite::QSourceHighliter::Monokai);
        highlighter->setCurrentLanguage(*lang);
        highlighter->rehighlight();
        highlighter_ = highlighter;
    }
    editor_->moveCursor(QTextCursor::Start);
    editor_->horizontalScrollBar()->setValue(0);
    editor_->verticalScrollBar()->setValue(0);
    updateLanguageLabel();

    int widest = 0;
    const QFontMetrics metrics(editor_->font());
    for (QTextBlock block = editor_->document()->begin(); block.isValid(); block = block.next()) {
        const QString line = block.text();
        const int advance = metrics.horizontalAdvance(QString(line.left(2048)).replace(QLatin1Char('\t'), QStringLiteral("    ")));
        widest = std::max(widest, advance);
        if (widest >= MaximumPreferredWidth)
            break;
    }
    const int toolbarWidth = actionsLayout_->sizeHint().width() + 2 * HorizontalInset;
    preferredWidth_ = std::min(MaximumPreferredWidth,
                               std::max({ 140, widest + 2 * HorizontalInset + 12, toolbarWidth }));
    updateHeight();
    updateGeometry();
}

void CodeBlockWidget::updateHeight()
{
    const int visibleLines = std::min(MaximumVisibleLines, std::max(1, editor_->document()->blockCount()));
    editorHeight_ = visibleLines * QFontMetrics(editor_->font()).lineSpacing() + 3;
    editor_->setFixedHeight(editorHeight_);
    updateGeometry();
}

int CodeBlockWidget::naturalWidth() const
{
    // A plugin can add actions after setCode(). Grow the natural width for
    // their buttons without letting any toolbar force an unbounded bubble.
    return std::min(MaximumPreferredWidth,
                    std::max(preferredWidth_, actionsLayout_->sizeHint().width() + 2 * HorizontalInset));
}

QSize CodeBlockWidget::sizeHint() const
{
    const QMargins margins = layout()->contentsMargins();
    return QSize(naturalWidth(),
                 margins.top() + margins.bottom() + actionsLayout_->sizeHint().height()
                 + layout()->spacing() * 2 + editorHeight_ + closingFence_->sizeHint().height());
}

QSize CodeBlockWidget::minimumSizeHint() const
{
    const QSize preferred = sizeHint();
    return QSize(std::min(100, preferred.width()), preferred.height());
}

void CodeBlockWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        updateLanguageLabel();
        updateHeight();
    }
}

void CodeBlockWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(87, 89, 81), 1));
    painter.setBrush(QColor(39, 40, 34));
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 7, 7);
}
