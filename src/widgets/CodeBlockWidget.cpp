/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "CodeBlockWidget.h"

#include "OverlayScrollBarManager.h"
#include "psicodelanguage.h"

#include "qsourcehighliter.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>

namespace {
constexpr int HorizontalInset = 10;
constexpr int VerticalInset = 7;
constexpr int HeaderHeight = 27;
constexpr int ActionGap = 6;
constexpr int MaximumVisibleLines = 25;
// The bubble's theme-specific maximum width, not this hard limit, normally
// clamps wide source and enables horizontal scrolling.
constexpr int MaximumPreferredWidth = 16384;
constexpr int MaximumFormatInput = 2 * 1024 * 1024;

QIcon copyIcon()
{
    // An always-available high-contrast vector symbol. Theme icons can be
    // missing, or dark gray on a nearly-black Monokai code surface.
    QPixmap pixmap(20, 20);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(245, 248, 242), 1.7));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(6.5, 5.5, 10, 12), 1.5, 1.5);
    painter.drawLine(QPointF(4, 13), QPointF(3, 13));
    painter.drawLine(QPointF(3, 13), QPointF(3, 3));
    painter.drawLine(QPointF(3, 3), QPointF(12, 3));
    return QIcon(pixmap);
}

// A nested code editor lets the outer timeline take over wheel scrolling when
// there is no more content in that direction.
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

QString prettyJson(const QString &source)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(source.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || doc.isNull())
        return {};
    QString formatted = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    // Qt's Indented format uses four spaces per nesting level. Use two for
    // compact reading inside bubbles, without changing JSON string values.
    QStringList lines = formatted.split(QLatin1Char('\n'));
    for (QString &line : lines) {
        int count = 0;
        while (count < line.size() && line.at(count) == QLatin1Char(' '))
            ++count;
        if (count && count % 4 == 0)
            line.replace(0, count, QString(count / 2, QLatin1Char(' ')));
    }
    formatted = lines.join(QLatin1Char('\n'));
    if (formatted.endsWith(QLatin1Char('\n')))
        formatted.chop(1);
    return formatted;
}

QString prettyXml(const QString &source)
{
    // Only use Qt's streaming parser. Never expand external resources, and
    // refuse DTD declarations in untrusted message content.
    QXmlStreamReader reader(source);
    QString output;
    QXmlStreamWriter writer(&output);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(2);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.tokenType() == QXmlStreamReader::DTD)
            return {};
        if (reader.hasError())
            return {};
        writer.writeCurrentToken(reader);
    }
    if (reader.hasError())
        return {};
    return output.trimmed();
}
}

CodeBlockWidget::CodeBlockWidget(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("mamCodeBlock"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setAutoFillBackground(false);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    // Header has no outer padding: the selector touches the top/right edge
    // of the rounded parent, as in AnyKeep. Buttons sit a little to its left.
    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("mamCodeBlockHeader"));
    header->setFixedHeight(HeaderHeight);
    auto *top = new QHBoxLayout(header);
    top->setContentsMargins(0, 0, 0, 0);
    top->setSpacing(0);
    top->addStretch(1);

    auto *actions = new QWidget(header);
    actionsLayout_ = new QHBoxLayout(actions);
    actionsLayout_->setContentsMargins(0, 0, 0, 0);
    actionsLayout_->setSpacing(4);
    top->addWidget(actions, 0, Qt::AlignVCenter);
    top->addSpacing(ActionGap);

    languageSelector_ = new QToolButton(header);
    languageSelector_->setObjectName(QStringLiteral("mamCodeBlockLanguage"));
    languageSelector_->setToolTip(tr("Choose syntax highlighting language"));
    languageSelector_->setAccessibleName(tr("Code language"));
    languageSelector_->setCursor(Qt::PointingHandCursor);
    languageSelector_->setFocusPolicy(Qt::StrongFocus);
    languageSelector_->setPopupMode(QToolButton::InstantPopup);
    languageSelector_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    languageSelector_->setFixedHeight(HeaderHeight);
    languageSelector_->setMinimumWidth(56);
    languageSelector_->setStyleSheet(QStringLiteral(
        "QToolButton#mamCodeBlockLanguage { background: #494b43; color: #f0f1ec;"
        " border: 0; border-left: 1px solid #74766c;"
        " border-top-right-radius: 7px; border-bottom-left-radius: 5px;"
        " padding: 0px 10px; font-weight: 500; }"
        "QToolButton#mamCodeBlockLanguage:hover { background: #62665b; }"
        "QToolButton#mamCodeBlockLanguage:pressed { background: #71776a; }"));
    top->addWidget(languageSelector_, 0, Qt::AlignRight | Qt::AlignTop);
    column->addWidget(header);

    languageMenu_ = new QMenu(languageSelector_);
    languageMenu_->setObjectName(QStringLiteral("mamCodeLanguageMenu"));
    auto *languageGroup = new QActionGroup(languageMenu_);
    languageGroup->setExclusive(true);
    for (const auto &choice : PsiCodeLanguage::choices()) {
        auto *option = languageMenu_->addAction(QString::fromLatin1(choice.label));
        option->setObjectName(QStringLiteral("mamCodeLanguageOption"));
        option->setData(QString::fromLatin1(choice.id));
        option->setCheckable(true);
        languageGroup->addAction(option);
        connect(option, &QAction::triggered, this, [this, option] {
            setHighlightLanguage(option->data().toString());
        });
    }
    languageSelector_->setMenu(languageMenu_);

    auto *codeArea = new QWidget(this);
    codeArea->setObjectName(QStringLiteral("mamCodeBlockBody"));
    auto *bodyLayout = new QVBoxLayout(codeArea);
    bodyLayout->setContentsMargins(HorizontalInset, 3, HorizontalInset, VerticalInset);
    bodyLayout->setSpacing(0);

    editor_ = new CodeEditor(codeArea);
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
        "QPlainTextEdit#mamCodeBlockEditor { border: 0; padding: 0;"
        " background: transparent; color: #e3e2d6;"
        " selection-background-color: #535e71; selection-color: #ffffff; }"));
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setStyleHint(QFont::Monospace);
    editor_->setFont(mono);
    editor_->setTabStopDistance(QFontMetricsF(mono).horizontalAdvance(QLatin1Char(' ')) * 4.0);
    bodyLayout->addWidget(editor_);
    column->addWidget(codeArea);

    copyAction_ = new QAction(tr("Copy code"), this);
    copyAction_->setObjectName(QStringLiteral("mamCodeBlockCopy"));
    copyAction_->setToolTip(tr("Copy code"));
    copyAction_->setIcon(copyIcon());
    connect(copyAction_, &QAction::triggered, this, [this] {
        QApplication::clipboard()->setText(displayedCode());
    });
    addCodeAction(copyAction_);

    formatAction_ = new QAction(tr("Format"), this);
    formatAction_->setObjectName(QStringLiteral("mamCodeBlockFormat"));
    formatAction_->setToolTip(tr("Format JSON or XML for reading"));
    formatAction_->setCheckable(true);
    connect(formatAction_, &QAction::toggled, this, [this](bool) { updateDisplayedCode(); });
    addCodeAction(formatAction_);

    // Plugins provide the execution backend. Psi shows the control only when
    // a plugin supplies an action; it never executes received text itself.
    runAction_ = new QAction(tr("Run"), this);
    runAction_->setObjectName(QStringLiteral("mamCodeBlockRun"));
    connect(runAction_, &QAction::triggered, this, &CodeBlockWidget::confirmRun);
    addCodeAction(runAction_);
    actionButtons_.value(runAction_)->hide();

    Mattermost::OverlayScrollBarManager::setEdgeNavigationButtonsEnabled(*editor_, false);
    Mattermost::OverlayScrollBarManager::attach(*editor_);
    setCode(QString(), QString());
}

CodeBlockWidget::~CodeBlockWidget()
{
    // QObject children (including owned QActions) emit destroyed() from the
    // QWidget base destructor, after actionButtons_ itself has been destroyed.
    for (QAction *action : actionButtons_.keys())
        disconnect(action, &QObject::destroyed, this, nullptr);
}

void CodeBlockWidget::addCodeAction(QAction *action)
{
    if (!action || actionButtons_.contains(action))
        return;
    auto *button = new QToolButton(this);
    button->setObjectName(QStringLiteral("mamCodeBlockAction"));
    button->setAutoRaise(false);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    button->setDefaultAction(action);
    button->setToolButtonStyle(action->icon().isNull() ? Qt::ToolButtonTextOnly : Qt::ToolButtonIconOnly);
    button->setMinimumHeight(23);
    button->setStyleSheet(QStringLiteral(
        "QToolButton#mamCodeBlockAction { color: #f1f3ed; background: #50554b;"
        " border: 1px solid #787e73; border-radius: 5px; padding: 2px 6px; }"
        "QToolButton#mamCodeBlockAction:hover { background: #656d5e; }"
        "QToolButton#mamCodeBlockAction:pressed { background: #777f6f; }"));
    actionsLayout_->addWidget(button);
    actionButtons_.insert(action, button);
    connect(action, &QObject::destroyed, this, [this, action] { removeCodeAction(action); });
    updateGeometry();
}

void CodeBlockWidget::removeCodeAction(QAction *action)
{
    if (action)
        disconnect(action, &QObject::destroyed, this, nullptr);
    auto *button = actionButtons_.take(action);
    if (!button)
        return;
    actionsLayout_->removeWidget(button);
    button->deleteLater();
    updateGeometry();
}

void CodeBlockWidget::setRunAction(QAction *action)
{
    // The passed action cannot be added directly as a button: its triggered()
    // signal would bypass the mandatory confirmation. Only the guarded proxy
    // is exposed in the header.
    if (runTarget_)
        disconnect(runTarget_, nullptr, runAction_, nullptr);
    runTarget_ = action;
    auto *button = actionButtons_.value(runAction_);
    button->setVisible(action != nullptr);
    runAction_->setEnabled(action && action->isEnabled());
    if (action) {
        connect(action, &QObject::destroyed, runAction_, [proxy = runAction_] {
            proxy->setEnabled(false);
        });
        connect(action, &QAction::changed, runAction_, [proxy = runAction_, guard = QPointer<QAction>(action)] {
            proxy->setEnabled(guard && guard->isEnabled());
        });
    }
    updateGeometry();
}

void CodeBlockWidget::setExecutionProvenance(bool messageEncrypted, bool verifiedOmemo)
{
    messageEncrypted_ = messageEncrypted;
    verifiedOmemo_ = verifiedOmemo;
}

void CodeBlockWidget::confirmRun()
{
    const QPointer<QAction> target = runTarget_;
    if (!target || !target->isEnabled())
        return;
    QMessageBox dialog(QMessageBox::Warning, tr("Run received code?"),
                       tr("Executing code from a message can read, modify or delete local data."
                          " Only continue if you have reviewed the code and trust its source."),
                       QMessageBox::NoButton, this);
    auto *run = dialog.addButton(tr("Run"), QMessageBox::AcceptRole);
    auto *cancel = dialog.addButton(QMessageBox::Cancel);
    dialog.setDefaultButton(cancel);
    dialog.setEscapeButton(cancel);
    if (!(messageEncrypted_ && verifiedOmemo_)) {
        auto *acceptRisk = new QCheckBox(
            tr("I understand and accept the risk of running code from an unverified source."), &dialog);
        dialog.setCheckBox(acceptRisk);
        run->setEnabled(false);
        connect(acceptRisk, &QCheckBox::toggled, run, &QPushButton::setEnabled);
    }
    QPointer<CodeBlockWidget> self(this);
    dialog.exec();
    if (self && target && dialog.clickedButton() == run && run->isEnabled())
        target->trigger();
}

void CodeBlockWidget::updateLanguageSelector()
{
    QString label = language_.isEmpty() ? tr("Plain text") : language_;
    for (QAction *option : languageMenu_->actions()) {
        const QString id = option->data().toString();
        option->setChecked(id == language_);
        if (id == language_ && !id.isEmpty())
            label = option->text();
    }
    if (label.size() > 24)
        label = label.left(23) + QChar(0x2026);
    languageSelector_->setText(label + QChar(0x25be));
    languageSelector_->setToolTip(tr("Syntax highlighting: %1. Click to choose another language.").arg(label));
}

void CodeBlockWidget::applyHighlightLanguage()
{
    delete highlighter_;
    highlighter_ = nullptr;
    if (const auto lang = PsiCodeLanguage::fromName(language_)) {
        auto *highlighter = new QSourceHighlite::QSourceHighliter(
            editor_->document(), QSourceHighlite::QSourceHighliter::Monokai);
        highlighter->setCurrentLanguage(*lang);
        highlighter->rehighlight();
        highlighter_ = highlighter;
    }
}

void CodeBlockWidget::setHighlightLanguage(const QString &language)
{
    const QString next = language.trimmed().toLower();
    if (language_ == next)
        return;
    language_ = next;
    // A different highlighting language also resets a temporary pretty view
    // so the recipient's original source is never silently transformed.
    formatAction_->setChecked(false);
    updateLanguageSelector();
    updateFormatAction();
    applyHighlightLanguage();
    updateGeometry();
}

void CodeBlockWidget::setCode(const QString &code, const QString &language)
{
    code_ = code;
    sourceLanguage_ = language.trimmed().toLower();
    language_ = sourceLanguage_;
    delete highlighter_;
    highlighter_ = nullptr;
    formatAction_->setChecked(false);
    formattedCode_.clear();
    editor_->setPlainText(code_);
    updateLanguageSelector();
    updateFormatAction();
    applyHighlightLanguage();
    editor_->moveCursor(QTextCursor::Start);
    editor_->horizontalScrollBar()->setValue(0);
    editor_->verticalScrollBar()->setValue(0);
    updateHeight();
    updateGeometry();
}

QString CodeBlockWidget::displayedCode() const
{
    return editor_->toPlainText();
}

void CodeBlockWidget::updateFormatAction()
{
    formattedCode_.clear();
    const QString syntax = language_.toLower();
    const bool supported = syntax == QLatin1String("json") || syntax == QLatin1String("xml");
    if (supported && code_.size() <= MaximumFormatInput)
        formattedCode_ = syntax == QLatin1String("json") ? prettyJson(code_) : prettyXml(code_);
    const bool valid = supported && !formattedCode_.isEmpty();
    formatAction_->setEnabled(valid);
    formatAction_->setToolTip(valid ? tr("Format for reading (toggle original)")
                                   : tr("Format available for valid JSON or XML, up to 2 MiB"));
    if (auto *button = actionButtons_.value(formatAction_))
        button->setVisible(supported);
}

void CodeBlockWidget::updateDisplayedCode()
{
    const QString text = formatAction_->isChecked() && !formattedCode_.isEmpty()
        ? formattedCode_ : code_;
    if (editor_->toPlainText() != text) {
        const int vertical = editor_->verticalScrollBar()->value();
        const int horizontal = editor_->horizontalScrollBar()->value();
        editor_->setPlainText(text);
        editor_->verticalScrollBar()->setValue(vertical);
        editor_->horizontalScrollBar()->setValue(horizontal);
        applyHighlightLanguage();
    }
    formatAction_->setText(formatAction_->isChecked() ? tr("Original") : tr("Format"));
    if (auto *button = actionButtons_.value(formatAction_))
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    updateHeight();
    updateGeometry();
}

void CodeBlockWidget::updateHeight()
{
    const int visibleLines = std::min(MaximumVisibleLines, std::max(1, editor_->document()->blockCount()));
    editorHeight_ = visibleLines * QFontMetrics(editor_->font()).lineSpacing() + 3;
    editor_->setFixedHeight(editorHeight_);
    int widest = 0;
    const QFontMetrics metrics(editor_->font());
    for (QTextBlock block = editor_->document()->begin(); block.isValid(); block = block.next()) {
        // Bound measuring cost for hostile multi-megabyte lines. Actual text is
        // never truncated, and Qt's scroll model still sees the full width.
        const QString line = block.text().left(2048);
        const int advance = metrics.horizontalAdvance(QString(line).replace(QLatin1Char('\t'), QStringLiteral("    ")));
        widest = std::max(widest, advance);
        if (widest >= MaximumPreferredWidth)
            break;
    }
    const int toolsWidth = actionsLayout_->sizeHint().width() + languageSelector_->sizeHint().width() + ActionGap;
    preferredWidth_ = std::min(MaximumPreferredWidth,
                               std::max({ 140, widest + HorizontalInset * 2 + 12, toolsWidth }));
    updateGeometry();
}

int CodeBlockWidget::naturalWidth() const
{
    return std::min(MaximumPreferredWidth, std::max(
        preferredWidth_, actionsLayout_->sizeHint().width() + languageSelector_->sizeHint().width() + ActionGap));
}

QSize CodeBlockWidget::sizeHint() const
{
    return QSize(naturalWidth(), HeaderHeight + 3 + editorHeight_ + VerticalInset);
}

QSize CodeBlockWidget::minimumSizeHint() const
{
    const QSize preferred = sizeHint();
    return QSize(std::min(100, preferred.width()), preferred.height());
}

void CodeBlockWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange)
        updateHeight();
}

void CodeBlockWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(87, 89, 81), 1));
    painter.setBrush(QColor(39, 40, 34));
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 7, 7);
}
