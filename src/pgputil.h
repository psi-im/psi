/*
 * psiaccount.cpp - handles a Psi account
 * Copyright (C) 2001-2005  Justin Karneges
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

#include "pgpkeydlg.h"
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>

class PGPUtil : public QObject {
    Q_OBJECT

public:
    static PGPUtil &instance();

    bool pgpAvailable();

    static void showDiagnosticText(const QString &event, const QString &diagnostic);

    QString stripHeaderFooter(const QString &);
    QString addHeaderFooter(const QString &, int);

    bool equals(const QString &, const QString &);

    static QString getKeyOwnerName(const QString &key);
    static QString getPublicKeyData(const QString &key);
    static QString getFingerprint(const QString &key);
    static QString chooseKey(PGPKeyDlg::Type type, const QString &key, const QString &title);

    struct SecureMessageSignature {
        enum {
            Valid = 0,          //< indentity is verified, matches signature
            InvalidSignature,   //< valid key provided, but signature failed
            InvalidKey,         //< invalid key provided
            NoKey               //< identity unknown
        };

        qint64 sigTimestamp = 0;
        int identityResult = -1;
        QString sigCreationDate;
        QString publicKeyId;
        QString userName;
    };
    static SecureMessageSignature parseSecureMessageSignature(const QString &stdOutString);

protected:
    PGPUtil();
    ~PGPUtil() = default;

private:
    static PGPUtil *m_instance;
};

