/*
 * multifiletransferdlg.cpp - file transfer dialog
 * Copyright (C) 2019 Sergey Ilinykh
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "multifiletransferdlg.h"
#include "ui_multifiletransferdlg.h"
#include "xmpp/jid/jid.h"
#include "jingle.h"
#include "jingle-ft.h"
#include "multifiletransferview.h"
#include "multifiletransfermodel.h"
#include "multifiletransferitem.h"
#include "psiaccount.h"
#include "avatars.h"
#include "psicontact.h"
#include "psicon.h"
#include "userlist.h"
#include "iconset.h"

#include <QFileIconProvider>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QBuffer>

using namespace XMPP;

class MultiFileTransferDlg::Private
{
public:
    PsiAccount *account;
    Jid peer;
    XMPP::Jingle::Session *session = nullptr;
    MultiFileTransferModel *model = nullptr;
    bool isOutgoing = false;
};

MultiFileTransferDlg::MultiFileTransferDlg(PsiAccount *acc, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MultiFileTransferDlg),
    d(new Private)
{
    d->account = acc;
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose); // TODO or maybe hide and wait for transfer to be completed?

    // we need square avatar space. to update minimum width to fit height
    QFontMetrics fm(font());
    int minHeight = 3 *(fm.height() + fm.leading()) + ui->ltNames->spacing() * 2;
    ui->lblMyAvatar->setMinimumWidth(minHeight);
    ui->lblPeerAvatar->setMinimumWidth(minHeight);

    ui->lblMyAvatar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->lblMyAvatar, &QLabel::customContextMenuRequested, this, [this](const QPoint &p){
        QList<QPair<Jid, PsiAccount*>> accs;
        for (auto const &a: d->account->psi()->contactList()->enabledAccounts()) {
            if (a->isAvailable()) {
                accs.append({a->selfContact()->jid(), a});
            }
        }
        if (accs.size() < 2) {
            return;
        }
        QMenu m(this);
        for (auto const &j: accs) {
            m.addAction(j.first.full())->setData(QVariant::fromValue(j.second));
        }
        auto act = m.exec(mapToGlobal(p));
        if (act) {
            d->account = act->data().value<PsiAccount*>();
            updateMyVisuals();
        }
    });

    d->model = new MultiFileTransferModel(this);
    ui->listView->setItemDelegate(new MultiFileTransferDelegate(this));
    ui->listView->setModel(d->model);
    ui->buttonBox->button(QDialogButtonBox::Abort)->hide();

    updateMyVisuals();
}

MultiFileTransferDlg::~MultiFileTransferDlg()
{
    delete ui;
}

void MultiFileTransferDlg::initOutgoing(const XMPP::Jid &jid, const QStringList &fileList)
{
    d->peer = jid;
    d->isOutgoing = true;
    updatePeerVisuals();
    for (auto const &fname: fileList) {
        QFileInfo fi(fname);
        if (fi.isFile() && fi.isReadable()) {
            auto mftItem = d->model->addTransfer(MultiFileTransferModel::Outgoing, fi.fileName(), fi.size());
            mftItem->setThumbnail(QFileIconProvider().icon(fi));
            mftItem->setFileName(fname);
        }
    }
    updateComonVisuals();
    ui->buttonBox->button(QDialogButtonBox::Apply)->setText(tr("Send"));

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this](){
        if (d->isOutgoing) {
            d->session = d->account->client()->jingleManager()->newSession(d->peer);
            QMimeDatabase mimeDb;
            QList<Jingle::Application*> appList;

            for (int i = 0; i < d->model->rowCount() - 1; ++i) {
                auto index = d->model->index(i, 0, QModelIndex());
                auto item = reinterpret_cast<MultiFileTransferItem*>(index.internalPointer());
                QFileInfo fi(item->filePath());
                if (!fi.isReadable()) {
                    delete item;
                    continue;
                }

                auto app = static_cast<Jingle::FileTransfer::Application*>(d->session->newContent(Jingle::FileTransfer::NS, d->session->role()));
                // compute file hash
                XMPP::Hash hash(XMPP::Hash::Blake2b512);
                QFile f(item->filePath());
                hash.computeFromDevice(&f); // FIXME it will freeze Psi for awhile on large files

                // take thumbnail
                QImage img(item->filePath());
                XMPP::Thumbnail thumb;
                if (!img.isNull()) {
                    QByteArray ba;
                    QBuffer buffer(&ba);
                    buffer.open(QIODevice::WriteOnly);
                    img = img.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    img.save(&buffer, "PNG");
                    thumb = XMPP::Thumbnail(ba, "image/png", img.width(), img.height());
                }

                Jingle::FileTransfer::File file;
                file.setDate(fi.lastModified());
                file.setDescription(item->description());
                file.setHash(hash);
                file.setMediaType(mimeDb.mimeTypeForFile(fi).name());
                file.setName(fi.fileName());
                file.setRange(); // indicate range support
                file.setSize(fi.size());
                file.setThumbnail(thumb);

                app->setFile(file);
                app->selectNextTransport();
                appList.append(app);
            }

            d->session->initiate(appList);
        }
    });
}

void MultiFileTransferDlg::initIncoming(XMPP::Jingle::Session *session)
{
    d->session = session;
    d->peer = session->peer();
    updatePeerVisuals();
}

void MultiFileTransferDlg::updateMyVisuals()
{
    QPixmap avatar;
    ui->lblMyAvatar->setToolTip(d->account->jid().full());
    avatar = d->account->avatarFactory()->getAvatar(d->account->jid());
    if (avatar.isNull()) {
        avatar = IconsetFactory::iconPixmap("psi/default_avatar");
    }
    ui->lblMyAvatar->setPixmap(avatar);
    ui->lblMyName->setText(d->account->nick());
}

void MultiFileTransferDlg::updatePeerVisuals()
{
    QPixmap avatar;
    if (d->peer.isValid()) {
        ui->lblPeerAvatar->setToolTip(d->peer.full());

        if (d->account->findGCContact(d->peer)) {
            avatar = d->account->avatarFactory()->getMucAvatar(d->peer);
            ui->lblPeerName->setText(d->peer.resource());

        } else {
            avatar = d->account->avatarFactory()->getAvatar(d->peer);
            auto item = d->account->userList()->find(d->peer.withResource(QString()));
            if (item) {
                ui->lblPeerName->setText(item->name());
            } else {
                ui->lblPeerName->setText(d->peer.node());
            }
        }
    } else {
        ui->lblPeerAvatar->setToolTip(tr("Not selected"));
        ui->lblPeerName->setText(tr("Not selected"));
    }

    if (avatar.isNull()) {
        avatar = IconsetFactory::iconPixmap("psi/default_avatar");
    }
    ui->lblPeerAvatar->setPixmap(avatar);
}

void MultiFileTransferDlg::updateComonVisuals()
{
    ui->lblStatus->setText(tr("%1 File(s)").arg(d->model->rowCount() - 1));
}
