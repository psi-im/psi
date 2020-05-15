/*
 * gpgtransaction.h
 * Copyright (C) 2020  Boris Pek <tehnick-8@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
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

#pragma once

#include "gpgprocess.h"
#include "xmpp_jid.h"
#include "xmpp_message.h"
#include <QByteArray>
#include <QStringList>

class GpgTransaction : public GpgProcess {
    Q_OBJECT

public:
    enum class Type { Sign, Verify, Encrypt, Decrypt };

    explicit GpgTransaction(const Type type, const QString &keyID, QObject *parent = nullptr);
    ~GpgTransaction() = default;

    void start();
    int  id() const;

    void setGpgArguments(const QStringList &arguments);
    void setStdInString(const QString &str);
    void setOrigMessage(const XMPP::Message &msg);
    void setJid(const XMPP::Jid &jid);
    void setData(const QByteArray &data);

    QString       stdInString() const;
    QString       stdOutString() const;
    QString       stdErrString() const;
    XMPP::Message origMessage() const;
    XMPP::Jid     jid() const;
    QByteArray    data() const;

signals:
    void transactionFinished();

private slots:
    void processStarted();
    void processFinished();

private:
    static int m_idCounter;
    int        m_id;
    Type       m_type;

    QStringList   m_arguments;
    QString       m_stdInString;
    QString       m_stdOutString;
    QString       m_stdErrString;
    XMPP::Message m_origMessage;
    XMPP::Jid     m_jid;
    QByteArray    m_data;
    QString       m_tempFile;
};
