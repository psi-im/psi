/*
 * multifiletransferdlg.cpp - file transfer dialog
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2019  Sergey Ilinykh
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

#include "multifiletransferdlg.h"

#include <QBuffer>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QMimeData>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QPainter>
#include <xmpp_tasks.h>

#include "avatars.h"
#include "fileutil.h"
#include "iconset.h"
#include "jingle-ft.h"
#include "jingle.h"
#include "multifiletransferdelegate.h"
#include "multifiletransferitem.h"
#include "multifiletransfermodel.h"
#include "networkaccessmanager.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psicontact.h"
#include "ui_multifiletransferdlg.h"
#include "userlist.h"
#include "xmpp/jid/jid.h"

using namespace XMPP;

class MultiFileTransferDlg::Private
{
public:
    PsiAccount *account;
    Jid peer;
    QPointer<XMPP::Jingle::Session> session;
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
    ui->buttonBox->button(QDialogButtonBox::Close)->hide();
    connect(ui->listView, &QListView::clicked, this, [this] (const QModelIndex &index) {
        auto state = index.data(MultiFileTransferModel::StateRole).toInt();
        if (state == MultiFileTransferModel::AddTemplate) {
            if (!d->model->isAddEnabled()) {
                return;
            }
            QStringList files_ = FileUtil::getOpenFileNames(this, tr("Open Files"));
            appendOutgoing(files_);
        }
    });

    updateMyVisuals();
}

MultiFileTransferDlg::~MultiFileTransferDlg()
{
    delete ui;
}

static void setMFTItemStateFromJingleState(MultiFileTransferItem *item, Jingle::FileTransfer::Application *app)
{
    QString comment;
    MultiFileTransferModel::State state = MultiFileTransferModel::State::Pending;
    if (app->state() > Jingle::State::Active) {
        // TODO consider lastError and return Failed instead
        state = MultiFileTransferModel::State::Done;
    } else if (app->state() > Jingle::State::Pending) {
        state = MultiFileTransferModel::State::Active;
    }
    switch (app->state()) {
    case Jingle::State::Created:             comment = QObject::tr("Not started"); break;
    case Jingle::State::PrepareLocalOffer:   comment = QObject::tr("Prepare local offer"); break;
    case Jingle::State::Unacked:             comment = QObject::tr("IQ unacknowledged"); break;
    case Jingle::State::Pending:             comment = QObject::tr("Waiting accept"); break;
    case Jingle::State::Accepted:            comment = QObject::tr("Accepted"); break;
    case Jingle::State::Connecting:          comment = QObject::tr("Connecting"); break;
    case Jingle::State::Active:              comment = QObject::tr("Transferring"); break;
    case Jingle::State::Finishing:           break;// put error in comment here if any
    case Jingle::State::Finished:            break;
    };
    item->setState(state, comment);
}

void MultiFileTransferDlg::initOutgoing(const XMPP::Jid &jid, const QStringList &fileList)
{
    d->peer = jid;
    d->isOutgoing = true;
    updatePeerVisuals();
    appendOutgoing(fileList);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setText(tr("Send"));

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this](){
        ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
        d->session = d->account->client()->jingleManager()->newSession(d->peer);
        setupSessionSignals();

        for (int i = 0; i < d->model->rowCount() - 1; ++i) {
            auto index = d->model->index(i, 0, QModelIndex());
            auto item = reinterpret_cast<MultiFileTransferItem*>(index.internalPointer());
            QFileInfo fi(item->filePath());
            if (!fi.isReadable()) {
                delete item;
                continue;
            }
            addTransferContent(item);
        }
        d->session->initiate();
    });
}

void MultiFileTransferDlg::setupCommonSignals(Jingle::FileTransfer::Application *app, MultiFileTransferItem *item)
{
    item->setProperty("jingle", QVariant::fromValue<Jingle::FileTransfer::Application*>(app));
    app->setProperty("mftitem", QVariant::fromValue<MultiFileTransferItem*>(item));
    connect(app, &Jingle::FileTransfer::Application::stateChanged, item, [this, app,item](Jingle::State state){
        if (state == Jingle::State::Accepted) {
            item->setOffset(app->acceptFile().range().offset);
        }
        setMFTItemStateFromJingleState(item, app);
        if (state == Jingle::State::Finished && app->senders() == Jingle::negateOrigin(d->session->role())) {
            // transfer has just finished and we were the receiving side.
            // if it was the last finished transfer xep recommends us to send session.terminate
            connect(item, &MultiFileTransferItem::openDirRequested, this, [item](){
                FileUtil::openFolder(item->filePath());
            });
            bool hasUnfinished = false;
            for (auto &c: d->session->contentList()) {
                if (c->state() != Jingle::State::Finished) {
                    hasUnfinished = true;
                    break;
                }
            }
            if (!hasUnfinished) {
                // ready to terminate
                d->session->terminate(Jingle::Reason::Condition::Success);
            }
        }
    });
    connect(app, &Jingle::FileTransfer::Application::progress, item, &MultiFileTransferItem::setCurrentSize);
}

void MultiFileTransferDlg::initIncoming(XMPP::Jingle::Session *session)
{
    d->session = session;
    setupSessionSignals();
    d->peer = session->peer();
    updatePeerVisuals();
    ui->buttonBox->button(QDialogButtonBox::Apply)->setText(tr("Receive"));
    for (const auto &c: session->contentList()) {
        if (c->creator() == Jingle::Origin::Initiator && c->pad()->ns() == Jingle::FileTransfer::NS) {
            auto app = static_cast<Jingle::FileTransfer::Application*>(c);
            auto file = app->file();
            auto item = d->model->addTransfer(MultiFileTransferModel::Incoming, file.name(), file.size()); // FIXME size is optional. ranges?
            setupCommonSignals(app, item);

            auto thumb = file.thumbnail();
            if (!thumb.uri.isEmpty()) {
                auto loader = new BinaryUriLoader(d->account, d->peer, thumb.uri);
                connect(loader, &BinaryUriLoader::ready, item, [item](const QByteArray &ba){
                    if (ba.isEmpty())
                        return;
                    QPixmap p;
                    p.loadFromData(ba);
                    item->setThumbnail(QIcon(p));
                });
            }
        }
    }
    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::pressed, this, [this](){
        ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
        auto cl = d->session->contentList();
        QList<Jingle::FileTransfer::Application*> appToAccept;
        for (auto it = cl.constBegin(); it != cl.constEnd(); ++it) {
            if (it.key().second == Jingle::Origin::Initiator) {
                appToAccept.append(static_cast<Jingle::FileTransfer::Application*>(it.value()));
            }
        }
        if (appToAccept.size() > 1) {
            auto dirName = FileUtil::getSaveDirName(this, tr("Directory to save files"));
            if (!dirName.isEmpty()) {
                QDir d(dirName);
                for (auto app: appToAccept) {
                    auto fn = d.absoluteFilePath(FileUtil::cleanFileName(app->file().name()));
                    QFileInfo fi(fn);
                    if (fi.dir() != d) { // in case it has .. or something like this
                        fn = d.absoluteFilePath(fi.fileName());
                        fi = QFileInfo(fn);
                    }
                    if (fi.exists()) {
                        // TODO suggest overwrite
                    }
                    auto item = app->property("mftitem").value<MultiFileTransferItem*>();
                    if (item)
                        item->setFileName(fn);
                    connect(app, &Jingle::FileTransfer::Application::deviceRequested, [fn, app](quint64 offset, quint64 size){
                        auto f = new QFile(fn, app);
                        f->open(QIODevice::WriteOnly);
                        f->seek(offset);
                        Q_UNUSED(size);
                        app->setDevice(f);
                    });
                }
            }
        } else if (appToAccept.size()) {
            auto app = appToAccept.first();
            auto fn = FileUtil::getSaveFileName(this, tr("Save As"), FileUtil::cleanFileName(app->file().name()), tr("All files (*)"));
            if (!fn.isEmpty()) {
                QFileInfo fi(fn);
                if (fi.exists()) {
                    // TODO suggest overwrite
                }
                auto item = app->property("mftitem").value<MultiFileTransferItem*>();
                if (item)
                    item->setFileName(fn);
                connect(app, &Jingle::FileTransfer::Application::deviceRequested, [fn, app](quint64 offset, quint64 size){
                    auto f = new QFile(fn, app);
                    f->open(QIODevice::WriteOnly);
                    f->seek(offset);
                    Q_UNUSED(size);
                    app->setDevice(f);
                });
            }
        }
        d->session->accept();
    });

    updateComonVisuals();
}

void MultiFileTransferDlg::reject()
{
    if (d->session && d->session->state() < Jingle::State::Finishing) {
        d->session->terminate(Jingle::Reason::Condition::Cancel);
    }
    QDialog::reject();
}

void MultiFileTransferDlg::accept()
{
    if (d->session && d->session->state() < Jingle::State::Finishing) {
        d->session->terminate(Jingle::Reason::Condition::Success); // really?
    }
    QDialog::accept();
}

void MultiFileTransferDlg::addTransferContent(MultiFileTransferItem *item)
{
    QMimeDatabase mimeDb;
    auto app = static_cast<Jingle::FileTransfer::Application*>(d->session->newContent(Jingle::FileTransfer::NS, d->session->role()));
    if (!app) {
        qWarning("Nothing registered in Jingle for %s", qPrintable(Jingle::FileTransfer::NS));
        return;
    }

    connect(app, &Jingle::FileTransfer::Application::deviceRequested, item, [app,item](quint64 offset, quint64 size){
        auto f = new QFile(item->filePath(), app);
        f->open(QIODevice::ReadOnly);
        f->seek(offset);
        app->setDevice(f);
        Q_UNUSED(size);
    });
    setupCommonSignals(app, item);

    // compute file hash
    XMPP::Hash hash(XMPP::Hash::Sha1); // use Blake2 when we have optimized implementation
    QFile f(item->filePath());
    hash.computeFromDevice(&f); // FIXME it will freeze Psi for awhile on large files

    // take thumbnail
    XMPP::Thumbnail thumb;
    auto icon = item->thumbnail();
    if (!icon.isNull()) {
        QPixmap p = icon.pixmap(icon.availableSizes()[0]);
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        p.save(&buffer, "PNG");
        thumb = XMPP::Thumbnail(ba, "image/png", p.width(), p.height());
    }

    Jingle::FileTransfer::File file;
    QFileInfo fi(item->filePath());
    file.setDate(fi.lastModified());
    file.setDescription(item->description());
    file.addHash(hash);
    file.setMediaType(mimeDb.mimeTypeForFile(fi).name());
    file.setName(fi.fileName());
    file.setRange(); // indicate range support
    file.setSize(fi.size());
    file.setThumbnail(thumb);

    app->setFile(file);
    d->session->addContent(app);
}

void MultiFileTransferDlg::appendOutgoing(const QStringList &fileList)
{
    for (auto const &fname: fileList) {
        QFileInfo fi(fname);
        if (fi.isFile() && fi.isReadable()) {
            auto mftItem = d->model->addTransfer(MultiFileTransferModel::Outgoing, fi.fileName(), fi.size());

            QImage img(fi.filePath());
            if (!img.isNull()) {
                img = img.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QImage back(64,64,QImage::Format_ARGB32_Premultiplied);
                back.fill(Qt::transparent);
                QPainter painter(&back);
                auto imgRect = img.rect();
                imgRect.moveCenter(back.rect().center());
                painter.drawImage(imgRect, img);
                mftItem->setThumbnail(QIcon(QPixmap::fromImage(std::move(back))));
            } else {
                mftItem->setThumbnail(QFileIconProvider().icon(fi));
            }

            mftItem->setFileName(fname);
            if(d->session) {
                addTransferContent(mftItem);
            }
        }
    }
    updateComonVisuals();
}

void MultiFileTransferDlg::setupSessionSignals()
{
    if (!d->session) {
        return;
    }
    connect(d->session.data(), &Jingle::Session::terminated, this, [this](){
        ui->buttonBox->button(QDialogButtonBox::Cancel)->hide();
        ui->buttonBox->button(QDialogButtonBox::Close)->show();
        ui->buttonBox->button(QDialogButtonBox::Apply)->hide();
        d->model->setAddEnabled(false);
    });
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

void MultiFileTransferDlg::dragEnterEvent(QDragEnterEvent *event)
{
    if (!d->model->isAddEnabled()) {
        event->acceptProposedAction();
    }
}

void MultiFileTransferDlg::dragMoveEvent(QDragMoveEvent *event)
{
    if (!d->model->isAddEnabled()) {
        event->acceptProposedAction();
    }
}

void MultiFileTransferDlg::dragLeaveEvent(QDragLeaveEvent *event)
{
    if (!d->model->isAddEnabled()) {
        event->accept();
    }
}

void MultiFileTransferDlg::dropEvent(QDropEvent *event)
{
    if (!d->model->isAddEnabled()) {
        return;
    }
    QStringList dragFiles;
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        foreach(const QUrl & url_, mimeData->urls()) {
            dragFiles << url_.toLocalFile();
        }
    }
    if (!dragFiles.isEmpty()) {
        appendOutgoing(dragFiles);
        event->acceptProposedAction();
    }
}

BinaryUriLoader::BinaryUriLoader(PsiAccount *acc, const Jid &peer, const QUrl &uri)
{
    auto uris = uri.toString();
    if (uris.startsWith(QString::fromLatin1("cid:"))) {
        JT_BitsOfBinary *task = new JT_BitsOfBinary(acc->client()->rootTask());
        connect(task, &JT_BitsOfBinary::finished, this, [this,task](){
            BoBData &bob = ((JT_BitsOfBinary*)sender())->data();
            emit ready(bob.data());
            deleteLater();
        }, Qt::QueuedConnection);
        task->get(peer, uri.toString().mid(4));
        task->go(true);
    } else {
        auto nam = acc->psi()->networkAccessManager();
        QNetworkReply *reply = nam->get(QNetworkRequest(uri));
        connect(reply, &QNetworkReply::finished, this, [this,reply](){
            QNetworkReply* reply = dynamic_cast<QNetworkReply*>(sender());
            ready(reply->readAll());
            reply->deleteLater();
            deleteLater();
        }, Qt::QueuedConnection);
    }
}
