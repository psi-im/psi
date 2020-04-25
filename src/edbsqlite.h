/*
 * edbsqlite.h
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

#ifndef EDBSQLITE_H
#define EDBSQLITE_H

#include "edbflatfile.h"
#include "eventdb.h"
#include "psievent.h"
#include "xmpp_jid.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTimer>
#include <QVariant>

enum QueryType {
    QueryContactsList,
    QueryLatest,
    QueryOldest,
    QueryDateForward,
    QueryDateBackward,
    QueryFindText,
    QueryRowCount,
    QueryRowCountBefore,
    QueryJidRowId,
    QueryInsertEvent
};

struct QueryProperty {
    QueryType type;
    bool      allAccounts;
    bool      allContacts;
    QueryProperty(QueryType tp, bool allAcc, bool allCont)
    {
        type        = tp;
        allAccounts = allAcc;
        allContacts = allCont;
    }
    bool operator==(const QueryProperty &other) const
    {
        return (type == other.type && allAccounts == other.allAccounts && allContacts == other.allContacts);
    }
};
uint qHash(const QueryProperty &struc);

class EDBSqLite : public EDB {
    Q_OBJECT

    class QueryStorage;
    class PreparedQuery : private QSqlQuery {
    public:
        void bindValue(const QString &placeholder, const QVariant &val) { QSqlQuery::bindValue(placeholder, val); }
        bool exec() { return QSqlQuery::exec(); }
        bool first() { return QSqlQuery::first(); }
        bool next() { return QSqlQuery::next(); }
        QSqlRecord record() const { return QSqlQuery::record(); }
        void       freeResult() { QSqlQuery::finish(); }

    private:
        friend class QueryStorage;
        PreparedQuery(QSqlDatabase db);
        ~PreparedQuery() { }
    };
    //--------
    class QueryStorage {
    public:
        QueryStorage();
        ~QueryStorage();
        PreparedQuery *getPreparedQuery(QueryType type, bool allAccounts, bool allContacts);

    private:
        QString getQueryString(QueryType type, bool allAccounts, bool allContacts);

    private:
        QHash<QueryProperty, PreparedQuery *> queryList;
    };
    //--------

public:
    enum InsertMode { Normal, Import };

    EDBSqLite(PsiCon *psi);
    ~EDBSqLite();
    bool init();

    int features() const;
    int get(const QString &accId, const XMPP::Jid &jid, const QDateTime date, int direction, int start, int len);
    int find(const QString &accId, const QString &str, const XMPP::Jid &jid, const QDateTime date, int direction);
    int append(const QString &accId, const XMPP::Jid &jid, const PsiEvent::Ptr &e, int type);
    int erase(const QString &accId, const XMPP::Jid &jid);
    QList<ContactItem> contacts(const QString &accId, int type);
    quint64            eventsCount(const QString &accId, const XMPP::Jid &jid);
    QString            getStorageParam(const QString &key);
    void               setStorageParam(const QString &key, const QString &val);

    void         setInsertingMode(InsertMode mode);
    void         setMirror(EDBFlatFile *mirr);
    EDBFlatFile *mirror() const;

private:
    enum { NotActive, NotCommited, Commited };
    struct item_query_req {
        QString       accId;
        XMPP::Jid     j;
        int           jidType;
        int           type; // 0 = latest, 1 = oldest, 2 = random, 3 = write
        int           start;
        int           len;
        int           dir;
        int           id;
        QDateTime     date;
        QString       findStr;
        PsiEvent::Ptr event;

        enum Type { Type_get, Type_append, Type_find, Type_erase };
    };
    int                     status;
    unsigned int            transactionsCounter;
    QDateTime               lastCommitTime;
    unsigned int            maxUncommitedRecs;
    int                     maxUncommitedSecs;
    unsigned int            commitByTimeoutSecs;
    QTimer *                commitTimer;
    EDBFlatFile *           mirror_;
    QList<item_query_req *> rlist;
    QHash<QString, qint64>  jidsCache;
    QueryStorage            queryes;

private:
    bool          appendEvent(const QString &accId, const XMPP::Jid &, const PsiEvent::Ptr &, int);
    PsiEvent::Ptr getEvent(const QSqlRecord &record);
    qint64        ensureJidRowId(const QString &accId, const XMPP::Jid &jid, int type);
    int           rowCount(const QString &accId, const XMPP::Jid &jid, const QDateTime before);
    bool          eraseHistory(const QString &accId, const XMPP::Jid &);
    bool          transaction(bool now);
    bool          rollback();
    void          startAutocommitTimer();
    void          stopAutocommitTimer();
    bool          importExecute();

private slots:
    void performRequests();
    bool commit();
};

#endif // EDBSQLITE_H
