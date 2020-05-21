/*
 * mucjoindlg.h
 * Copyright (C) 2001-2008  Justin Karneges, Michail Pishchagin
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

#ifndef MUCJOINDLG_H
#define MUCJOINDLG_H

#include "psiaccount.h"
#include "ui_mucjoin.h"
#include "xmpp_jid.h"

#include <QDialog>
#include <QTimer>

class PsiCon;
class QString;

class MUCJoinDlg : public QDialog {
    Q_OBJECT

public:
    MUCJoinDlg(PsiCon *, PsiAccount *);
    ~MUCJoinDlg();

    void setJid(const XMPP::Jid &jid);
    void setNick(const QString &nick);
    void setPassword(const QString &password);

    void joined();
    void error(int, const QString &);

public slots:
    void done(int);
    void doJoin(PsiAccount::MucJoinReason reason = PsiAccount::MucCustomJoin);

    // reimplemented
    void accept();

public:
    PsiAccount::MucJoinReason getReason() const { return reason_; }

private slots:
    void updateIdentity(PsiAccount *);
    void updateIdentityVisibility();
    void pa_disconnected();
    void favoritesCurrentRowChanged(int);
    void favoritesItemDoubleClicked(QListWidgetItem *lwi);
    void lwFavorites_customContextMenuRequested(const QPoint &pos);

private:
    Ui::MUCJoin               ui_;
    PsiCon *                  controller_;
    PsiAccount *              account_;
    QPushButton *             joinButton_;
    XMPP::Jid                 jid_;
    PsiAccount::MucJoinReason reason_;
    QTimer *                  timer_;
    bool                      nickAlreadyCompleted_;

    void disableWidgets();
    void enableWidgets();
    void setWidgetsEnabled(bool enabled);
    void updateFavorites();
};

#endif // MUCJOINDLG_H
