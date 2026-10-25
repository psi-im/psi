/*
 * historyupgrade.cpp
 * Copyright (C) 2011  Aleksey Andreev
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

#include "historyupgrade.h"
#include "historyarchive.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QMessageBox>
#include <QMetaObject>
#include <QObject>
#include <QProgressDialog>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>

bool HistoryUpgrade::upgradeDatabase(const QString &path)
{
    // Preserve the history upgrade startup gate: refusing an upgrade stops startup,
    // and no account may use an incomplete schema. Never touch the GUI's SQL
    // connection from the worker.
    if (QMessageBox::question(
            nullptr, QCoreApplication::translate("HistoryImport", "Upgrade history"),
            QCoreApplication::translate("HistoryImport", "The history database needs to be upgraded. This can take some time for a large history. "
               "Psi will continue starting after the upgrade finishes. Continue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return false;
    class UpgradeProgress final : public QProgressDialog {
    public:
        UpgradeProgress() : QProgressDialog(QCoreApplication::translate("HistoryImport", "Upgrading history…"), QString(), 0, 0) { }
        void reject() override { } // A transaction already running must finish before startup can continue.
        void closeEvent(QCloseEvent *event) override { event->ignore(); }
    } progress;
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setCancelButton(nullptr);
    bool  success = false;
    auto *worker  = QThread::create([path, &success, &progress] {
        const QString connection = QStringLiteral("history-upgrade-worker");
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setDatabaseName(path);
            if (db.open()) {
                QSqlQuery foreignKeys(db);
                foreignKeys.exec("PRAGMA foreign_keys=ON");
                success = HistoryArchive::migrate(db, [&progress](qint64 id) {
                    QMetaObject::invokeMethod(
                        &progress,
                        [&progress, id] { progress.setLabelText(QCoreApplication::translate("HistoryImport", "Upgrading history (record %1)…").arg(id)); },
                        Qt::QueuedConnection);
                });
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connection);
    });
    QObject::connect(worker, &QThread::finished, &progress, &QDialog::accept);
    worker->start();
    progress.exec();
    worker->wait();
    delete worker;
    if (!success)
        QMessageBox::critical(nullptr, QCoreApplication::translate("HistoryImport", "Upgrade history"),
                              QCoreApplication::translate("HistoryImport", "The history upgrade failed. Changes were rolled back; Psi cannot continue."));
    return success;
}

