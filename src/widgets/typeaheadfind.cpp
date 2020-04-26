/*
 * typeaheadfind.cpp - Typeahead find toolbar
 * Copyright (C) 2006  Maciej Niedzielski
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

#include "typeaheadfind.h"

#include "iconaction.h"
#include "psioptions.h"
#include "shortcutmanager.h"
#include "stretchwidget.h"

#include <QAction>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>

/**
 * \class TypeAheadFindBar
 * \brief The TypeAheadFindBar class provides a toolbar for typeahead finding.
 */

class TypeAheadFindBar::Private {
private:
    void updateFoundStyle(bool state)
    {
        if (state) {
            le_find->setStyleSheet("");
        } else {
            le_find->setStyleSheet("QLineEdit { background: #ff6666; color: #ffffff }");
        }
    }

public:
    // setup search and react to search results
    void doFind(bool backward = false)
    {
        QTextDocument::FindFlags options;

        if (caseSensitive)
            options |= QTextDocument::FindCaseSensitively;

        if (backward) {
            options |= QTextDocument::FindBackward;

            if (widgetType == TypeAheadFindBar::Type::WebView) {
                // FIXME make it work with web
            } else {
                // move cursor before currect selection
                // to prevent finding the same occurence again
                QTextCursor cursor = te->textCursor();
                cursor.setPosition(cursor.selectionStart());
                cursor.movePosition(QTextCursor::Left);
                te->setTextCursor(cursor);
            }
        }
        find(text, options);
    }

    // real search code

    void find(const QString &str, QTextDocument::FindFlags options,
              QTextCursor::MoveOperation start = QTextCursor::NoMove)
    {
        if (widgetType == TypeAheadFindBar::Type::WebView) {
#ifdef WEBKIT
#ifdef WEBENGINE
            QWebEnginePage::FindFlags wkOptions;
            wkOptions |= options & QTextDocument::FindBackward ? QWebEnginePage::FindBackward
                                                               : QWebEnginePage::FindFlags(nullptr);
            wkOptions |= options & QTextDocument::FindCaseSensitively ? QWebEnginePage::FindCaseSensitively
                                                                      : QWebEnginePage::FindFlags(nullptr);
            wv->findText(str, wkOptions, [this](bool found) { updateFoundStyle(found); });
#else
            QWebPage::FindFlags wkOptions;
            wkOptions |= options & QTextDocument::FindBackward ? QWebPage::FindBackward : (QWebPage::FindFlags)0;
            wkOptions |= options & QTextDocument::FindCaseSensitively ? QWebPage::FindCaseSensitively
                                                                      : (QWebPage::FindFlags)0;
            updateFoundStyle(wv->findText(str, wkOptions));
#endif
#else
            Q_UNUSED(str);
#endif
            return;
        }

        // If we are here then it's not webkit/engine.

        if (start != QTextCursor::NoMove) {
            QTextCursor cursor = te->textCursor();
            cursor.movePosition(start);
            te->setTextCursor(cursor);
        }

        bool found = te->find(text, options);
        if (!found && start == QTextCursor::NoMove) {
            find(text, options, options & QTextDocument::FindBackward ? QTextCursor::End : QTextCursor::Start);
            return;
        }

        updateFoundStyle(found);
    }

    QString                text;
    bool                   caseSensitive;
    TypeAheadFindBar::Type widgetType;

    QTextEdit *te;
#ifdef WEBKIT
    WebView *wv;
#endif
    QLineEdit *le_find;
    QAction *  act_prev;
    QAction *  act_next;
    QCheckBox *cb_case;
};

/**
 * \brief Creates new find toolbar, hidden by default.
 * \param textedit, QTextEdit that this toolbar will operate on.
 * \param title, toolbar's title
 * \param parent, toolbar's parent
 */
TypeAheadFindBar::TypeAheadFindBar(QTextEdit *textedit, const QString &title, QWidget *parent) : QToolBar(title, parent)
{
    d             = new Private();
    d->widgetType = Type::TextEdit;
    d->te         = textedit;
    init();
}

#ifdef WEBKIT
TypeAheadFindBar::TypeAheadFindBar(WebView *webView, const QString & /*title*/, QWidget * /*parent*/)
{
    d             = new Private();
    d->widgetType = Type::WebView;
    d->wv         = webView;
    init();
}
#endif

void TypeAheadFindBar::init()
{
    setIconSize(QSize(16, 16));
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);

    d->caseSensitive = false;
    d->text          = "";

    d->le_find = new QLineEdit(this);
    d->le_find->setMaximumWidth(200);
    d->le_find->setToolTip(tr("Search"));
    connect(d->le_find, SIGNAL(textEdited(const QString &)), SLOT(textChanged(const QString &)));
    addWidget(d->le_find);

    d->act_prev = new IconAction(tr("Find previous"), "psi/arrowUp", "", 0, this);
    d->act_prev->setEnabled(false);
    d->act_prev->setToolTip(tr("Find previous"));
    connect(d->act_prev, SIGNAL(triggered()), SLOT(findPrevious()));
    addAction(d->act_prev);

    d->act_next = new IconAction(tr("Find next"), "psi/arrowDown", "", 0, this);
    d->act_next->setEnabled(false);
    d->act_next->setToolTip(tr("Find next"));
    connect(d->act_next, SIGNAL(triggered()), SLOT(findNext()));
    addAction(d->act_next);

    d->cb_case = new QCheckBox(tr("&Case sensitive"), this);
    connect(d->cb_case, SIGNAL(stateChanged(int)), SLOT(caseToggled(int)));
    addWidget(d->cb_case);

    addWidget(new StretchWidget(this));

    optionsUpdate();

    hide();
}

/**
 * \brief Destroys the toolbar.
 */
TypeAheadFindBar::~TypeAheadFindBar()
{
    delete d;
    d = nullptr;
}

/**
 * \brief Should be called when application options are changed.
 * This slot updates toolbar's shortcuts.
 */
void TypeAheadFindBar::optionsUpdate()
{
    d->act_prev->setShortcuts(ShortcutManager::instance()->shortcuts("chat.find-prev"));
    d->act_next->setShortcuts(ShortcutManager::instance()->shortcuts("chat.find-next"));
}

/**
 * \brief Opens (shows) the toolbar.
 */
void TypeAheadFindBar::open()
{
    show();
    d->le_find->setFocus();
    d->le_find->selectAll();
    emit visibilityChanged(true);
}

/**
 * \brief Closes (hides) the toolbar.
 */
void TypeAheadFindBar::close()
{
    hide();
    emit visibilityChanged(false);
}

/**
 * \brief Switched between visible and hidden state.
 */
void TypeAheadFindBar::toggleVisibility()
{
    if (isVisible())
        hide();
    else
        // show();
        open();
}

/**
 * \brief Private slot activated when find text chagnes.
 */
void TypeAheadFindBar::textChanged(const QString &str)
{
    QTextCursor cursor;
    if (d->widgetType == Type::TextEdit) {
        cursor = d->te->textCursor();
    }

    if (str.isEmpty()) {
        d->act_prev->setEnabled(false);
        d->act_next->setEnabled(false);
        d->le_find->setStyleSheet("");
        if (d->widgetType == Type::WebView) {
#ifdef WEBKIT
            d->wv->page()->findText(""); // its buggy in qt-4.6.0
#endif
        } else { // TextEditType
            cursor.clearSelection();
            d->te->setTextCursor(cursor);
        }
        d->le_find->setStyleSheet("");
    } else {
        d->act_prev->setEnabled(true);
        d->act_next->setEnabled(true);

        if (d->widgetType == Type::TextEdit) {
            // don't jump to next word occurence after appending new charater
            cursor.setPosition(cursor.selectionStart());
            d->te->setTextCursor(cursor);
        }

        d->text = str;
        d->doFind();
    }
}

/**
 * \brief Private slot activated when find-next is requested.
 */
void TypeAheadFindBar::findNext() { d->doFind(); }

/**
 * \brief Private slot activated when find-prev is requested.
 */
void TypeAheadFindBar::findPrevious() { d->doFind(true); }

/**
 * \brief Private slot activated when case-sensitive box is toggled.
 */
void TypeAheadFindBar::caseToggled(int state) { d->caseSensitive = (state == Qt::Checked); }
