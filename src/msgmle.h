/*
 * msgmle.h - subclass of PsiTextView to handle various hotkeys
 * Copyright (C) 2001-2003  Justin Karneges, Michail Pishchagin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef MSGMLE_H
#define MSGMLE_H

#include "xmpp_htmlelement.h"

#include <QTextEdit>
#include <memory>

#define MAX_MESSAGE_HISTORY 50

class AudioRecorder;
class CapitalLettersController;
class ChatEdit;
class HTMLTextController;
class QEvent;
class QImage;
class QKeyEvent;
class QLabel;
class QResizeEvent;
class QTimer;
class QToolButton;
class SpellHighlighter;

class ChatEdit : public QTextEdit {
    void updateBackground();

    Q_OBJECT

public:
    ChatEdit(QWidget *parent);
    ~ChatEdit();

    void setDialog(QWidget *dialog);

    // reimplemented
    QSize  sizeHint() const;
    void   setFont(const QFont &);
    QMenu *createStandardContextMenu(const QPoint &position);

    static bool       checkSpellingGloballyEnabled();
    void              setCheckSpelling(bool);
    XMPP::HTMLElement toHTMLElement();
    bool              isCorrection() { return correction; }
    void              setLastMessageId(const QString &id) { lastId = id; }
    const QString &   lastMessageId() { return lastId; }
    void              resetCorrection()
    {
        correction = false;
        updateBackground();
    }
    CapitalLettersController *capitalizer();
    void                      addSoundRecButton();
    bool                      hasSoundRecButton() const { return bool(recButton_); }
    void                      removeSoundRecButton();

public slots:
    void appendMessageHistory(const QString &text);
    void clearMessageHistory();
    void doHTMLTextMenu();
    void setCssString(const QString &css);
    void insertAsQuote(const QString &text);

protected slots:
    void applySuggestion();
    void addToDictionary();
    void optionsChanged();
    void showHistoryMessageNext();
    void showHistoryMessagePrev();
    void showHistoryMessageFirst();
    void showHistoryMessageLast();
    void changeActPasteAsQuoteState();
    void pasteAsQuote();
    void setRigthMargin();

signals:
    void fileSharingRequested(const QMimeData *);

protected:
    // override the tab/esc behavior
    bool focusNextPrevChild(bool next);
    void keyPressEvent(QKeyEvent *);
    bool event(QEvent *event);
    void contextMenuEvent(QContextMenuEvent *e);
    void showMessageHistory();
    void initActions();
    void setShortcuts();
    void setEditText(const QString &text);
    void insertFromMimeData(const QMimeData *source);
    bool canInsertFromMimeData(const QMimeData *source) const;

private:
    void setOverlayText(int value);

private:
    QWidget *                         dialog_         = nullptr;
    bool                              check_spelling_ = false;
    std::unique_ptr<SpellHighlighter> spellhighlighter_;
    QPoint                            last_click_;
    int                               previous_position_ = 0;
    QStringList                       typedMsgsHistory;
    int                               typedMsgsIndex       = 0;
    QAction *                         act_showMessagePrev  = nullptr;
    QAction *                         act_showMessageNext  = nullptr;
    QAction *                         act_showMessageFirst = nullptr;
    QAction *                         act_showMessageLast  = nullptr;
    QAction *                         act_changeCase       = nullptr;
    QAction *                         actPasteAsQuote_     = nullptr;
    QString                           currentText;
    HTMLTextController *              controller_  = nullptr;
    CapitalLettersController *        capitalizer_ = nullptr;
    bool                              correction   = false;
    QString                           lastId;
    QPalette                          palOriginal;
    QPalette                          palCorrection;
    std::unique_ptr<QLayout>          layout_;
    std::unique_ptr<QToolButton>      recButton_;
    std::unique_ptr<QLabel>           overlay_;
    std::unique_ptr<QTimer>           timer_;
    std::unique_ptr<AudioRecorder>    recorder_;
    int                               timeout_;
};

class LineEdit : public ChatEdit {
    Q_OBJECT
public:
    LineEdit(QWidget *parent);
    ~LineEdit();

    // reimplemented
    QSize minimumSizeHint() const;
    QSize sizeHint() const;

protected:
    // reimplemented
    void resizeEvent(QResizeEvent *);

private slots:
    void recalculateSize();
    void updateScrollBar();
};

#endif // MSGMLE_H
