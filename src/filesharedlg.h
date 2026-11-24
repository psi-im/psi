/*
 * filesharedlg.h - file sharing dialog
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

#ifndef FILESHAREDLG_H
#define FILESHAREDLG_H

#include "iris/jid/jid.h"
#include "iris/xmpp_reference.h"

#include <QDialog>

class FileSharingItem;
class MultiFileTransferModel;
class PsiAccount;
class QMimeData;

namespace Ui {
class FileShareDlg;
}

namespace XMPP {
class Message;
}

class FileShareDlg : public QDialog {
    Q_OBJECT

public:
    // The receiving chat must send the callback's encrypted=true result
    // inside an end-to-end encrypted XMPP stanza. Never leak aesgcm: keys in
    // plaintext messages, logs or an ordinary OOB URL.
    using Callback = std::function<void(const QList<XMPP::Reference> &&, const QString &, bool encrypted)>;
    struct EncryptionOptions {
        bool available;
        bool selected;
        constexpr EncryptionOptions(bool available = false, bool selected = false) :
            available(available), selected(selected) { }
    };

    explicit FileShareDlg(PsiAccount *acc, const XMPP::Jid &myJid, const QList<FileSharingItem *> &items,
                          const Callback &callback, QWidget *parent = nullptr,
                          EncryptionOptions encryption = {});
    ~FileShareDlg();

    static void shareFiles(PsiAccount *acc, const XMPP::Jid &myJid, const Callback &callback, QWidget *parent,
                           EncryptionOptions encryption = {});
    static void shareFiles(PsiAccount *acc, const XMPP::Jid &myJid, const QMimeData *data, const Callback &callback,
                           QWidget *parent, EncryptionOptions encryption = {});
private slots:
    void publish();

private:
    void showImage(const QImage &img);
    void finish();

    Ui::FileShareDlg        *ui;
    PsiAccount              *account;
    XMPP::Jid                myJid;
    QImage                   image;
    MultiFileTransferModel  *filesModel;
    QList<FileSharingItem *> readyPublishers;
    QList<XMPP::Reference> readyEncryptedReferences;
    EncryptionOptions encryptionOptions_;
    bool encryptedUpload_ = false;
    Callback                 publishedCallback;
    int                      inProgressCount = 0;
    bool                     hasFailures     = false;
};

#endif // FILESHAREDLG_H
