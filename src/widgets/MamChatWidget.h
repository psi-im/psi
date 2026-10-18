/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "LongListWidget.h"
#include "messageview.h"

#include <QList>
#include <QSet>

class QFrame;
class QLabel;
class QResizeEvent;

class MamChatWidget final : public Mattermost::LongListWidget {
    Q_OBJECT
public:
    explicit MamChatWidget(QWidget *parent = nullptr);

    // This is the presentation/model seam. No network requests or SQL in the widget.
    void appendMessage(const MessageView &view);
    int prependArchivePage(const QList<QPair<QString, MessageView>> &page);
    void clearMessages();
    void setHistoryLoading(bool loading);
    void setHistoryExhausted(bool exhausted);
    bool historyExhausted() const { return historyExhausted_; }
    int messageCount() const { return items_.size(); }

    // Whole-message selection uses logical model keys, not materialized QWidget pointers.
    int selectedMessageCount() const { return selectedKeys_.size(); }
    void selectMessageRange(int first, int last);
    void setMessageSelected(int index, bool selected);
    void copySelectedMessages();
    void cancelMessageSelection();

signals:
    void olderMessagesRequested();

protected:
    QWidget *createItemWidget(int index) override;
    bool eventFilter(QObject *target, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    int indexOfKey(const QString &key) const;
    void refreshSelection();
    void positionSelectionToolbar();
    QFrame *eventRow(QObject *target) const;

    struct Row {
        QString key;
        MessageView message;
    };
    QList<Row> items_;
    QSet<QString> keys_;
    bool historyLoading_ = false;
    bool historyExhausted_ = false;
    quint64 generatedKey_ = 0;

    QSet<QString> selectedKeys_;
    QString selectionAnchorKey_;
    QString pressedKey_;
    bool mousePressed_ = false;
    bool selectingByDrag_ = false;
    bool swallowMouseRelease_ = false;
    QFrame *selectionToolbar_ = nullptr;
    QLabel *selectionCountLabel_ = nullptr;
};
