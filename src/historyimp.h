/*
 * historyimp.h
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

#ifndef HISTORYIMP_H
#define HISTORYIMP_H

#include "eventdb.h"
#include "jidutil.h"
#include "psicon.h"
#include "xmpp/jid/jid.h"

#include <QDialog>
#include <QLabel>
#include <QObject>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>

struct ImportItem {
    QStringList accIds;
    XMPP::Jid   jid;
    int         startNum;
    ImportItem(const QStringList &ids, const XMPP::Jid &j)
    {
        accIds   = ids;
        jid      = j;
        startNum = 0;
    }
};

class HistoryImport : public QObject {
    Q_OBJECT

public:
    enum { ResultNone, ResultNormal, ResultCancel, ResultError };
    HistoryImport(PsiCon *psi);
    ~HistoryImport();
    bool isNeeded();
    int  exec();
    int  importDuration();

private:
    PsiCon *          psi_;
    QList<ImportItem> importList;
    EDB *             srcEdb;
    EDB *             dstEdb;
    EDBHandle *       hErase;
    EDBHandle *       hRead;
    EDBHandle *       hWrite;
    QDateTime         startTime;
    QDateTime         stopTime;
    bool              active;
    int               result_;
    quint64           recordsCount;
    QDialog *         dlg;
    QLabel *          lbStatus;
    QProgressBar *    progressBar;
    QStackedWidget *  stackedWidget;
    QPushButton *     btnOk;

private:
    void clear();
    void showDialog();

private slots:
    void readFromFiles();
    void writeToSqlite();
    void start();
    void stop(int reason = ResultCancel);
    void cancel();

signals:
    void finished(int);
};

#endif // HISTORYIMP_H
