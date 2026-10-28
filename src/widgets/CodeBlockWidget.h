/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QHash>
#include <QPointer>
#include <QWidget>

class QAction;
class QEvent;
class QHBoxLayout;
class QMenu;
class QPlainTextEdit;
class QSyntaxHighlighter;
class QToolButton;

// One read-only XEP-0393 fenced block with a bounded, scrollable editor.
// All language/format changes are local presentation choices; the received
// source is immutable and remains available via code().
class CodeBlockWidget final : public QWidget {
    Q_OBJECT
public:
    explicit CodeBlockWidget(QWidget *parent = nullptr);
    ~CodeBlockWidget() override;

    void setCode(const QString &code, const QString &language = QString());
    QString code() const { return code_; }
    QString language() const { return language_; }
    QString sourceLanguage() const { return sourceLanguage_; }
    QString displayedCode() const;
    void setHighlightLanguage(const QString &language);

    // Plugin actions are never owned by this widget. Run actions must use the
    // guarded entry point instead of addCodeAction(): it always confirms first.
    void addCodeAction(QAction *action);
    void removeCodeAction(QAction *action);
    void setRunAction(QAction *action);
    // Do not infer OMEMO verification from "encrypted" alone. Both checks
    // must be true to omit the explicit risk-acceptance checkbox.
    void setExecutionProvenance(bool messageEncrypted, bool verifiedOmemo);
    QAction *copyAction() const { return copyAction_; }
    QAction *formatAction() const { return formatAction_; }
    QAction *runAction() const { return runAction_; }
    QToolButton *languageSelector() const { return languageSelector_; }
    QPlainTextEdit *editor() const { return editor_; }

    int naturalWidth() const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void updateHeight();
    void updateLanguageSelector();
    void applyHighlightLanguage();
    void updateFormatAction();
    void updateDisplayedCode();
    void confirmRun();

    QPlainTextEdit *editor_ = nullptr;
    QToolButton *languageSelector_ = nullptr;
    QMenu *languageMenu_ = nullptr;
    QHBoxLayout *actionsLayout_ = nullptr;
    QAction *copyAction_ = nullptr;
    QAction *formatAction_ = nullptr;
    QAction *runAction_ = nullptr;
    QPointer<QAction> runTarget_;
    QSyntaxHighlighter *highlighter_ = nullptr;
    QHash<QAction *, QToolButton *> actionButtons_;
    QString code_;
    QString language_;
    QString sourceLanguage_;
    QString formattedCode_;
    bool messageEncrypted_ = false;
    bool verifiedOmemo_ = false;
    int preferredWidth_ = 140;
    int editorHeight_ = 0;
};
