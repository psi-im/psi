/*
 * historyimp.cpp
 * Copyright (C) 2011   Aleksey Andreev
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

#include <QDir>
#include <QTimer>
#include <QMessageBox>
#include <QLayout>

#include "historyimp.h"
#include "edbsqlite.h"
#include "edbflatfile.h"
#include "applicationinfo.h"
#include "psicontactlist.h"
#include "psiaccount.h"
#include "psicontact.h"

HistoryImport::HistoryImport(PsiCon *psi) : QObject(),
    psi_(psi),
    srcEdb(NULL),
    dstEdb(NULL),
    hErase(NULL),
    hRead(NULL),
    hWrite(NULL),
    active(false),
    result_(ResultNone),
    recordsCount(0),
    dlg(NULL)
{
}

HistoryImport::~HistoryImport()
{
    clear();
}

bool HistoryImport::isNeeded()
{
    bool res = false;
    EDBSqLite *stor = static_cast<EDBSqLite *>(psi_->edb());
    if (!stor->getStorageParam("import_start").isEmpty()) {
        EDB *src = stor->mirror();
        if (!src)
            src = new EDBFlatFile(psi_);
        //if (sou && sou->eventsCount(QString(), XMPP::Jid()) != 0)
        if (!src->contacts(QString(), EDB::Contact).isEmpty())
            res = true;
        else
            stor->setStorageParam("import_start", QString());
        if (src != stor->mirror())
            delete src;
    }
    return res;
}

void HistoryImport::clear()
{
    if (dstEdb) {
        ((EDBSqLite *)dstEdb)->setInsertingMode(EDBSqLite::Normal);
        ((EDBSqLite *)dstEdb)->setMirror(new EDBFlatFile(psi_));
    }
    if (hErase) {
        delete hErase;
        hErase = NULL;
    }
    if (hRead) {
        delete hRead;
        hRead = NULL;
    }
    if (srcEdb) {
        delete srcEdb;
        srcEdb = NULL;
    }
    if (hWrite) {
        delete hWrite;
        hWrite = NULL;
    }
    if (dlg) {
        delete dlg;
        dlg = NULL;
    }
}

int HistoryImport::exec()
{
    active = true;

    dstEdb = psi_->edb();
    ((EDBSqLite *)dstEdb)->setMirror(NULL);
    ((EDBSqLite *)dstEdb)->setInsertingMode(EDBSqLite::Import);

    dstEdb->setStorageParam("import_start", "yes");

    if (!srcEdb)
        srcEdb = new EDBFlatFile(psi_);

    foreach (const EDB::ContactItem &ci, srcEdb->contacts(QString(), EDB::Contact)) {
        const XMPP::Jid &jid = ci.jid;
        QStringList accIds;
        foreach (PsiAccount *acc, psi_->contactList()->accounts()) {
            foreach (PsiContact *contact, acc->contactList()) {
                if (contact->jid() == jid)
                    accIds.append(acc->id());
            }
        }
        if (accIds.isEmpty()) {
            PsiAccount *pa = psi_->contactList()->defaultAccount();
            if (pa)
                accIds.append(pa->id());
            else
                accIds.append(psi_->contactList()->accounts().first()->id());
        }
        importList.append(ImportItem(accIds, jid));
    }

    if (importList.isEmpty())
        stop(ResultNormal);
    else
        showDialog();

    return result_;
}

void HistoryImport::stop(int reason)
{
    stopTime = QDateTime::currentDateTime();
    result_ = reason;
    if (reason == ResultNormal) {
        dstEdb->setStorageParam("import_start", QString());
        int sec = importDuration();
        int min = sec / 60;
        sec = sec % 60;
        qWarning("%s", QString("Import is finished. Duration is %1 min. %2 sec.").arg(min).arg(sec).toUtf8().constData());
    }
    else if (reason == ResultCancel)
        qWarning("Import canceled");
    else
        qWarning("Import error");

    active = false;
    emit finished(reason);
}

int HistoryImport::importDuration()
{
    return startTime.secsTo(stopTime);
}

void HistoryImport::readFromFiles()
{
    if (!active)
        return;
    if (hWrite != NULL) {
        if (!hWrite->writeSuccess()) {
            stop(ResultError); // Write error
            return;
        }
    }
    else if (hErase != NULL && !hErase->writeSuccess()) {
        stop(ResultError);
        return;
    }
    if (importList.isEmpty()) {
        stop(ResultNormal);
        return;
    }
    if (hRead == NULL) {
        hRead = new EDBHandle(srcEdb);
        connect(hRead, SIGNAL(finished()), this, SLOT(writeToSqlite()));
    }

    const ImportItem &item = importList.first();
    int start = item.startNum;
    if (start == 0)
        qWarning("%s", QString("Importing %1").arg(JIDUtil::toString(item.jid, true)).toUtf8().constData());
    --recordsCount;
    if (dlg && (recordsCount % 100) == 0)
        progressBar->setValue(progressBar->value() + 1);
    hRead->get(item.accIds.first(), item.jid, QDateTime(), EDB::Forward, start, 1);
}

void HistoryImport::writeToSqlite()
{
    if (!active)
        return;
    const EDBResult r = hRead->result();
    if (hRead->lastRequestType() != EDBHandle::Read || r.size() > 1) {
        stop(ResultError);
        return;
    }
    if (r.isEmpty()) {
        importList.first().accIds.removeFirst();
        if (importList.first().accIds.isEmpty())
            importList.removeFirst();
        QTimer::singleShot(0, this, SLOT(readFromFiles()));
        return;
    }
    if (hWrite == NULL) {
        hWrite = new EDBHandle(dstEdb);
        connect(hWrite, SIGNAL(finished()), this, SLOT(readFromFiles()));
    }
    EDBItemPtr it = r.first();
    ImportItem &item = importList.first();
    hWrite->append(item.accIds.first(), item.jid, it->event(), EDB::Contact);
    item.startNum += 1;
}

void HistoryImport::showDialog()
{
    dlg = new QDialog();
    dlg->setModal(true);
    dlg->setWindowTitle(tr("Psi+ Import history"));
    QVBoxLayout *mainLayout = new QVBoxLayout(dlg);
    stackedWidget = new QStackedWidget(dlg);

    QWidget *page1 = new QWidget();
    QGridLayout *page1Layout = new QGridLayout(page1);
    QLabel *lbMessage = new QLabel(page1);
    lbMessage->setWordWrap(true);
    lbMessage->setText(tr("Found %1 files for import.\nContinue?").arg(importList.size()));
    page1Layout->addWidget(lbMessage, 0, 0, 1, 1);
    stackedWidget->addWidget(page1);

    QWidget *page2 = new QWidget();
    QHBoxLayout *page2Layout = new QHBoxLayout(page2);
    QGridLayout *page2GridLayout = new QGridLayout();
    page2GridLayout->addWidget(new QLabel(tr("Status:"), page2), 0, 0, 1, 1);
    lbStatus = new QLabel(page2);
    page2GridLayout->addWidget(lbStatus, 0, 1, 1, 1);
    page2GridLayout->addWidget(new QLabel(tr("Progress:"), page2), 1, 0, 1, 1);
    progressBar = new QProgressBar(page2);
    progressBar->setMaximum(1);
    progressBar->setValue(0);
    page2GridLayout->addWidget(progressBar, 1, 1, 1, 1);
    page2Layout->addLayout(page2GridLayout);
    stackedWidget->addWidget(page2);

    mainLayout->addWidget(stackedWidget);
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    QSpacerItem *buttonsSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    buttonsLayout->addItem(buttonsSpacer);
    btnOk = new QPushButton(dlg);
    connect(btnOk, SIGNAL(clicked()), this, SLOT(start()));
    btnOk->setText(tr("Ok"));
    buttonsLayout->addWidget(btnOk);
    QPushButton *btnCancel = new QPushButton(dlg);
    connect(btnCancel, SIGNAL(clicked()), this, SLOT(cancel()));
    btnCancel->setText(tr("Exit"));
    buttonsLayout->addWidget(btnCancel);
    mainLayout->addLayout(buttonsLayout);

    dlg->adjustSize();
    dlg->exec();
}

void HistoryImport::start()
{
    qWarning("Import start");
    startTime = QDateTime::currentDateTime();
    btnOk->setEnabled(false);
    stackedWidget->setCurrentIndex(1);

    lbStatus->setText(tr("Counting records"));
    qApp->processEvents();
    recordsCount = srcEdb->eventsCount(QString(), XMPP::Jid());
    int max = recordsCount / 100;
    if ((recordsCount % 100) != 0)
        ++max;
    progressBar->setMaximum(max);
    progressBar->setValue(0);

    lbStatus->setText(tr("Import"));
    hErase = new EDBHandle(dstEdb);
    connect(hErase, SIGNAL(finished()), this, SLOT(readFromFiles()));
    hErase->erase(QString(), QString());
    while (active)
        qApp->processEvents();
    if (result_ == ResultNormal)
        dlg->accept();
    else
        lbStatus->setText(tr("Error"));
}

void HistoryImport::cancel()
{
    stop();
    dlg->reject();
}
