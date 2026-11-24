/*
 * filesharedlg.cpp - file sharing dialog
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

#include "filesharedlg.h"


#include "filesharingmanager.h"
#include "fileutil.h"
#include "iris/httpfileupload.h"
#include "iris/xmpp_client.h"
#include "iris/xmpp_message.h"
#include "iris/xmpp_file-sharing.h"
#include "multifiletransferdelegate.h"
#include "multifiletransferitem.h"
#include "multifiletransfermodel.h"
#include "psiaccount.h"
#include "psicon.h"
#include "textutil.h"
#include "ui_filesharedlg.h"
#include "videothumbnailgenerator.h"

#include <QApplication>
#include <QCheckBox>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QUrl>


FileShareDlg::FileShareDlg(PsiAccount *acc, const XMPP::Jid &myJid, const QList<FileSharingItem *> &items,
                           const Callback &callback, QWidget *parent, EncryptionOptions encryption) :
    QDialog(parent), ui(new Ui::FileShareDlg), account(acc), myJid(myJid), publishedCallback(callback),
    encryptionOptions_(encryption)
{
    setAttribute(Qt::WA_DeleteOnClose);
    ui->setupUi(this);
    // Missing cipher support must not hide a selected protection requirement
    // and accidentally route the file through the plaintext publisher.
    ui->cb_encrypt->setVisible(encryptionOptions_.available || encryptionOptions_.selected);
    ui->cb_encrypt->setChecked(encryptionOptions_.selected);
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

    for (auto const &pi : items) {
        QFileInfo fi(pi->fileName());
        auto      tr = filesModel->addTransfer(MultiFileTransferModel::Outgoing, fi.fileName(), quint64(fi.size()));
        tr->setThumbnail(pi->thumbnail(QSize(64, 64)));
        tr->setProperty("publisher", QVariant::fromValue<FileSharingItem *>(pi));
    }

    QImage preview;
    if (items.count() > 1 || (preview = items[0]->preview(this->screen()->geometry().size() / 2)).isNull()) {
        ui->lv_files->show();
        ui->pixmapRatioLabel->hide();
    } else {
        showImage(preview);
    }
}

void FileShareDlg::showImage(const QImage &img)
{
    auto pix = QPixmap::fromImage(img);

    const auto sg = this->screen()->geometry();
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

void FileShareDlg::publish()
{
    // The checkbox requires protection; clearing it is not a request for plaintext.
    // HTTP chooses its key delivery from the chat context at publication time.
    // Protected publications cannot use legacy unscoped Jingle sources yet.
    encryptedUpload_ = encryptionOptions_.selected
        || (encryptionOptions_.available && ui->cb_encrypt->isChecked());
    ui->buttonBox->button(QDialogButtonBox::Apply)->setDisabled(true);
    ui->cb_encrypt->setDisabled(true); // Retry must use the same transfer security mode.
    QList<QPair<QPointer<FileSharingItem>, QPointer<MultiFileTransferItem>>> pending;
    filesModel->forEachTransfer([this, &pending](MultiFileTransferItem *item) {
        auto *publisher = item->property("publisher").value<FileSharingItem *>();
        if (item->property("shared").toBool())
            return;
        item->setState(MultiFileTransferModel::Active);
        ++inProgressCount;
        pending.append({ publisher, item });
        QObject::disconnect(publisher, nullptr, this, nullptr);
        connect(publisher, &FileSharingItem::logChanged, this,
                [publisher, item = QPointer<MultiFileTransferItem>(item)]() {
                    if (item)
                        item->setInfo(TextUtil::plain2rich(publisher->log().join('\n')));
                });
    });

    const auto mode = encryptedUpload_ ? FileSharingItem::PublicationMode::Aes256Gcm
                                      : FileSharingItem::PublicationMode::Plain;
    const QPointer<FileShareDlg> guard(this);
    // Count all attempts before any synchronous completion is possible.
    for (const auto &entry : std::as_const(pending)) {
        if (!guard)
            return;
        const auto publisher = entry.first;
        const auto item = entry.second;
        auto done = [this, item](const XMPP::Reference &reference, const QString &error) {
            const bool success = reference.isValid() && reference.mediaSharing().isValid();
            if (success)
                readyReferences.append(reference);
            else
                hasFailures = true;
            if (item) {
                item->setState(success ? MultiFileTransferModel::Done : MultiFileTransferModel::Failed);
                if (success) {
                    item->setProperty("shared", true);
                    if (item->fullSize())
                        item->setCurrentSize(*item->fullSize());
                } else {
                    item->setInfo(TextUtil::plain2rich(error));
                }
            }
            if (!--inProgressCount)
                finish();
        };
        auto start = [guard, publisher, item, mode, jid = myJid, done](QByteArray jpeg = {}) {
            if (!guard)
                return;
            if (!publisher) {
                done({}, tr("The file is unavailable for sharing"));
                return;
            }
            if (!jpeg.isEmpty())
                publisher->setVideoThumbnailJpeg(std::move(jpeg));
            publisher->publish(jid, mode, guard, done, [item](size_t bytes) {
                if (item && item->fullSize())
                    item->setCurrentSize(qMin<quint64>(bytes, *item->fullSize()));
            });
        };
        // BoB is a plaintext source; encrypted attempts do not publish it.
        if (!encryptedUpload_ && publisher && publisher->mimeType().startsWith(QLatin1String("video/"))
            && !publisher->localFilePath().isEmpty())
            VideoThumbnailGenerator::start(publisher->localFilePath(), this, start);
        else
            start();
    }
    if (pending.isEmpty())
        finish();
}

FileShareDlg::~FileShareDlg() { delete ui; }

void FileShareDlg::finish()
{
    QList<Reference> references;
    QString desc = ui->lineEdit->text();

    const auto appendReference = [this, &references, &desc](Reference r, const QString &text) {
        if (!r.isValid())
            return;
        const QString refText = QStringLiteral(" ") + text;
        r.setRange(desc.size(), desc.size() + refText.size() - 1);
        desc += refText;
        references.append(r);
    };
    for (const auto &ref : std::as_const(readyReferences)) {
        const auto uri = QUrl(ref.uri());
        const auto file = ref.mediaSharing().file;
        const auto scheme = uri.scheme();
        const bool simple = encryptedUpload_ || scheme == QLatin1String("http")
            || scheme == QLatin1String("https") || scheme == QLatin1String("ftp");
        const QString text = simple
            ? ref.uri()
            : QLatin1String("SIMS(") + file.mediaType() + ", "
                + (file.size() ? (QString::number(*file.size()) + "B, ") : "stream ")
                + tr("requires compliant client") + ")";
        appendReference(ref, text);
    }
    readyReferences.clear();
    const QPointer<FileShareDlg> guard(this);
    if (!references.isEmpty())
        publishedCallback(std::move(references), desc, encryptedUpload_);
    if (!guard)
        return;
    if (!hasFailures)
        close();
    else {
        ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        hasFailures = false;
    }
}

void FileShareDlg::shareFiles(PsiAccount *acc, const XMPP::Jid &myJid, const Callback &callback, QWidget *parent,
                             EncryptionOptions encryption)
{
    QStringList files  = FileUtil::getOpenFileNames(parent, QObject::tr("Open Files For Sharing"));
    auto        itList = acc->psi()->fileSharingManager()->fromFilesList(files, acc);
    if (!itList.count())
        return;

    auto dlg = new FileShareDlg(acc, myJid, itList, callback, parent, encryption);
    dlg->show();
}

void FileShareDlg::shareFiles(PsiAccount *acc, const Jid &myJid, const QMimeData *data, const Callback &callback,
                              QWidget *parent, EncryptionOptions encryption)
{
    auto items = acc->psi()->fileSharingManager()->createFromMimeData(data, acc);
    if (items.isEmpty())
        return;
    auto dlg = new FileShareDlg(acc, myJid, items, callback, parent, encryption);
    dlg->show();
}
