/*
 * filesharedlg.cpp - file sharing dialog
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "filesharedlg.h"
#include "ui_filesharedlg.h"
#include "psiaccount.h"
#include "xmpp_message.h"
#include "multifiletransfermodel.h"
#include "multifiletransferitem.h"
#include "xmpp_client.h"
#include "httpfileupload.h"
#include "multifiletransferdelegate.h"
#include "filesharingmanager.h"
#include "filecache.h"
#include "psicon.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>
#include <QPushButton>
#include <QFileIconProvider>
#include <QPainter>

FileShareDlg::FileShareDlg(const QList<FileSharingItem*> &items, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FileShareDlg)
{
    setAttribute(Qt::WA_DeleteOnClose);
    ui->setupUi(this);
    ui->pixmapRatioLabel->hide();
    ui->lv_files->hide();
    filesModel = new MultiFileTransferModel(this);
    filesModel->setAddEnabled(false); // we can reenable it if ever implemented in this dialog
    ui->lv_files->setModel(filesModel);
    ui->lv_files->setItemDelegate(new MultiFileTransferDelegate(this));

    auto shareBtn = ui->buttonBox->button(QDialogButtonBox::Apply);
    shareBtn->setDefault(true);
    shareBtn->setText(tr("Share"));
    connect(shareBtn, &QPushButton::clicked, this, &FileShareDlg::publish);

    for (auto const &pi: items) {
        QFileInfo fi(pi->fileName());
        auto tr = filesModel->addTransfer(MultiFileTransferModel::Outgoing, fi.fileName(), fi.size());
        tr->setThumbnail(pi->thumbnail(QSize(64,64)));
        if (pi->isPublished()) {
            tr->setCurrentSize(fi.size());
            tr->setState(MultiFileTransferModel::Done);
        }
        tr->setProperty("publisher", QVariant::fromValue<FileSharingItem*>(pi));
    }

    QImage preview;
    if (items.count() > 1 || (preview = items[0]->preview(QApplication::desktop()->screenGeometry(this).size() / 2)).isNull()) {
        ui->lv_files->show();
        ui->pixmapRatioLabel->hide();
    } else {
        showImage(preview);
    }
}

void FileShareDlg::showImage(const QImage &img)
{
    auto pix = QPixmap::fromImage(img);

    const auto sg = QApplication::desktop()->screenGeometry(this);
    if (pix.size().boundedTo(sg.size() / 2) != pix.size()) {
        pix = pix.scaled(sg.size() / 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    ui->pixmapRatioLabel->setPixmap(pix);
    QRect r(0, 0, pix.width(), pix.height());
    r.moveCenter(ui->pixmapRatioLabel->geometry().center());
    ui->pixmapRatioLabel->setGeometry(r);
    ui->lv_files->hide();
    ui->pixmapRatioLabel->show();
}

QString FileShareDlg::description() const
{
    return ui->lineEdit->toPlainText();
}

FileShareDlg *FileShareDlg::fromMimeData(const QMimeData *data, PsiAccount *acc, QWidget *parent)
{
    auto items = acc->psi()->fileSharingManager()->fromMimeData(data, acc);
    if (items.isEmpty())
        return nullptr;
    return new FileShareDlg(items, parent);
}

QList<FileSharingItem *> FileShareDlg::takeItems()
{
    auto ret = readyPublishers;
    readyPublishers.clear();
    return ret;
}

void FileShareDlg::publish()
{
    ui->buttonBox->button(QDialogButtonBox::Apply)->setDisabled(true);
    QList<FileSharingItem*> toPublish;
    filesModel->forEachTransfer([this,&toPublish](MultiFileTransferItem *item){
        auto publisher = item->property("publisher").value<FileSharingItem*>();
        if (publisher->isPublished()) {
            item->setState(MultiFileTransferModel::Done);
            item->setCurrentSize(item->fullSize());
            readyPublishers.append(publisher);
            return;
        }
        item->setState(MultiFileTransferModel::Active);
        connect(publisher, &FileSharingItem::publishProgress, this, [this, item](size_t progress){
            item->setCurrentSize(progress);
        });
        connect(publisher, &FileSharingItem::publishFinished, this, [this,publisher,item](){
            if (publisher->uris().count()) {
                item->setState(MultiFileTransferModel::Done);
                item->setCurrentSize(item->fullSize());
            } else {
                item->setState(MultiFileTransferModel::Failed);
                hasFailures = true;
            }
            inProgressCount--;
            readyPublishers.append(publisher);
            if (!inProgressCount)
                emit published();
        });
        inProgressCount++;
        toPublish.append(publisher);
    });
    if (!inProgressCount) {
        emit published();
    }
    for (auto &p: toPublish)
        p->publish();
}

FileShareDlg::~FileShareDlg()
{
    delete ui;
}
