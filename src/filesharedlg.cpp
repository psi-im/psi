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

#include "aesgcmurl.h"

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
#include <QFile>
#include <QCheckBox>
#include <QUuid>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QUrl>


// Keep the plaintext strictly local. For encrypted publications we bypass
// FileSharingItem::publish(): it can expose an ordinary, unencrypted Jingle
// source in addition to HTTP. Only this one ciphertext HTTP source is shared.
class EncryptedHttpPublication final : public QObject {
public:
    using Completion = std::function<void(const XMPP::Reference &, const QString &)>;
    EncryptedHttpPublication(XMPP::Client *client, FileSharingItem *item, QObject *parent) :
        QObject(parent), client_(client), item_(item), plaintext_(item ? item->localFilePath() : QString()),
        encrypted_(&plaintext_, XMPP::StatelessFileSharing::Cipher::Aes256Gcm)
    {
    }

    ~EncryptedHttpPublication() override
    {
        // Cancel the uploader before its QIODevice source is destroyed.
        if (uploader_)
            delete uploader_.data();
    }

    void start(std::function<void(qint64)> progress, Completion completed)
    {
        using namespace XMPP::StatelessFileSharing;
        auto fail = [&](const QString &message) {
            completed({}, message);
        };
        if (!client_ || !item_) {
            fail(tr("File or account was closed"));
            return;
        }
        const auto size = item_->fileSize();
        if (!size || !cipherSupported(Cipher::Aes256Gcm)) {
            fail(tr("AES-256-GCM is unavailable or the file size is unknown"));
            return;
        }
        const auto wireSize = encryptedSize(Cipher::Aes256Gcm, *size);
        if (!wireSize || !plaintext_.open(QIODevice::ReadOnly) || !encrypted_.open(QIODevice::ReadOnly)) {
            fail(tr("Cannot open the file for encrypted upload"));
            return;
        }
        // Deliberately do not advertise the original name or media type to
        // the HTTP upload service. The original metadata is in OMEMO instead.
        const auto opaqueName = QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".bin");
        uploader_ = client_->httpFileUploadManager()->upload(
            &encrypted_, *wireSize, opaqueName, QStringLiteral("application/octet-stream"));
        if (!uploader_) {
            fail(tr("HTTP upload service is unavailable"));
            return;
        }
        connect(uploader_, &XMPP::HttpFileUpload::progress, this,
                [progress](qint64 bytes, qint64) { progress(bytes); });
        connect(uploader_, &XMPP::HttpFileUpload::finished, this,
                [this, completed = std::move(completed)]() {
                    if (!uploader_->success() || !encrypted_.finished()) {
                        completed({}, uploader_->statusString().isEmpty()
                                          ? tr("Encrypted HTTP upload did not complete")
                                          : uploader_->statusString());
                        uploader_->deleteLater();
                        uploader_.clear();
                        return;
                    }
                    QUrl url(uploader_->getHttpSlot().get.url);
                    // aesgcm: URLs are resolved as HTTPS by AesGcmUrl::parse().
                    // An HTTP-only service cannot carry an interoperable link.
                    if (!url.isValid() || url.scheme() != QLatin1String("https")
                        || url.host().isEmpty() || !item_) {
                        completed({}, tr("Encrypted sharing requires an HTTPS download URL"));
                        uploader_->deleteLater();
                        uploader_.clear();
                        return;
                    }
                    url.setScheme(QStringLiteral("aesgcm"));
                    url.setFragment(QString::fromLatin1((encrypted_.iv() + encrypted_.key()).toHex()));
                    if (!AesGcmUrl::parse(url)) {
                        completed({}, tr("The encrypted file URL is invalid"));
                        uploader_->deleteLater();
                        uploader_.clear();
                        return;
                    }
                    const QString source = url.toString(QUrl::FullyEncoded);
                    XMPP::Reference reference(XMPP::Reference::Data, source);
                    XMPP::MediaSharing sharing;
                    sharing.file = item_->fileMetadata(); // original metadata, only inside OMEMO
                    sharing.sources = { source }; // never include plaintext HTTP or Jingle
                    reference.setMediaSharing(sharing);
                    completed(reference, {});
                    uploader_->deleteLater();
                    uploader_.clear();
                });
    }

private:
    QPointer<XMPP::Client> client_;
    QPointer<FileSharingItem> item_;
    QFile plaintext_;
    XMPP::StatelessFileSharing::EncryptingDevice encrypted_;
    QPointer<XMPP::HttpFileUpload> uploader_;
};

FileShareDlg::FileShareDlg(PsiAccount *acc, const XMPP::Jid &myJid, const QList<FileSharingItem *> &items,
                           const Callback &callback, QWidget *parent, EncryptionOptions encryption) :
    QDialog(parent), ui(new Ui::FileShareDlg), account(acc), myJid(myJid), publishedCallback(callback),
    encryptionOptions_(encryption)
{
    setAttribute(Qt::WA_DeleteOnClose);
    ui->setupUi(this);
    ui->cb_encrypt->setVisible(encryptionOptions_.available
                               && XMPP::StatelessFileSharing::cipherSupported(
                                   XMPP::StatelessFileSharing::Cipher::Aes256Gcm));
    ui->cb_encrypt->setChecked(ui->cb_encrypt->isVisible() && encryptionOptions_.selected);
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
        if (pi->isPublished()) {
            tr->setCurrentSize(quint64(fi.size()));
            tr->setState(MultiFileTransferModel::Done);
        }
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
    encryptedUpload_ = ui->cb_encrypt->isVisible() && ui->cb_encrypt->isChecked();
    ui->buttonBox->button(QDialogButtonBox::Apply)->setDisabled(true);
    ui->cb_encrypt->setDisabled(true); // Retry must use the same transfer security mode.
    QList<FileSharingItem *> toPublish;
    filesModel->forEachTransfer([this, &toPublish](MultiFileTransferItem *item) {
        auto publisher = item->property("publisher").value<FileSharingItem *>();
        if (item->property("shared").toBool())
            return;
        item->setState(MultiFileTransferModel::Active);
        if (encryptedUpload_) {
            ++inProgressCount;
            auto *job = new EncryptedHttpPublication(account->client(), publisher, this);
            job->start(
                [item](qint64 bytes) {
                    if (item->fullSize())
                        item->setCurrentSize(qMin<quint64>(quint64(qMax<qint64>(0, bytes)), *item->fullSize()));
                },
                [this, item, job](const XMPP::Reference &ref, const QString &error) {
                    if (ref.isValid() && ref.mediaSharing().isValid()) {
                        item->setProperty("shared", true);
                        readyEncryptedReferences.append(ref);
                        item->setState(MultiFileTransferModel::Done);
                        if (item->fullSize())
                            item->setCurrentSize(*item->fullSize());
                    } else {
                        item->setState(MultiFileTransferModel::Failed);
                        item->setInfo(TextUtil::plain2rich(error));
                        hasFailures = true;
                    }
                    job->deleteLater();
                    --inProgressCount;
                    if (!inProgressCount)
                        finish();
                });
            return;
        }
        QObject::disconnect(publisher, nullptr, this, nullptr);
        connect(publisher, &FileSharingItem::publishProgress, this,
                [item](size_t progress) { item->setCurrentSize(progress); });
        connect(publisher, &FileSharingItem::publishFinished, this, [this, publisher, item]() {
            if (publisher->isPublished()) {
                item->setProperty("shared", true);
                readyPublishers.append(publisher);
                item->setState(MultiFileTransferModel::Done);
                if (item->fullSize()) {
                    item->setCurrentSize(*item->fullSize());
                }
            } else {
                item->setState(MultiFileTransferModel::Failed);
                hasFailures = true;
            }
            inProgressCount--;
            if (!inProgressCount)
                finish();
        });
        connect(publisher, &FileSharingItem::logChanged, this,
                [publisher, item]() { item->setInfo(TextUtil::plain2rich(publisher->log().join('\n'))); });
        inProgressCount++;
        toPublish.append(publisher);
    });
    if (!inProgressCount) {
        finish();
    }
    for (auto &p : toPublish) {
        if (!p->mimeType().startsWith(QLatin1String("video/")) || p->localFilePath().isEmpty()) {
            p->publish(myJid);
            continue;
        }
        const QPointer<FileSharingItem> item(p);
        // No nested event loop or UI-thread video decoding. The source can be
        // published even when a codec is missing or poster extraction times out.
        VideoThumbnailGenerator::start(p->localFilePath(), this, [item, jid = myJid](QByteArray jpeg) {
            if (!item)
                return;
            item->setVideoThumbnailJpeg(std::move(jpeg));
            item->publish(jid);
        });
    }
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
    for (const auto *item : std::as_const(readyPublishers)) {
        auto ref = item->toReference();
        const QUrl uri = item->simpleSource();
        const QString text = uri.isValid()
            ? uri.toString(QUrl::FullyEncoded)
            : QStringLiteral("SIMS(%1, %2)")
                  .arg(item->mimeType(), tr("requires compliant client"));
        appendReference(ref, text);
    }
    for (const auto &ref : std::as_const(readyEncryptedReferences))
        appendReference(ref, ref.uri());
    readyPublishers.clear();
    readyEncryptedReferences.clear();
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
