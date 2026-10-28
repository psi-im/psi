/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QHash>
#include <QWidget>

class QAction;
class QEvent;
class QLabel;
class QPlainTextEdit;
class QSyntaxHighlighter;
class QToolButton;
class QHBoxLayout;

// One fenced XEP-0393 block. A single editor owns selection and both scroll
// models; neither scrollbar takes space from the bubble's width/height hint.
class CodeBlockWidget final : public QWidget {
    Q_OBJECT
public:
    explicit CodeBlockWidget(QWidget *parent = nullptr);
    ~CodeBlockWidget() override;

    void setCode(const QString &code, const QString &language = QString());
    QString code() const { return code_; }
    QString language() const { return language_; }

    // Plugins can supply their own QAction (icon, tooltip, enabled state,
    // triggered handler). The widget never takes ownership of these actions.
    // No changes to the rendering/layout API are needed for new commands.
    void addCodeAction(QAction *action);
    void removeCodeAction(QAction *action);
    QAction *copyAction() const { return copyAction_; }
    QPlainTextEdit *editor() const { return editor_; }

    int naturalWidth() const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void updateHeight();
    void updateLanguageLabel();

    QPlainTextEdit *editor_ = nullptr;
    QLabel *languageLabel_ = nullptr;
    QLabel *closingFence_ = nullptr;
    QHBoxLayout *actionsLayout_ = nullptr;
    QAction *copyAction_ = nullptr;
    QSyntaxHighlighter *highlighter_ = nullptr;
    QHash<QAction *, QToolButton *> actionButtons_;
    QString code_;
    QString language_;
    int preferredWidth_ = 140;
    int editorHeight_ = 0;
};
