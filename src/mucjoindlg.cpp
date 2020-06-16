/*
 * mucjoindlg.cpp
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

#include "mucjoindlg.h"

#include "accountscombobox.h"
#include "bookmarkmanager.h"
#include "groupchatdlg.h"
#include "iconset.h"
#include "jidutil.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psicontactlist.h"
#include "psiiconset.h"

#include <QMessageBox>
#include <QString>
#include <QStringList>

static const int     nickConflictCode = 409;
static const QString additionalSymbol = "_";
static const int     timeout          = 90000;

enum FavoritesItemRoles { FavoritesJidRole = Qt::UserRole, FavoritesPasswordRole, FavoritesIsBookmark };

MUCJoinDlg::MUCJoinDlg(PsiCon *psi, PsiAccount *pa) : QDialog(nullptr), nickAlreadyCompleted_(false)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint
                   | Qt::CustomizeWindowHint);
    setModal(false);
    ui_.setupUi(this);
#ifndef Q_OS_MAC
    setWindowIcon(IconsetFactory::icon("psi/groupChat").icon());
#endif
    controller_ = psi;
    account_    = nullptr;
    controller_->dialogRegister(this);
    ui_.ck_history->setChecked(true);
    ui_.ck_history->hide();
    joinButton_ = ui_.buttonBox->addButton(tr("&Join"), QDialogButtonBox::AcceptRole);
    joinButton_->setDefault(true);
    timer_ = new QTimer(this);
    timer_->setInterval(timeout);
    connect(timer_, &QTimer::timeout, this, [this]() {
        account_->groupChatLeave(jid_.domain(), jid_.node());
        error(404, tr("No response from server for %1 seconds").arg(timeout / 1000));
    });

    reason_ = MucCustomJoin;

    updateIdentity(pa);

    ui_.cb_ident->setController(controller_);
    ui_.cb_ident->setOnlineOnly(true);
    connect(ui_.cb_ident, SIGNAL(activated(PsiAccount *)), SLOT(updateIdentity(PsiAccount *)));
    ui_.cb_ident->setAccount(pa);

    connect(controller_, SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
    updateIdentityVisibility();

    setWindowTitle(CAP(windowTitle()));

    connect(ui_.lwFavorites, SIGNAL(currentRowChanged(int)), SLOT(favoritesCurrentRowChanged(int)));
    connect(ui_.lwFavorites, SIGNAL(itemDoubleClicked(QListWidgetItem *)),
            SLOT(favoritesItemDoubleClicked(QListWidgetItem *)));
    connect(ui_.lwFavorites, &QListWidget::customContextMenuRequested, this,
            &MUCJoinDlg::lwFavorites_customContextMenuRequested);

    ui_.lwFavorites->setContextMenuPolicy(Qt::CustomContextMenu);
    ui_.lwFavorites->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);

    favoritesCurrentRowChanged(ui_.lwFavorites->currentRow());

    setWidgetsEnabled(true);
    adjustSize();
}

void MUCJoinDlg::lwFavorites_customContextMenuRequested(const QPoint &pos)
{
    QPoint globalPos = ui_.lwFavorites->mapToGlobal(pos);

    auto item = ui_.lwFavorites->itemAt(pos);
    if (!item)
        return;

    QMenu    myMenu;
    QAction *bookmarkAction;
    QAction *destroyAction;
    if (item->data(FavoritesIsBookmark).toBool()) {
        bookmarkAction = new QAction(IconsetFactory::icon(QLatin1String("psi/bookmark_add")).icon(),
                                     tr("Remove from bookmarks"), &myMenu);
        connect(bookmarkAction, &QAction::triggered, this, [this, item](bool) {
            auto jid = item->data(FavoritesJidRole).toString();
            account_->bookmarkManager()->removeConference(XMPP::Jid(jid).bare());
            item->setIcon(IconsetFactory::icon(QLatin1String("psi/history")).icon());
            item->setData(FavoritesIsBookmark, false);
            controller_->recentGCAdd(jid);
        });
    } else {
        bookmarkAction = new QAction(IconsetFactory::icon(QLatin1String("psi/bookmark_remove")).icon(),
                                     tr("Add to bookmarks"), &myMenu);
        connect(bookmarkAction, &QAction::triggered, this, [this, item](bool) {
            auto jid = XMPP::Jid(item->data(FavoritesJidRole).toString());
            account_->bookmarkManager()->addConference(
                { jid.node(), jid.bare(), ConferenceBookmark::Never, jid.resource() });
            item->setIcon(IconsetFactory::icon(QLatin1String("psi/bookmarks")).icon());
            item->setData(FavoritesIsBookmark, true);
        });
    }
    destroyAction = new QAction(IconsetFactory::icon(QLatin1String("psi/remove")).icon(), tr("Remove"), &myMenu);
    connect(destroyAction, &QAction::triggered, this, [this, item](bool) {
        auto jid = item->data(FavoritesJidRole).toString();
        if (item->data(FavoritesIsBookmark).toBool())
            account_->bookmarkManager()->removeConference(XMPP::Jid(jid).bare());
        controller_->recentGCRemove(jid);
        ui_.lwFavorites->takeItem(ui_.lwFavorites->row(item));
        delete item;
    });

    myMenu.addAction(bookmarkAction);
    myMenu.addAction(destroyAction);

    // Show context menu at handling position
    myMenu.exec(globalPos);
}

MUCJoinDlg::~MUCJoinDlg()
{
    if (controller_)
        controller_->dialogUnregister(this);
    if (account_)
        account_->dialogUnregister(this);
}

void MUCJoinDlg::done(int r)
{
    if (ui_.busy->isActive()) {
        // int n = QMessageBox::information(0, tr("Warning"), tr("Are you sure you want to cancel joining groupchat?"),
        // tr("&Yes"), tr("&No")); if(n != 0)
        //    return;
        account_->groupChatLeave(jid_.domain(), jid_.node());
    }
    QDialog::done(r);
}

void MUCJoinDlg::updateIdentity(PsiAccount *pa)
{
    if (account_)
        disconnect(account_, SIGNAL(disconnected()), this, SLOT(pa_disconnected()));

    account_ = pa;
    joinButton_->setEnabled(bool(account_));

    if (!account_) {
        ui_.busy->stop();
        return;
    }

    connect(account_, SIGNAL(disconnected()), this, SLOT(pa_disconnected()));
    updateFavorites();
}

void MUCJoinDlg::updateIdentityVisibility()
{
    bool visible = controller_->contactList()->enabledAccounts().count() > 1;
    ui_.cb_ident->setVisible(visible);
    ui_.lb_identity->setVisible(visible);
}

void MUCJoinDlg::updateFavorites()
{
    QListWidgetItem *recentLwi = nullptr, *lwi;

    ui_.lwFavorites->clear();

    QHash<QString, QListWidgetItem *>     bmMap; // jid to item
    QMultiMap<QString, QListWidgetItem *> nmMap; // name to item
    if (account_ && account_->bookmarkManager()->isAvailable()) {
        for (ConferenceBookmark c : account_->bookmarkManager()->conferences()) {
            if (!c.jid().isValid()) {
                continue;
            }
            QString jidBare(c.jid().bare());
            QString name = c.name();
            if (name.isEmpty()) {
                name = jidBare;
            }
            QString s = QString("%1 (%2)").arg(name, c.nick());
            lwi       = new QListWidgetItem(IconsetFactory::icon(QLatin1String("psi/bookmarks")).icon(), s);
            lwi->setData(FavoritesJidRole, c.jid().withResource(c.nick()).full());
            lwi->setData(FavoritesPasswordRole + 1, c.password());
            lwi->setData(FavoritesIsBookmark, true);
            bmMap.insert(jidBare, lwi);
            nmMap.insert(name.toLower(), lwi);
        }
    }
    for (auto &item : nmMap) { // sorted by key (name)
        ui_.lwFavorites->addItem(item);
    }

    for (const QString &j : controller_->recentGCList()) {
        Jid jid(j);
        if (!jid.isValid()) {
            continue;
        }
        QString bareJid = jid.bare();
        lwi             = bmMap.value(bareJid);
        if (!lwi) {
            QString s = tr("%1 on %2").arg(jid.resource()).arg(JIDUtil::toString(jid, false));
            lwi       = new QListWidgetItem(IconsetFactory::icon(QLatin1String("psi/history")).icon(), s);
            lwi->setData(FavoritesJidRole, j);
            lwi->setData(FavoritesIsBookmark, false);
            ui_.lwFavorites->addItem(lwi);
        }
        if (!recentLwi) {
            recentLwi = lwi;
        }
    }

    ui_.lwFavorites->setVisible(ui_.lwFavorites->count() > 0);
    if (ui_.lwFavorites->count() > 0) {
        ui_.lwFavorites->setFocus();
    } else {
        ui_.le_room->setFocus();
    }

    if (recentLwi && ui_.le_room->text().isEmpty()) {
        ui_.lwFavorites->setCurrentItem(recentLwi);
    }
}

void MUCJoinDlg::pa_disconnected()
{
    if (ui_.busy->isActive()) {
        ui_.busy->stop();
    }
}

void MUCJoinDlg::favoritesCurrentRowChanged(int row)
{
    if (row < 0) {
        return;
    }
    QListWidgetItem *lwi = ui_.lwFavorites->currentItem();
    Jid              jid(lwi->data(FavoritesJidRole).toString());
    QString          password(lwi->data(FavoritesPasswordRole).toString());
    if (!jid.isValid() || jid.node().isEmpty()) {
        return;
    }

    ui_.le_host->setText(jid.domain());
    ui_.le_room->setText(jid.node());
    ui_.le_nick->setText(jid.resource());
    ui_.le_pass->setText(password);
}

void MUCJoinDlg::favoritesItemDoubleClicked(QListWidgetItem *lwi)
{
    Jid     jid(lwi->data(FavoritesJidRole).toString());
    QString password(lwi->data(FavoritesPasswordRole).toString());
    if (!jid.isValid() || jid.node().isEmpty()) {
        return;
    }

    ui_.le_host->setText(jid.domain());
    ui_.le_room->setText(jid.node());
    ui_.le_nick->setText(jid.resource());
    ui_.le_pass->setText(password);
    doJoin();
}

void MUCJoinDlg::doJoin(MucJoinReason r)
{
    if (!account_ || !account_->checkConnected(this))
        return;

    reason_ = r;

    QString host = ui_.le_host->text();
    QString room = ui_.le_room->text();
    QString nick = ui_.le_nick->text();
    QString pass = ui_.le_pass->text();

    if (host.isEmpty() || room.isEmpty() || nick.isEmpty()) {
        QMessageBox::information(this, tr("Error"), tr("You must fill out the fields in order to join."));
        return;
    }

    Jid j = room + '@' + host + '/' + nick;
    if (!j.isValid()) {
        QMessageBox::information(this, tr("Error"), tr("You entered an invalid room name."));
        return;
    }

    GCMainDlg *gc = account_->findDialog<GCMainDlg *>(j.bare());
    if (gc) {
        if (gc->isHidden() && !gc->isTabbed())
            gc->ensureTabbedCorrectly();
        gc->bringToFront();
        if (gc->isInactive()) {
            if (gc->jid() != j.bare())
                gc->setJid(j.bare());
            gc->reactivate();
        }
        joined();
        return;
    }

    if (!account_->groupChatJoin(host, room, nick, pass, !ui_.ck_history->isChecked())) {
        QMessageBox::information(this, tr("Error"), tr("You are in or joining this room already!"));
        return;
    }

    controller_->dialogUnregister(this);
    jid_ = room + '@' + host + '/' + nick;
    account_->dialogRegister(this, jid_);

    setWidgetsEnabled(false);
    timer_->start();
    ui_.busy->start();
}

void MUCJoinDlg::setWidgetsEnabled(bool enabled)
{
    ui_.cb_ident->setEnabled(enabled);
    ui_.lwFavorites->setEnabled(enabled && ui_.lwFavorites->count() > 0);
    ui_.gb_info->setEnabled(enabled);
    joinButton_->setEnabled(enabled);
}

void MUCJoinDlg::joined()
{
    controller_->recentGCAdd(jid_.full());
    timer_->stop();
    ui_.busy->stop();

    nickAlreadyCompleted_ = false;
    closeDialogs(this);
    deleteLater();
    account_->addMucItem(jid_.bare());
}

void MUCJoinDlg::error(int error, const QString &str)
{
    timer_->stop();
    ui_.busy->stop();
    setWidgetsEnabled(true);

    account_->dialogUnregister(this);
    controller_->dialogRegister(this);

    if (!nickAlreadyCompleted_ && reason_ == MucAutoJoin && error == nickConflictCode) {
        nickAlreadyCompleted_ = true;
        ui_.le_nick->setText(ui_.le_nick->text() + additionalSymbol);
        doJoin(reason_);
        return;
    }

    if (!isVisible())
        show();

    nickAlreadyCompleted_ = false;

    QMessageBox *msg = new QMessageBox(QMessageBox::Information, tr("Error"),
                                       tr("Unable to join groupchat.\nReason: %1").arg(str), QMessageBox::Ok, this);
    msg->setAttribute(Qt::WA_DeleteOnClose, true);
    msg->setModal(false);
    msg->show();
}

void MUCJoinDlg::setJid(const Jid &mucJid)
{
    ui_.le_host->setText(mucJid.domain());
    ui_.le_room->setText(mucJid.node());
}

void MUCJoinDlg::setNick(const QString &nick) { ui_.le_nick->setText(nick); }

void MUCJoinDlg::setPassword(const QString &password) { ui_.le_pass->setText(password); }

void MUCJoinDlg::accept() { doJoin(); }
